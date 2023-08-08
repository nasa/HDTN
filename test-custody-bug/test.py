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
import re
import select
import socket
import scapy.contrib.bp as bp


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
        self.sockets = []

        BackgroundProcess.output_dir = self.tmpdir

    def tearDown(self):
        for job in self.jobs:
            print(f"Killing {job.name}...")
            job.terminate()
            print(f"Killed {job.name}")
        for s in self.sockets:
            s.close()


    def process(self, *args, **kwargs):
        """Start background process (with jobtracking for teardown)"""
        proc = BackgroundProcess(*args, **kwargs)
        self.jobs.append(proc)
        return proc

    def socket(self, *args, **kwargs):
        """Create socket"""
        s = socket.socket(*args, **kwargs)
        self.sockets.append(s)
        return s

    def test_expedited_in_store(self):
        """Test storage full of expedited, receiving normal"""

        # Bind to recv addr
        recv_sock = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
        recv_sock.bind(('127.0.0.1', 4001))

        # Start HDTN
        print("Starting HDTN")
        cmd = (
            self.hdtn_exec,
            f'--contact-plan-file={self.configs / "contact_plan.json"}',
            f'--hdtn-config-file={self.configs / "hdtn.json"}',
        )
        hdtn = self.process(cmd, "hdtn")
        hdtn.wait_for_output('UdpBundleSink bound successfully on UDP port', 5)

        # Send a packet
        print("Sending")
        s = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
        bundle = bp.BP(
                version=6,              # Version
                ProcFlags=1<<4|1<<3,    # Bundle processing control flags
                DSO=2,                  # Destination scheme offset
                DSSO=3,                 # Destination SSP offset 
                SSO=1,                  # Source scheme offset
                SSSO=4,                 # Source SSP offset
                CT=5,                   # Creation time timestamp
                CTSN=6,                 # Creation timestamp sequence number
                CSO=1,
                CSSO=4,
                LT=1000,                # Life time
                DL=0                    # Dictionary length
            ) / bp.BPBLOCK(
                    Type=1,
                    ProcFlags=(1 << 3),
                    load='hello world')
        print(bundle)
                    
        packet = bytes(bundle)
        s.sendto(packet, ("127.0.0.1", 4000))
        print(f"Sent bundle of length {len(packet)}")

        # Wait for response
        print("Waiting for response")
        has_data = select.select([recv_sock], [], [], 10)
        self.assertGreater(len(has_data[0]), 0)

        recv_bundle, recv_addr = recv_sock.recvfrom(1024)
        print(recv_bundle.hex())
        print(f"got a response of {len(recv_bundle)} bytes from {recv_addr}")

        # Send a custody signal
        # TODO we need to make scapy protocols for Admin Records + Custody signals

        # Stop processes
        print("Cleanup and tests")
        hdtn.nice_kill()



if __name__ == "__main__":
    unittest.main()
