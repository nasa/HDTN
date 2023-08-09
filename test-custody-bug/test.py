#!/usr/bin/env python3
"""Test priority enforcement"""
import unittest
import os
from pathlib import Path
import shutil
import select
import socket
import scapy.contrib.bp as bp
from backgroundprocess import BackgroundProcess
from scapy.contrib.bp import BP
from adminrecord import AdminRecord, CustodySignal

def build_bundle(): 
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
        )

    payload = bp.BPBLOCK(
            Type=1,
            ProcFlags=(1 << 3),
            load='hello world')
    return bundle / payload

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
        recv_sock.bind(('127.0.0.1', 4002))

        # For getting custody signal
        recv_sig_sock = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
        recv_sig_sock.bind(('127.0.0.1', 4001))


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
        bundle = build_bundle()
        packet = bytes(bundle)
        s.sendto(packet, ("127.0.0.1", 4010))
        print(f"Sent bundle of length {len(packet)}")

        # Wait for response
        for _ in range(2):
            print("Waiting for response")
            has_data = select.select([recv_sock, recv_sig_sock], [], [], 10)
            self.assertGreater(len(has_data[0]), 0)
            rcv = has_data[0][0]

            recv_bundle, recv_addr = rcv.recvfrom(1024)
            print(recv_bundle.hex())
            print(f"got a response of {len(recv_bundle)} bytes from {recv_addr}")
            print(BP(recv_bundle))

        # Send a custody signal
        # TODO we need to make scapy protocols for Admin Records + Custody signals

        # Stop processes
        print("Cleanup and tests")
        hdtn.nice_kill()



if __name__ == "__main__":
    unittest.main()
