#!/usr/bin/env python3
"""Test priority enforcement"""
import unittest
import os
from pathlib import Path
from subprocess import Popen, TimeoutExpired, PIPE
from threading import Thread
from queue import Queue, Empty
import signal
import random
import hashlib
import shutil
import time
from typing import Dict
import re
import csv


class TimeoutException(Exception):
    """Timeout while waiting for something to happen"""


def wait_for_files(files: Dict[Path, int], timeout=10):
    """Wait for files of specified length to exist"""
    missing_files = dict(files)
    start_time = time.time()
    while True:

        def file_missing(item):
            file, size = item
            if not file.exists():
                return True
            return file.stat().st_size < size

        missing_files = dict(filter(file_missing, missing_files.items()))

        if not missing_files:
            elapsed = time.time() - start_time
            print(f"all files present after {elapsed} seconds")
            return
        time.sleep(0.2)
        if timeout != 0 and time.time() - start_time > timeout:
            msg = " ".join(str(m) for m in missing_files)
            raise TimeoutException(f"Timeout while waiting for files: {msg}")


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
                    elapsed = time.time() - start_time
                    print(f"{self.name}: {elapsed}s : found {output}")
                    return


def write_random_data(path, size):
    """Writes a file with random data, returns hash"""
    data = random.randbytes(size)
    data_hash = hashlib.md5(data).hexdigest()
    with open(path, "wb") as f:
        f.write(data)
    return data_hash


def get_file_hash(path):
    """Get hash of file"""
    with open(path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


class TestPriority(unittest.TestCase):
    """Test case for priority enforcement"""

    def setUp(self):
        root = os.getenv("HDTN_SOURCE_ROOT")
        if root is None:
            raise RuntimeError("HDTN_SOURCE_ROOT must bet set in environment")
        root = Path(root)
        apps = root / "build" / "common" / "bpcodec" / "apps"

        self.bpsendfile_exec = apps / "bpsendfile"
        self.bpreceivefile_exec = apps / "bpreceivefile"
        self.hdtn_exec = (
            root / "build" / "module" / "hdtn_one_process" / "hdtn-one-process"
        )

        self.configs = Path(__file__).parent.resolve() / "configs"

        # Logs written here
        self.tmpdir = Path(__file__).parent.resolve() / "tmp"
        if self.tmpdir.exists():
            shutil.rmtree(self.tmpdir)
        self.tmpdir.mkdir()
        self.jobs = []

        BackgroundProcess.output_dir = self.tmpdir

    def tearDown(self):
        for job in self.jobs:
            print(f"Killing {job.name}...")
            job.terminate()
            print(f"Killed {job.name}")

    def process(self, *args, **kwargs):
        """Start background process (with jobtracking for teardown)"""
        proc = BackgroundProcess(*args, **kwargs)
        self.jobs.append(proc)
        return proc

    def check_priority(self):
        """Assert that priorities in stats file are ordered"""
        stat_files = list((self.tmpdir / "stats" / "bundle_stats").glob("*.csv"))
        self.assertEqual(len(stat_files), 1, msg="No stats file, requires DO_STATS_LOGGING")
        stat_file = stat_files[0]
        with open(stat_file, "r") as f:
            reader = csv.DictReader(f)
            prev_priority = None
            for row in reader:
                priority = int(row["priority"])
                if prev_priority is not None:
                    self.assertLessEqual(priority, prev_priority)
                prev_priority = priority

    def test_expedited_in_store(self):
        """Test storage full of expedited, receiving normal"""
        file_length = pow(1024, 2)
        # Start HDTN
        print("Starting HDTN")
        cmd = (
            self.hdtn_exec,
            f'--contact-plan-file={self.configs / "contact_plan.json"}',
            f'--hdtn-config-file={self.configs / "hdtn.json"}',
        )
        hdtn = self.process(cmd, "hdtn")

        time.sleep(1)

        # Send high priority data
        print("Starting high priority send")
        data_dir = self.tmpdir / "1"
        data_dir.mkdir()
        expedited_hash = write_random_data(data_dir / "testfile1.bin", file_length)
        cmd = (
            self.bpsendfile_exec,
            "--max-bundle-size-bytes=1000",
            f"--file-or-folder-path={data_dir}",
            "--recurse-directories-depth=2",
            "--my-uri-eid=ipn:1.1",
            "--dest-uri-eid=ipn:2.1",
            "--bundle-priority=2",
            "--bundle-lifetime-milliseconds=30000",
            f'--outducts-config-file={self.configs / "expedited.json"}',
            "--cla-rate=560000",
        )
        bpsendfile_expedited = self.process(cmd, "bpsendfile-expedited")

        # Wait for high priority data to be sent
        print("Waiting for high priority data")
        bpsendfile_expedited.wait_for_output("all bundles generated and fully sent", 30)
        bpsendfile_expedited.wait_for_output(
            " BpSourcePatternThreadFunc thread exiting", 10
        )
        time.sleep(2)  # Give it time to clean up
        bpsendfile_expedited.nice_kill(2)

        # Send normal priority data with NO DELAY between starting receiver
        print("Starting normal priority send")
        data_dir = self.tmpdir / "2"
        data_dir.mkdir()
        normal_hash = write_random_data(data_dir / "testfile2.bin", file_length)
        cmd = (
            self.bpsendfile_exec,
            "--max-bundle-size-bytes=1000",
            f"--file-or-folder-path={data_dir}",
            "--recurse-directories-depth=2",
            "--my-uri-eid=ipn:3.1",
            "--dest-uri-eid=ipn:2.1",
            "--bundle-priority=1",
            "--bundle-lifetime-milliseconds=30000",
            f'--outducts-config-file={self.configs / "normal.json"}',
            "--cla-rate=560000",
        )
        bpsendfile_normal = self.process(cmd, "bpsendfile-normal")

        # Start receiver
        print("Starting bpreceive")
        receive_dir = self.tmpdir / "received"
        cmd = (
            self.bpreceivefile_exec,
            f"--save-directory={receive_dir}",
            "--my-uri-eid=ipn:2.1",
            f'--inducts-config-file={self.configs / "receive.json"}',
        )
        bpreceivefile = self.process(cmd, "bpreceivefile")

        file1 = receive_dir / "testfile1.bin"
        file2 = receive_dir / "testfile2.bin"

        # Wait for normal priority data to be sent
        print("Waiting for normal priority data")
        bpsendfile_normal.wait_for_output("all bundles generated and fully sent", 30)
        bpsendfile_normal.wait_for_output(
            " BpSourcePatternThreadFunc thread exiting", 10
        )
        time.sleep(2)  # Give it time to clean up
        bpsendfile_normal.nice_kill(2)

        # Wait for files
        print("Waiting for files")
        bpreceivefile.wait_for_output("closed", 120)
        bpreceivefile.wait_for_output("closed", 120)
        wait_for_files({file1: file_length, file2: file_length}, 60)
        time.sleep(1)  # wait for data to finish transfer

        # Stop processes
        print("Cleanup and tests")
        bpsendfile_normal.nice_kill(2)
        hdtn.nice_kill()
        bpreceivefile.nice_kill()

        # Check results
        self.assertTrue(file1.exists())
        self.assertTrue(file2.exists())
        self.assertEqual(get_file_hash(file1), expedited_hash)
        self.assertEqual(get_file_hash(file2), normal_hash)

        self.check_priority()


if __name__ == "__main__":
    unittest.main()
