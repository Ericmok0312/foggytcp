#!/bin/bash

# Change to the directory where the client is located
cd /vagrant/foggytcp || { echo "Failed to change directory to /vagrant/foggytcp"; exit 1; }

parameters=(
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_1.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_2.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_3.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_4.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_6.txt"
    "1Mbps,10ms,/vagrant/foggytcp/testfile/file_5.txt"
    "5Mbps,10ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_5.txt"
    "20Mbps,10ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,0ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,5ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,10ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,20ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,50ms,/vagrant/foggytcp/testfile/file_5.txt"
    "10Mbps,100ms,/vagrant/foggytcp/testfile/file_5.txt"
)

for param in "${parameters[@]}"; do
    # Split parameters using IFS
    IFS=',' read -r BANDWIDTH DELAY FILENAME <<< "$param"
    echo "Current parameters: BANDWIDTH=$BANDWIDTH, DELAY=$DELAY, FILENAME=$FILENAME"

    for i in {1..10}; do
        # Clear existing traffic control settings
        sudo tcdel enp0s8 --all >/dev/null 2>&1  # Suppress output/stderr
        
        # Apply new network settings
        sudo tcset enp0s8 --rate "$BANDWIDTH" --delay "$DELAY" --overwrite || { echo "tcset failed for $param"; exit 1; }
        
        while true; do
            # Execute client command with timeout and capture output
            OUTPUT=$(timeout 300s ./client 10.0.1.1 3120 "$FILENAME")
            STATUS=$?
            
            if [ $STATUS -eq 0 ]; then
                # Generate output filename and save results
                OUTPUT_FILE="/vagrant/foggytcp/outputs/results.txt"
                echo "$OUTPUT" >> "$OUTPUT_FILE"
                
                # Extract and display the last line of output
                LAST_LINE=$(echo "$OUTPUT" | tail -n 1)
                echo "$LAST_LINE"
                break
            else
                echo "timeout retrying"
            fi
        done
        sleep 1
    done
done
