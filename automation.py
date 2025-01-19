import subprocess
import threading
from concurrent.futures import ThreadPoolExecutor
from time import sleep

def run_server_thread(bandwidth, delay, filename, stop_event, timeout=60):
    server_cmd = ['vagrant', 'ssh' ,'server', '-c', f'cd /vagrant/foggytcp && sudo tcset enp0s8 --rate {bandwidth} --delay {delay} --overwrite && bash ServerScript.sh {bandwidth} {delay} {filename}']
    server_process = subprocess.Popen(server_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    try:
        # Log server output and errors
        stop_event.wait()
        server_process.terminate()
    finally:
        server_process.terminate()

def run_client_thread(bandwidth, delay, filename, stop_event, timeout=30):
    client_cmd = ['vagrant', 'ssh','client', '-c', f'cd /vagrant/foggytcp && bash ClientScript.sh {bandwidth} {delay} {filename}']
    client_process = subprocess.Popen(client_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    try:
        output, error = client_process.communicate()

        print(f"Client output: {output.decode().strip()}")

        output_str = output.decode().strip()
        if output_str:
            last_line = output_str.split('\n')[-1]
            results.append(last_line)
        else:
            print("Client output is empty.")
            results.append("-1")

        # Notify server thread to kill the server script
        stop_event.set()
        client_process.terminate()
    except subprocess.TimeoutExpired:
        print("Client process timed out.")
        results.append("-1")
        stop_event.set()
    finally:
        stop_event.set()
        client_process.terminate()

def start_threads(bandwidth, delay, filename1, filename2):
    stop_event = threading.Event()
    server_thread = threading.Thread(target=run_server_thread, args=(bandwidth, delay, filename1, stop_event))
    client_thread = threading.Thread(target=run_client_thread, args=(bandwidth, delay, filename2, stop_event))

    server_thread.start()
    sleep(10)
    client_thread.start()

    server_thread.join()
    client_thread.join()

def create_threads():
    parameters = [
        # Test 1
        ("10Mbps", "1ms", "/vagrant/foggytcp/server_output1", "/vagrant/foggytcp/testfile/file_1.txt"),
        ("10Mbps", "1ms", "/vagrant/foggytcp/server_output2", "/vagrant/foggytcp/testfile/file_2.txt"),
        ("10Mbps", "1ms", "/vagrant/foggytcp/server_output3", "/vagrant/foggytcp/testfile/file_3.txt"),
        ("10Mbps", "1ms", "/vagrant/foggytcp/server_output4", "/vagrant/foggytcp/testfile/file_4.txt"),
        ("10Mbps", "1ms", "/vagrant/foggytcp/server_output5", "/vagrant/foggytcp/testfile/file_5.txt"),

        # # Test 2
        # ("1Mbps", "10ms", "/vagrant/foggytcp/server_output7", "/vagrant/foggytcp/testfile/file_5.txt"),
        # ("5Mbps", "10ms", "/vagrant/foggytcp/server_output8", "/vagrant/foggytcp/testfile/file_5.txt"),
        # ("10Mbps", "10ms", "/vagrant/foggytcp/server_output9", "/vagrant/foggytcp/testfile/file_5.txt"),
        # ("20Mbps", "10ms", "/vagrant/foggytcp/server_output10", "/vagrant/foggytcp/testfile/file_5.txt"),

        # # Test 3
        # ("10Mbps", "0ms", "/vagrant/foggytcp/server_output11", "/vagrant/foggytcp/testfile/file_5.txt"),
        # ("10Mbps", "5ms", "/vagrant/foggytcp/server_output12", "/vagrant/foggytcp/testfile/file_5.txt"),
        # ("10Mbps", "10ms", "/vagrant/foggytcp/server_output13", "/vagrant/foggytcp/testfile/file_5.txt"),
        # ("10Mbps", "20ms", "/vagrant/foggytcp/server_output14", "/vagrant/foggytcp/testfile/file_5.txt"),
    ]

    for bandwidth, delay, filename1, filename2 in parameters:
        print(f"Running test with bandwidth={bandwidth}, delay={delay}, filename1={filename1}, filename2={filename2}")
        with ThreadPoolExecutor(max_workers=1) as executor:
            futures = []
            for i in range(1):  # Repeat 5 times for each set of parameters
                futures.append(executor.submit(start_threads, bandwidth, delay, filename1+"_"+str(i)+".out", filename2))
            for future in futures:
                future.result()
            with open('result.txt', 'w') as f:
                for result in results:
                    f.write(result + '\n')
                f.write('\n')

def main():
    global results
    results = []
    create_threads()

main()