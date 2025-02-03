#!/bin/bash

# Check if the correct number of arguments are provided


# Assign arguments to variables

FILENAME=test_output.txt


# Extract the base name and extension of the filename
BASENAME=$(basename "$FILENAME" | cut -d. -f1)
EXTENSION=$(basename "$FILENAME" | cut -d. -f2)

INDEX=1

while true; do
    # Construct the new filename with the incremented index
    NEW_FILENAME="./outputs/${BASENAME}_${INDEX}.${EXTENSION}"
    
    # Execute the server command and directly output the last line of the output
    timeout 300s ./server 10.0.1.1 3120 "$NEW_FILENAME" | tail -n 1
    
    # Increment the index
    INDEX=$((INDEX + 1))
    
done
