/* Saim Hashmi_101241041*/ 
/* Abdullah Salman_101282570*/

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>

#define NUM_Q 5
#define STUDENT_ID_LEN 5   /* 4 digits + NUL */
#define MAX_PATH 512

/* Shared memory layout */
typedef struct {
    char rubric[NUM_Q];            /* rubric letter for each question */
    int  qstate[NUM_Q];            /* 0 = not started, 1 = being marked, 2 = done */
    char student[STUDENT_ID_LEN];  /* current exam student ID string */
    int  current_exam_done;        /* parent/TA marks when exam appears done */
    int  terminate_flag;           /* set when student 9999 or error */
    int  rubric_changed;           /* TA sets when they modify rubric in SHM */
    unsigned int event_counter;    /* shared counter for readable logs */
} shm_area_t;


/* helper: sleep random milliseconds in [min_ms, max_ms] */
static void random_sleep_ms(int min_ms, int max_ms) {
    if (max_ms <= min_ms) {
        usleep(min_ms * 1000);
        return;
    }
    int r = (rand() % (max_ms - min_ms + 1)) + min_ms;
    usleep((useconds_t)r * 1000);
}

/* load rubric file (5 lines) into shared memory */
static int load_rubric_file(const char *path, shm_area_t *sh) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen rubric");
        return -1;
    }
    char line[256];
    int qnum;
    char letter;
    for (int i = 0; i < NUM_Q; ++i) {
        if (!fgets(line, sizeof(line), f)) {
            fprintf(stderr, "rubric: expected %d lines, found fewer\n", NUM_Q);
            fclose(f);
            return -1;
        }
        /* Accept formats like "1, A" or "1,A" */
        if (sscanf(line, "%d , %c", &qnum, &letter) != 2 &&
            sscanf(line, "%d,%c", &qnum, &letter) != 2 &&
            sscanf(line, "%d %c", &qnum, &letter) != 2) {
            fprintf(stderr, "rubric: bad line: %s", line);
            fclose(f);
            return -1;
        }
        if (qnum < 1 || qnum > NUM_Q) {
            fprintf(stderr, "rubric: invalid question number %d\n", qnum);
            fclose(f);
            return -1;
        }
        sh->rubric[qnum - 1] = letter;
    }
    fclose(f);
    return 0;
}

/* write rubric back to file from shared memory */
static int write_rubric_file(const char *path, shm_area_t *sh) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen rubric write");
        return -1;
    }
    for (int i = 0; i < NUM_Q; ++i) {
        fprintf(f, "%d, %c\n", i + 1, sh->rubric[i]);
    }
    fclose(f);
    return 0;
}

/* load exam with 1-based index  from exam_dir into shared memory */
static int load_exam_file(const char *exam_dir, int idx, shm_area_t *sh) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/exam%02d.txt", exam_dir, idx);
    FILE *f = fopen(path, "r");
    if (!f) {
        /* missing file -> treat as termination condition */
        fprintf(stderr, "[PARENT] cannot open %s: %s\n", path, strerror(errno));
        sh->terminate_flag = 1;
        return -1;
    }
    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "[PARENT] empty exam file %s\n", path);
        fclose(f);
        sh->terminate_flag = 1;
        return -1;
    }

    char sid[STUDENT_ID_LEN];
    memset(sid, 0, sizeof(sid));
    int copied = 0;
    for (int i = 0; i < 4 && line[i] != '\0' && line[i] != '\n'; ++i) {
        sid[copied++] = line[i];
    }
    sid[4] = '\0';
    if (copied == 0) {
        fprintf(stderr, "[PARENT] bad student id in %s\n", path);
        fclose(f);
        sh->terminate_flag = 1;
        return -1;
    }

    unsigned int ev = sh->event_counter++;
    printf("[%05u][PARENT] Loaded %s (student=%s)\n", ev, path, sid);
    strncpy(sh->student, sid, STUDENT_ID_LEN);

    for (int i = 0; i < NUM_Q; ++i) sh->qstate[i] = 0;
    sh->current_exam_done = 0;
    if (strncmp(sh->student, "9999", 4) == 0) {
        ev = sh->event_counter++;
        printf("[%05u][PARENT] Sentinel student 9999 reached -> terminating.\n", ev);
        sh->terminate_flag = 1;
    }
    fclose(f);
    return 0;
}

