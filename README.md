# SYSC4001 - Assignment 3 Part 2


**Students:**  
Saim Hashmi (101241041)  
Abdullah Salman (101282570)

## Overview
This project implements a concurrent system where multiple Teaching Assistants (TAs) work together to mark exams using shared memory and process synchronization.

---
## Project Structure
.
├── part2a.c # Part 2.a - Concurrent version (race conditions allowed)
├── part2b.c # Part 2.b - Synchronized version (with semaphores)
├── generating_exams.sh # Script to generate exam files
├── data/
│ ├── rubric.txt # Rubric file with 5 exercises
│ └── exams/ # Directory containing exam files
│ ├── exam01.txt
│ ├── exam02.txt
│ └── ...
│ └── exam20.txt # Contains student 9999 - termination signal
├── Part A output/ # Logs from Part 2.a runs
├── Part B output/ # Logs from Part 2.b runs
├── reportPartC.pdf # Deadlock, Livelock, and Execution Order Report
└── Design_Discussion.pdf # Critical section design discussion
---

## Requirment
- GCC compiler
- Linux/Unix environment
- Standard C libraries

## Compilation Instructions

### Part 2.a (Concurrent - Race Conditions)
```bash
gcc -std=c99 -Wall -Wextra -O2 -o part2a part2a.c
```

### Part 2.b (Synchronized - Semaphores)
```bash
gcc -std=c99 -Wall -Wextra -O2 -o part2b part2b.c
```

##Generate Exam Files

```bash
chmod +x generating_exams.sh
./generating_exams.sh
```
##Execution Instructions
Part 2a
```bash
./part2a 2 data/rubric.txt data/exams
./part2a 3 data/rubric.txt data/exams
./part2a 4 data/rubric.txt data/exams
```
Part 2b
```bash
./part2b 2 data/rubric.txt data/exams
./part2b 3 data/rubric.txt data/exams
./part2b 4 data/rubric.txt data/exams
```
## Program Behavior

### Part 2.a Features
- Multiple TA processes run concurrently  
- Race conditions may occur  
- Multiple TAs can claim the same question simultaneously  
- Output shows interleaved execution  

### Part 2.b Features
- Semaphore synchronization eliminates race conditions  
- Only one TA can claim each question  
- Rubric modifications are protected  
- Clean, orderly progression through exams

### Expected Output Format
---
[00025][TA 1] Starting work on student 0001
[00026][TA 1] Inspect Q1 rubric ('A')
[00027][TA 2] Starting work on student 0001
[00028][TA 1] No change for Q1
[00029][TA 2] Inspect Q1 rubric ('A')
...
[00045][TA 1] Claiming Q1 for student 0001
[00050][TA 2] Claiming Q2 for student 0001
---
### File Requirements

**rubric.txt Format**
---
1, A
2, B
3, C
4, D
5, E
---

**Exam File Format**
---
0001
Exam content for student 0001
---
### Termination
- Programs automatically terminate when student ID `9999` is processed  
- All shared memory and semaphores are properly cleaned up  
