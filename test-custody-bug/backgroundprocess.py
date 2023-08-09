from pathlib import Path
from subprocess import Popen, TimeoutExpired, PIPE
from threading import Thread
from queue import Queue, Empty
import signal
import time
import re

class TimeoutException(Exception):
    """Timeout while waiting for something to happen"""

class BackgroundProcess:
    """Run process in background, capturing stdout and stderr"""

    output_dir: Path

    def __init__(self, cmd, name=None):
        """Create BackgroundProcess

        cmd is a tuple with the command, ex ('ls', '-l')
        name is an optional nice-name for log files"""
        self.proc = Popen(
            cmd, stdout=PIPE, stderr=PIPE, cwd=BackgroundProcess.output_dir
        )

        if name:
            self.name = name
        else:
            self.name = str(self.proc.pid)

        self.stdout_queue = Queue()
        self.stdout_thread = Thread(target=self._enqueue_stdout)
        self.stdout_thread.start()

        self.stderr_queue = Queue()
        self.stderr_thread = Thread(target=self._enqueue_stderr)
        self.stderr_thread.start()

    def _enqueue_stdout(self):
        # From https://stackoverflow.com/a/4896288
        logfile = BackgroundProcess.output_dir / f"{self.name}.stdout"
        with open(logfile, "w", buffering=1) as output:
            if self.proc.stdout is None:
                return
            while True:
                line = self.proc.stdout.readline().decode("ascii", errors="ignore")
                if line is None or line == "":
                    break
                self.stdout_queue.put(line)
                output.write(line)
                # print(line.decode('ascii', errors='ignore').strip('\n'))
            self.proc.stdout.close()

    def _enqueue_stderr(self):
        # From https://stackoverflow.com/a/4896288
        logfile = BackgroundProcess.output_dir / f"{self.name}.stderr"
        with open(logfile, "w", buffering=1) as output:
            if self.proc.stderr is None:
                return
            while True:
                line = self.proc.stderr.readline().decode("ascii", errors="ignore")
                if line is None or line == "":
                    break
                self.stderr_queue.put(line)
                output.write(line)
                # print(line.decode('ascii', errors='ignore').strip('\n'))
            self.proc.stderr.close()

    def nice_kill(self, timeout: int = 2):
        """Try to interrupt process, then kill if timeout reached"""
        self.proc.send_signal(signal.SIGINT)
        try:
            self.proc.wait(timeout)
        except TimeoutExpired:
            pass
        self.proc.kill()

    def terminate(self):
        """Terminate process"""
        return self.proc.terminate()

    def wait_for_output(self, output, timeout=0):
        """Wait for output to appear in stdout"""
        start_time = time.time()
        while True:
            if timeout != 0 and time.time() - start_time > timeout:
                raise TimeoutException("Timeout reached")
            try:
                line = self.stdout_queue.get(timeout=0.2)
            except Empty:
                continue
            else:
                match = re.search(output, line)
                if match:
                    #elapsed = time.time() - start_time
                    #print(f"{self.name}: {elapsed}s : found {output}")
                    return


