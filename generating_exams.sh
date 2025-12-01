#!/bin/bash
# Script to generate 20 exam files for SYSC4001 A3 Part 2

set -e

BASE_DIR="$(dirname "$0")"
EXAM_DIR="$BASE_DIR/data/exams"

# Ensure directory exists
mkdir -p "$EXAM_DIR"

echo "Generating exam files in $EXAM_DIR"

# Generate exam01.txt .. exam19.txt
for i in $(seq 1 19); do
    sid=$(printf "%04d" "$i")
    file="$EXAM_DIR/exam$(printf "%02d" "$i").txt"

    cat > "$file" <<EOF
${sid}
Exam ${sid} contents.
EOF

    echo "Created $file"
done

# Exam20 = sentinel
last="$EXAM_DIR/exam20.txt"
cat > "$last" <<EOF
9999
Exam 9999 – last exam, stops all TAs.
EOF
echo "Created $last (last exam)"

echo "Done generating exams."
