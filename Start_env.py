import subprocess
import time
import os
import sys

# Change the working directory to the location of this Python file
os.chdir(os.path.dirname(os.path.abspath(__file__)))


if len(sys.argv) > 1 and sys.argv[1].lower() == 'init':
    skip_vagrant_up = True
else:
    skip_vagrant_up = False

if not skip_vagrant_up:
    # Start Vagrant
    process = subprocess.Popen(["vagrant", "up"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    process.communicate()

# Open new command line windows and execute the commands
subprocess.Popen(["start", "cmd.exe", "/k", "vagrant ssh client"], shell=True)
subprocess.Popen(["start", "cmd.exe", "/k", "vagrant ssh server"], shell=True)