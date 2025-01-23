import csv

# Define the input and output file paths
input_file = 'result.txt'
output_file = 'output.csv'

# Define the column headers
headers = [
    'frame.time_relative', 'ip.src', 'cmutcp.source_port', 'ip.dst', 
    'cmutcp.destination_port', 'cmutcp.seq_num', 'cmutcp.ack_num', 
    'cmutcp.hlen', 'cmutcp.plen', 'cmutcp.flags', 
    'cmutcp.advertised_window', 'cmutcp.extension_length'
]

# Open the input file for reading
with open(input_file, 'r') as infile:
    # Open the output file for writing
    with open(output_file, 'w', newline='') as outfile:
        # Create a CSV writer object
        csv_writer = csv.writer(outfile)
        
        # Write the headers to the CSV file
        csv_writer.writerow(headers)
        
        # Read each line from the input file
        for line in infile:
            # Strip any leading/trailing whitespace and split the line by commas
            row = line.strip().split(',')
            
            # Write the row to the CSV file
            csv_writer.writerow(row)

print(f"Data has been successfully written to {output_file}")