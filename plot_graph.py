import numpy as np
from sklearn.cluster import DBSCAN
import matplotlib.pyplot as plt

# Read data from the text file
def read_data_from_file(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()
    transmission_times = []
    for line in lines:
        if "Transmission took" in line:
            time = int(line.split("Transmission took")[1].strip().split()[0])
            transmission_times.append(time)
    return transmission_times

# Process data into the three tests
def process_data(data):
    x = 10  # Number of repeated samples for the same test case

    test1 = data[:6 * x]  # Test 1: Filesizes with Bandwidth = 10Mbps, Delay = 10ms
    test2 = data[6 * x: 10 * x]  # Test 2: Bandwidths with Filesize = 1MB, Delay = 10ms
    test3 = data[10 * x:]  # Test 3: Delays with Bandwidth = 10Mbps, Filesize = 1MB

    def average(data):
        def remove_outliers(data_chunk):
            data_chunk = np.array(data_chunk).reshape(-1, 1)
            clustering = DBSCAN(eps=10, min_samples=2).fit(data_chunk)
            labels = clustering.labels_
            filtered_data = [data_chunk[i][0] for i in range(len(labels)) if labels[i] != -1]
            return filtered_data

        averaged_data = []
        for i in range(0, len(data), x):
            chunk = data[i:i + x]
            filtered_chunk = remove_outliers(chunk)
            averaged_data.append(np.mean(filtered_chunk))
        
        return averaged_data

    test1_avg = average(test1)
    test2_avg = average(test2)
    test3_avg = average(test3)

    return test1, test2, test3, test1_avg, test2_avg, test3_avg

# Visualize data
def visualize_data(test1, test2, test3, test1_avg, test2_avg, test3_avg):
    # Test 1: Plot
    plt.figure(figsize=(10, 6))
    filesizes = ["1KB", "5KB", "25KB", "100KB", "1MB", "10MB"]
    plt.plot(filesizes, test1_avg, marker='o', label="Test 1")
    plt.title("Test 1: Transmission Time vs. Filesize")
    plt.xlabel("Filesize")
    plt.ylabel("Transmission Time (ms)")
    plt.grid(True)
    plt.legend()
    plt.show()

    plt.figure(figsize=(10, 6))
    plt.boxplot([test1[i:i+10] for i in range(0, len(test1), 10)], labels=filesizes)
    plt.title("Test 1: Transmission Time vs. Filesize (Box Plot)")
    plt.xlabel("Filesize")
    plt.ylabel("Transmission Time (ms)")
    plt.grid(True)
    plt.show()

    # Test 2: Plot
    plt.figure(figsize=(10, 6))
    bandwidths = ["1Mbps", "5Mbps", "10Mbps", "20Mbps"]
    plt.plot(bandwidths, test2_avg, marker='o', color='orange', label="Test 2")
    plt.title("Test 2: Transmission Time vs. Bandwidth")
    plt.xlabel("Bandwidth")
    plt.ylabel("Transmission Time (ms)")
    plt.grid(True)
    plt.legend()
    plt.show()

    plt.figure(figsize=(10, 6))
    plt.boxplot([test2[i:i+10] for i in range(0, len(test2), 10)], labels=bandwidths)
    plt.title("Test 2: Transmission Time vs. Bandwidth (Box Plot)")
    plt.xlabel("Bandwidth")
    plt.ylabel("Transmission Time (ms)")
    plt.grid(True)
    plt.show()

    # Test 3: Plot
    plt.figure(figsize=(10, 6))
    delays = ["0ms", "5ms", "10ms", "20ms", "50ms", "100ms"]
    plt.plot(delays, test3_avg, marker='o', color='green', label="Test 3")
    plt.title("Test 3: Transmission Time vs. Delay")
    plt.xlabel("Delay")
    plt.ylabel("Transmission Time (ms)")
    plt.grid(True)
    plt.legend()
    plt.show()

    plt.figure(figsize=(10, 6))
    plt.boxplot([test3[i:i+10] for i in range(0, len(test3), 10)], labels=delays)
    plt.title("Test 3: Transmission Time vs. Delay (Box Plot)")
    plt.xlabel("Delay")
    plt.ylabel("Transmission Time (ms)")
    plt.grid(True)
    plt.show()

# Main function
def main():
    filename = "foggytcp/outputs/results.txt"  # Replace with your file name
    data = read_data_from_file(filename)
    test1, test2, test3, test1_avg, test2_avg, test3_avg = process_data(data)
    visualize_data(test1, test2, test3, test1_avg, test2_avg, test3_avg)

if __name__ == "__main__":
    main()