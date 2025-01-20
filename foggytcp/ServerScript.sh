#!/bin/bash

# Check if the correct number of arguments are provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <bandwidth> <delay> <filename>"
    exit 1
fi

# Assign arguments to variables
BANDWIDTH=$1
DELAY=$2
FILENAME=$3

cd foggytcp
# Execute the server command and capture the output
OUTPUT=$(./server 10.0.1.1 3120 "$FILENAME")

# Get the last line of the output
LAST_LINE=$(echo "$OUTPUT" | tail -n 1)

# Return the last line to the Python caller
echo "$LAST_LINE"