/* TA worker process code */
static void ta_worker(int ta_id, shm_area_t *sh) {
    /* seed random using pid/time to vary per process */
    srand((unsigned int)(time(NULL) ^ (getpid() << 8)));

    for (;;) {
        if (sh->terminate_flag) {
            unsigned int ev = sh->event_counter++;
            printf("[%05u][TA %d] terminate flag set; exiting\n", ev, ta_id);
            break;
        }

        unsigned int ev = sh->event_counter++;
        printf("[%05u][TA %d] Starting work on student %s\n", ev, ta_id, sh->student);

        /* 1) Inspect rubric (in shared memory) */
        for (int q = 0; q < NUM_Q; ++q) {
            ev = sh->event_counter++;
            char curr = sh->rubric[q];
            printf("[%05u][TA %d] Inspect Q%d rubric ('%c')\n", ev, ta_id, q + 1, curr);

            random_sleep_ms(500, 1000); /* 0.5-1.0s */

            /* random choice: 0 or 1 */
            int change = rand() & 1;
            if (change) {
                char old = sh->rubric[q];
                char newc = old + 1;
                if (newc > 'Z') newc = 'A';
                sh->rubric[q] = newc;
                sh->rubric_changed = 1;

                ev = sh->event_counter++;
                printf("[%05u][TA %d] Modified rubric Q%d: %c -> %c (in SHM)\n",
                       ev, ta_id, q + 1, old, newc);
            } else {
                ev = sh->event_counter++;
                printf("[%05u][TA %d] No change for Q%d\n", ev, ta_id, q + 1);
            }
            if (sh->terminate_flag) break;
        }

        if (sh->terminate_flag) {
            ev = sh->event_counter++;
            printf("[%05u][TA %d] terminate after rubric; exiting\n", ev, ta_id);
            break;
        }

        /* 2) Mark questions for this exam */
        for (;;) {
            if (sh->terminate_flag) {
                ev = sh->event_counter++;
                printf("[%05u][TA %d] terminate while marking; exiting\n", ev, ta_id);
                goto ta_exit;
            }

            int pick = -1;

            for (int i = 0; i < NUM_Q; ++i) {
                if (sh->qstate[i] != 2) { pick = i; break; }
            }
            if (pick == -1) {
                /* nothing left; mark exam done and wait for parent to load next */
                sh->current_exam_done = 1;
                ev = sh->event_counter++;
                printf("[%05u][TA %d] All questions observed done for student %s\n",
                       ev, ta_id, sh->student);
                break;
            }

            /* attempt to claim this question (no locking: races allowed) */
            if (sh->qstate[pick] == 2) continue; /* lost race; try again */
            sh->qstate[pick] = 1; /* pretend claiming */
            ev = sh->event_counter++;
            printf("[%05u][TA %d] Claiming Q%d for student %s (rubric='%c')\n",
                   ev, ta_id, pick + 1, sh->student, sh->rubric[pick]);

            /* marking time 1.0 - 2.0 s */
            random_sleep_ms(1000, 2000);

            /* finish marking */
            sh->qstate[pick] = 2;
            ev = sh->event_counter++;
            printf("[%05u][TA %d] Finished Q%d for student %s\n",
                   ev, ta_id, pick + 1, sh->student);
        }

        /* 3) busy-wait until parent loads the next exam (part a) */
        ev = sh->event_counter++;
        printf("[%05u][TA %d] Waiting for next exam to be loaded...\n", ev, ta_id);


        while (!sh->terminate_flag && sh->current_exam_done) {
            usleep(100 * 1000); 
        }
    }

ta_exit:
    return;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_TAs> <rubric_file> <exam_dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int nTAs = atoi(argv[1]);
    if (nTAs < 2) {
        fprintf(stderr, "num_TAs must be >= 2\n");
        return EXIT_FAILURE;
    }
    const char *rubric_path = argv[2];
    const char *exam_dir = argv[3];

    /* create shared memory */
    int shmid = shmget(IPC_PRIVATE, sizeof(shm_area_t), IPC_CREAT | 0600);
    if (shmid < 0) {
        perror("shmget");
        return EXIT_FAILURE;
    }
    shm_area_t *sh = (shm_area_t *)shmat(shmid, NULL, 0);
    if (sh == (shm_area_t *)-1) {
        perror("shmat parent");
        shmctl(shmid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    memset(sh, 0, sizeof(*sh));
    sh->terminate_flag = 0;
    sh->current_exam_done = 0;
    sh->rubric_changed = 0;
    sh->event_counter = 0;
    /* default rubric fill (will be replaced by load) */
    for (int i = 0; i < NUM_Q; ++i) sh->rubric[i] = 'A' + i;

    /* load rubric */
    if (load_rubric_file(rubric_path, sh) != 0) {
        fprintf(stderr, "Failed to load rubric from %s\n", rubric_path);
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    /* load first exam file */
    int exam_idx = 1;
    if (load_exam_file(exam_dir, exam_idx, sh) != 0) {
        fprintf(stderr, "Failed to load first exam (index %d)\n", exam_idx);
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    /* fork TA processes */
    for (int i = 0; i < nTAs; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* continue trying to fork remaining if possible */
        } else if (pid == 0) {
            /* child: attach to shm and run worker */
            shm_area_t *child_sh = (shm_area_t *)shmat(shmid, NULL, 0);
            if (child_sh == (shm_area_t *)-1) {
                perror("shmat child");
                exit(EXIT_FAILURE);
            }
            ta_worker(i + 1, child_sh);
            shmdt(child_sh);
            exit(EXIT_SUCCESS);
        }
        /* parent continues */
    }

    /* parent: monitor rubric changes and load subsequent exams */
    srand((unsigned int)time(NULL));
    while (!sh->terminate_flag) {
        /* if exam appears done, load next */
        if (sh->current_exam_done && !sh->terminate_flag) {
            exam_idx++;
            if (load_exam_file(exam_dir, exam_idx, sh) != 0) {
                /* load_exam_file sets terminate on error */
                break;
            }
            sh->current_exam_done = 0; /* reset - new exam loaded */
        }

        /* if rubric changed in shm, persist to file */
        if (sh->rubric_changed) {
            unsigned int ev = sh->event_counter++;
            printf("[%05u][PARENT] Detected rubric change in SHM; saving to file\n", ev);
            if (write_rubric_file(rubric_path, sh) != 0) {
                ev = sh->event_counter++;
                fprintf(stderr, "[%05u][PARENT] Failed to write rubric file\n", ev);
            } else {
                sh->rubric_changed = 0;
            }
        }

        usleep(200 * 1000); /* 200 ms polling */
    }

    /* termination: wait for children */
    unsigned int ev = sh->event_counter++;
    printf("[%05u][PARENT] Termination signaled; waiting for children...\n", ev);

    int status;
    while (wait(&status) > 0) {
        /* wait for all children */
    }

    ev = sh->event_counter++;
    printf("[%05u][PARENT] All children exited. Cleaning up shared memory.\n", ev);

    /* cleanup */
    shmdt(sh);
    shmctl(shmid, IPC_RMID, NULL);

    return EXIT_SUCCESS;
}

