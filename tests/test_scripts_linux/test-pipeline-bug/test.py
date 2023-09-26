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
from scapy.contrib.bp import BP
from adminrecord import AdminRecord, CustodySignal, BpBlock, Bundle
from backgroundprocess import BackgroundProcess, TimeoutException
from scapy.all import raw, Raw
import datetime

class DtnTime:
    def __init__(self, ts, seq):
        self.ts = ts 
        self.seq = seq

    BASE = datetime.datetime.fromisoformat('2000-01-01T00:00:00+00:00').timestamp()
    last_ts = 0
    next_count = 0

    @staticmethod
    def get_dtn_time():
        ts = int(datetime.datetime.now().timestamp())
        if ts == DtnTime.last_ts:
            seq = DtnTime.next_count
            DtnTime.next_count += 1
            return DtnTime(ts, seq)
        else:
            DtnTime.last_ts = ts
            DtnTime.next_count = 1
            seq = 0
        return DtnTime(ts, seq)

@contextmanager
def l(*args, end=" "):
    """Print before/after doing something like 'start ... done'"""
    print(*args, "...", end=end, flush=True)
    try:
        yield
        print("done")
    finally:
        pass

def build_bundle(bundle_num, payload_size):
    """Create bundle to send to HDTN"""
    dtn_time = DtnTime.get_dtn_time()
    bundle = BP(
        version=6,  # Version
        ProcFlags=1 << 4 | 1 << 3,  # Bundle processing control flags
        DSO=2,  # Destination scheme offset
        DSSO=3,  # Destination SSP offset
        SSO=1,  # Source scheme offset
        SSSO=4,  # Source SSP offset
        CT=dtn_time.ts,  # Creation time timestamp
        CTSN=dtn_time.seq,  # Creation timestamp sequence number
        CSO=1,
        CSSO=4,
        LT=1000,  # Life time
        DL=0,  # Dictionary length
    )

    payload_base = f'bundle {bundle_num}'.encode('ascii')
    remain_size = payload_size - len(payload_base)
    if remain_size < 0:
        raise Exception(f"Requested payload size {payload_size} bytes too small")
    payload_content = payload_base + (b' ' * remain_size)

    payload = BpBlock(Type=1, flags=(1 << 3)) / Raw(payload_content)
    return bundle / payload


def build_custody_signal(custody_bundle):
    """Build custody signal to send to HDTN"""
    bp = custody_bundle["Bundle"]
    dtn_time = DtnTime.get_dtn_time()
    sig = (
        BP(
            version=6,
            ProcFlags=1 << 4 | 1 << 1,
            DSO=bp.custodian_scheme_offset,
            DSSO=bp.custodian_ssp_offset,
            SSO=2,
            SSSO=3,
            CT=dtn_time.ts,  # Creation time timestamp
            CTSN=dtn_time.seq,  # Creation timestamp sequence number
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
            creat_time=bp.creation_timestamp_time,
            creat_seq=bp.creation_timestamp_num,
            src_eid=f"ipn:{bp.src_scheme_offset}.{bp.src_ssp_offset}",
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
        self.bpsink_exec = (
            root / "build" / "common" / "bpcodec" / "apps" / "bpsink-async"
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

    def test_pipeline(self):
        """Test pipeline errors"""

        # Final destination for bundle

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

        with l("Starting bpsink"):
            cmd = (
                self.bpsink_exec,
                f'--my-uri-eid=ipn:2.3',
                f'--acs-aware-bundle-agent',
                f'--inducts-config-file={self.configs / "bpsink-in.json"}',
                f'--custody-transfer-outducts-config-file={self.configs / "bpsink-out.json"}',
            )
            bpsink = self.process(cmd, "bpsink")
            #bpsink.wait_for_output("UdpBundleSink bound successfully on UDP port", 5)
            time.sleep(5)

        with l("Sending bundles to HDTN"):
            s = self.socket(socket.AF_INET, socket.SOCK_DGRAM)
            # how many bundles do I need here? I need to back up the queue, so at
            # least two bundles: one to put back-pressure on the queue, and one that
            # will be held up
            sent_bundles = []
            for i in range(2):
                bundle = build_bundle(i, 32)
                packet = raw(bundle)
                print(f"sending bundle {i} of size {len(packet)}")
                s.sendto(packet, ("127.0.0.1", 4010))
                sent_bundles.append(Bundle(packet))

        with l("Waiting for custody signals from HDTN"):
            custody_bundles = list(sent_bundles)
            while custody_bundles:
                has_data = select.select([node_one_sock], [], [], 1000)
                if not has_data[0]:
                    raise Exception("No data")
                recv_bundle, _ = node_one_sock.recvfrom(1024)
                b = Bundle(recv_bundle)

                self.assertIn('AdminRecord', b)

                found_index = None
                for i, c in enumerate(custody_bundles):
                    t = b['CustodySignal'].creat_time
                    s = b['CustodySignal'].creat_seq
                    if (
                        c['Bundle'].creation_timestamp_time == t and
                        c['Bundle'].creation_timestamp_num == s):
                        found_index = i
                        break
                else:
                    raise Exception("Custody signal not for bundle we know")
                custody_bundles.pop(found_index)

        # Wait for log errors
        with l("Checking for insert error message"):
            self.assertRaises(
                TimeoutException,
                hdtn.wait_for_output,
                'could not insert custody id into finalDestNodeIdToOpenCustIdsMap', 
                timeout=10)
        with l("Checking for timer message"):
            self.assertRaises(
                TimeoutException,
                hdtn.wait_for_output,
                "can't find custody timer associated with bundle identified by acs custody signal",
                timeout=1)
        with l("Checking for catalog entry error message"):
            self.assertRaises(
                TimeoutException,
                hdtn.wait_for_output,
                "error finding catalog entry for bundle identified by acs custody signal",
                timeout=1)

if __name__ == "__main__":
    unittest.main()
