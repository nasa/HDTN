#!/usr/bin/env python3
"""Test custody bug"""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import select
import shutil
import socket
import time
import unittest
import zmq
from adminrecord import AdminRecord, CustodySignal, BpBlock
from backgroundprocess import BackgroundProcess
from scapy.all import raw, Raw
from scapy.contrib.bp import BP


@contextmanager
def l(*args, end=" "):
    """Print before/after doing something like 'start ... done'"""
    print(*args, "...", end=end, flush=True)
    try:
        yield
        print("done")
    finally:
        pass


def do_api_command(cmd):
    """Send HDTN an API Command"""
    with zmq.Context() as c:
        with c.socket(zmq.REQ) as s:
            s.connect("tcp://127.0.0.1:10305")
            s.send_json(cmd)
            if s.poll(2000, zmq.POLLIN):
                s.recv()
            s.close()


def clear_contacts():
    """Send HDTN an empty contact plan"""
    p = {"contacts": []}
    cmd = {"apiCall": "upload_contact_plan", "contactPlanJson": json.dumps(p)}
    do_api_command(cmd)


def get_expiring():
    """Get expiring bundles of priority zero"""
    cmd = {
        "apiCall": "get_expiring_storage",
        "priority": 0,
        "thresholdSecondsFromNow": 10000000,
    }
    do_api_command(cmd)


def build_bundle():
    """Create bundle to send to HDTN"""
    bundle = BP(
        version=6,  # Version
        ProcFlags=1 << 4 | 1 << 3,  # Bundle processing control flags
        DSO=2,  # Destination scheme offset
        DSSO=3,  # Destination SSP offset
        SSO=1,  # Source scheme offset
        SSSO=4,  # Source SSP offset
        CT=5,  # Creation time timestamp
        CTSN=6,  # Creation timestamp sequence number
        CSO=1,
        CSSO=4,
        LT=1000,  # Life time
        DL=0,  # Dictionary length
    )

    payload = BpBlock(Type=1, flags=(1 << 3)) / Raw(b"hello world")
    return bundle / payload


def build_custody_signal(custody_bundle):
    """Build custody signal to send to HDTN"""
    bp = custody_bundle["BP"]
    sig = (
        BP(
            version=6,
            ProcFlags=1 << 4 | 1 << 1,
            DSO=bp.CSO,
            DSSO=bp.CSSO,
            SSO=2,
            SSSO=3,
            CT=12,
            CTSN=13,
            LT=1000,
            DL=0,
        )
        / BpBlock(Type=1, flags=(1 << 3))
        / AdminRecord(Type=2, flags=0)
        / CustodySignal(
            succeeded=1,
            reason=0,
            sig_time_sec=0xCAFE,
            sig_time_nano=0xBEEF,
            creat_time=bp.CT,
            creat_seq=bp.CTSN,
            src_eid=f"ipn:{bp.SSO}.{bp.SSSO}",
        )
    )
    return sig


class TestCustodyBug(unittest.TestCase):
    """Regression test for custody transfer bug"""

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
        BackgroundProcess.enable_core_dumps()

    def tearDown(self):
        for job in self.jobs:
            with l(f"Killing {job.name}"):
                job.terminate()
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

        # Final destination for bundle
        node_two_sock = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
        node_two_sock.bind(("127.0.0.1", 4002))

        # For receiving custody signal from HDTN
        node_one_sock = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
        node_one_sock.bind(("127.0.0.1", 4001))

        with l("Starting HDTN"):
            cmd = (
                self.hdtn_exec,
                f'--contact-plan-file={self.configs / "contact_plan.json"}',
                f'--hdtn-config-file={self.configs / "hdtn.json"}',
            )
            hdtn = self.process(cmd, "hdtn")
            hdtn.wait_for_output("UdpBundleSink bound successfully on UDP port", 5)

        with l("Sending bundle to HDTN"):
            s = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
            bundle = build_bundle()
            packet = raw(bundle)
            s.sendto(packet, ("127.0.0.1", 4010))

        custody_bundle = None
        with l("Waiting for bundles from HDTN...", end="\n"):
            for _ in range(2):
                has_data = select.select([node_one_sock, node_two_sock], [], [], 10)
                self.assertGreater(len(has_data[0]), 0)
                rcv = has_data[0][0]
                recv_bundle, recv_addr = rcv.recvfrom(1024)
                b = BP(recv_bundle)
                if rcv == node_two_sock:
                    custody_bundle = b
                    payload = raw(b["Raw"]).decode("ascii", errors="backslashreplace")
                    print(
                        f"  Node two got bundle from {recv_addr}:\n    {b} : {payload}"
                    )
                elif rcv == node_one_sock:
                    print(f"  Node one got custody signal from {recv_addr}:\n    {b}")

        self.assertIsNotNone(custody_bundle)

        with l("Clearing contacts"):
            clear_contacts()

        with l("Waiting for custody transfer timer to expire"):
            time.sleep(5)  # In config, set this to 2 seconds

        with l("Sending custody bundle"):
            custody_signal = build_custody_signal(custody_bundle)
            s.sendto(raw(custody_signal), ("127.0.0.1", 4010))

        with l("Call get_expiring_storage API to cause crash"):
            get_expiring()

        with l("Testing that HDTN has not crashed"):
            time.sleep(1)  # This sleep probably not needed
            # poll returns None if the process is still running
            self.assertIsNone(hdtn.proc.poll())


if __name__ == "__main__":
    unittest.main()
