import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor
import os
import signal

class TestRunner:
    def __init__(self, params):
        self.params = params
        self.lock = threading.Lock()
        self.success_count = 0
        self.attempt_count = 0
        self.max_attempts = 20  # 最大嘗試次數防止無限循環
        self.timeout = 60    # 單次測試超時時間

    def run(self):
        while self.success_count < 5 and self.attempt_count < self.max_attempts:
            self.attempt_count += 1
            if self._execute_single_test():
                with self.lock:
                    self.success_count += 1
        return self.success_count >= 5

    def _execute_single_test(self):
        # 生成唯一文件名
        test_id = f"{self.params['name']}_{self.attempt_count}"
        server_output = f"{self.params['server_output']}_{test_id}.out"
        client_file = self.params['client_file']
        client_output = f"{self.params['server_output']}_{test_id}_client.out"

        print(f"Test case: Bandwidth: {self.params['bw']}, delay {self.params['delay']}, file {client_file}, attempt {self.attempt_count}")

        # 初始化進程引用
        processes = {
            'server': None,
            'client': None
        }

        try:
            # 配置並啟動伺服器
            server_cmd = [
                'vagrant', 'ssh', 'server', '-c',
                f'cd /vagrant/foggytcp && '
                f'bash ServerScript.sh {self.params["bw"]} {self.params["delay"]} {server_output}'
            ]
            processes['server'] = subprocess.Popen(
                server_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )

            # 等待伺服器初始化
            time.sleep(1)
            print("Done init Server")

            # 配置並啟動客戶端
            client_cmd = [
                'vagrant', 'ssh', 'client', '-c',
                f'cd /vagrant/foggytcp && '
                f'sudo tcset enp0s8 --rate {self.params["bw"]} --delay {self.params["delay"]} --overwrite && '
                f'bash ClientScript.sh {self.params["bw"]} {self.params["delay"]} {client_file}'
            ]
            processes['client'] = subprocess.Popen(
                client_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            print("Done init Client")

            # 等待進程完成或超時
            start_time = time.time()
            while True:
                # 檢查超時
                print("Waiting")
                if time.time() - start_time > self.timeout:
                    self._cleanup_processes(processes, test_id)
                    return False

                # 檢查進程狀態
                client_done = processes['client'].poll() is not None
                server_done = processes['server'].poll() is not None

                # 兩個進程都正常完成
                if client_done and server_done:
                    if processes['client'].returncode == 0 and processes['server'].returncode == 0:
                        # 記錄成功日誌
                        self._log_result(test_id, success=True)
                        self._save_client_output(processes['client'], client_output)
                        return True
                    break  # 任一進程失敗

                # 任一進程異常終止
                if (client_done and processes['client'].returncode != 0) or \
                   (server_done and processes['server'].returncode != 0):
                    break

                time.sleep(1)

            # 清理異常狀態
            self._cleanup_processes(processes, test_id)
            return False

        except Exception as e:
            print(f"Error in test {test_id}: {str(e)}")
            self._cleanup_processes(processes, test_id)
            return False

    def _cleanup_processes(self, processes, test_id):
        # 終止本地進程
        for role in ['server', 'client']:
            if processes[role] and processes[role].poll() is None:
                processes[role].terminate()
                try:
                    processes[role].wait(timeout=5)
                except subprocess.TimeoutExpired:
                    processes[role].kill()

        # 清理虛擬機內可能殘留的進程
        self._clean_vm_processes('server', 'ServerScript.sh')
        self._clean_vm_processes('client', 'ClientScript.sh')

    def _clean_vm_processes(self, vm, script_name):
        subprocess.run(
            ['vagrant', 'ssh', vm, '-c', f'pkill -f {script_name}'],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

    def _log_result(self, test_id, success):
        log_entry = {
            'timestamp': time.strftime("%Y-%m-%d %H:%M:%S"),
            'test_id': test_id,
            'params': self.params,
            'success': success,
            'attempt': self.attempt_count
        }
        with open('test_results.log', 'a') as f:
            f.write(str(log_entry) + '\n')

    def _save_client_output(self, client_process, client_output):
        stdout, stderr = client_process.communicate()
        with open("result.txt", 'ab') as f:
            f.write(stdout)

def main():
    test_parameters = [
        # Test Series 1: 不同文件傳輸測試
        {
            'name': 'test1_file1',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server1',
            'client_file': '/vagrant/foggytcp/testfile/file_1.txt'
        },
        {
            'name': 'test1_file2',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server2',
            'client_file': '/vagrant/foggytcp/testfile/file_2.txt'
        },
        {
            'name': 'test1_file3',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server3',
            'client_file': '/vagrant/foggytcp/testfile/file_3.txt'
        },
        {
            'name': 'test1_file4',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server4',
            'client_file': '/vagrant/foggytcp/testfile/file_4.txt'
        },
        {
            'name': 'test1_file5',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server5',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test1_file6',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server6',
            'client_file': '/vagrant/foggytcp/testfile/file_6.txt'
        },

        # Test Series 2: 不同帶寬測試
        {
            'name': 'test2_1Mbps',
            'bw': '1Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server7',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test2_5Mbps',
            'bw': '5Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server8',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test2_10Mbps',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server9',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test2_20Mbps',
            'bw': '20Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server10',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },

        # Test Series 3: 不同延遲測試
        {
            'name': 'test3_0ms',
            'bw': '10Mbps',
            'delay': '0ms',
            'server_output': '/vagrant/foggytcp/outputs/server11',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test3_5ms',
            'bw': '10Mbps',
            'delay': '5ms',
            'server_output': '/vagrant/foggytcp/outputs/server12',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test3_10ms',
            'bw': '10Mbps',
            'delay': '10ms',
            'server_output': '/vagrant/foggytcp/outputs/server13',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        },
        {
            'name': 'test3_20ms',
            'bw': '10Mbps',
            'delay': '20ms',
            'server_output': '/vagrant/foggytcp/outputs/server14',
            'client_file': '/vagrant/foggytcp/testfile/file_5.txt'
        }
    ]

    with ThreadPoolExecutor(max_workers=1) as executor:
        futures = []
        for params in test_parameters:
            runner = TestRunner(params)
            futures.append(executor.submit(runner.run))

        for future in futures:
            if future.result():
                print(f"Test {params['name']} completed successfully")
            else:
                print(f"Test {params['name']} failed to complete 5 successful runs")


# 確保輸出目錄存在
os.makedirs('/vagrant/foggytcp/outputs', exist_ok=True)
main()