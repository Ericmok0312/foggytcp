import subprocess
import threading
from concurrent.futures import ThreadPoolExecutor
from time import sleep
import os

def run_server_thread(bandwidth, delay, filename, timeout=60):
    capture_start_cmd = ['sudo', './capture_packets.sh', 'start', 'server.pcap']
    server_cmd = ['vagrant', 'ssh', 'server', '-c', f'cd /vagrant/foggytcp && sudo tcset enp0s8 --rate {bandwidth} --delay {delay} --overwrite && bash ServerScript.sh {bandwidth} {delay} {filename}']
    
    # Start packet capture
    capture_start_process = subprocess.Popen(capture_start_cmd, cwd='/vagrant/foggytcp/utils', stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    server_process = subprocess.Popen(server_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    try:
        # Wait for the server process to complete
        server_output, server_error = server_process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        server_process.terminate()
        server_output, server_error = server_process.communicate()

    # Stop packet capture
    capture_stop_cmd = ['sudo', './capture_packets.sh', 'stop', 'server.pcap']
    capture_stop_process = subprocess.Popen(capture_stop_cmd, cwd='/vagrant/foggytcp/util', stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    capture_stop_process.communicate()

    # Analyze packet capture
    capture_analyze_cmd = ['./capture_packets.sh', 'analyze', 'server.pcap']
    capture_analyze_process = subprocess.Popen(capture_analyze_cmd, cwd='/vagrant/foggytcp/util', stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    capture_analyze_output, capture_analyze_error = capture_analyze_process.communicate()

    # Store results in result.txt
    with open('result.txt', 'a') as f:
        f.write(f"Server output: {server_output.decode().strip()}\n")
        f.write(f"Packet capture analysis: {capture_analyze_output.decode().strip()}\n")
        f.write('\n')

def run_client_thread(bandwidth, delay, filename, timeout=60):
    client_cmd = ['vagrant', 'ssh', 'client', '-c', f'cd /vagrant/foggytcp && sudo tcset enp0s8 --rate {bandwidth} --delay {delay} --overwrite && bash ClientScript.sh {bandwidth} {delay} {filename}']
    client_process = subprocess.Popen(client_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    try:
        output, error = client_process.communicate(timeout=timeout)
        print(f"Client output: {output.decode().strip()}")
    except subprocess.TimeoutExpired:
        print("Client process timed out.")
        client_process.terminate()

def start_threads(bandwidth, delay, filename1, filename2):
    server_thread = threading.Thread(target=run_server_thread, args=(bandwidth, delay, filename1))
    client_thread = threading.Thread(target=run_client_thread, args=(bandwidth, delay, filename2))

    server_thread.start()
    sleep(1)
    client_thread.start()

    server_thread.join()
    client_thread.join()

def create_threads():
    # Ensure the output directory exists
    output_dir = "/vagrant/foggytcp/outputs"

    parameters = [
        # Test 1
        ("10Mbps", "10ms", f"{output_dir}/server_output1", "/vagrant/foggytcp/testfile/file_1.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output2", "/vagrant/foggytcp/testfile/file_2.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output3", "/vagrant/foggytcp/testfile/file_3.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output4", "/vagrant/foggytcp/testfile/file_4.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output5", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output6", "/vagrant/foggytcp/testfile/file_6.txt"),
        # Test 2
        ("1Mbps", "10ms", f"{output_dir}/server_output7", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("5Mbps", "10ms", f"{output_dir}/server_output8", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output9", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("20Mbps", "10ms", f"{output_dir}/server_output10", "/vagrant/foggytcp/testfile/file_5.txt"),

        # Test 3
        ("10Mbps", "0ms", f"{output_dir}/server_output11", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("10Mbps", "5ms", f"{output_dir}/server_output12", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("10Mbps", "10ms", f"{output_dir}/server_output13", "/vagrant/foggytcp/testfile/file_5.txt"),
        ("10Mbps", "20ms", f"{output_dir}/server_output14", "/vagrant/foggytcp/testfile/file_5.txt"),
    ]

    for bandwidth, delay, filename1, filename2 in parameters:
        print(f"Running test with bandwidth={bandwidth}, delay={delay}, filename1={filename1}, filename2={filename2}")
        with ThreadPoolExecutor(max_workers=1) as executor:
            futures = []
            for i in range(1):  # Repeat 1 time for each set of parameters
                futures.append(executor.submit(start_threads, bandwidth, delay, filename1+"_"+str(i)+".out", filename2))
            for future in futures:
                future.result()

def main():
    create_threads()

main()