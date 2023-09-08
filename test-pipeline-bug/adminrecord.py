#!/usr/bin/env python3
"""Custom scapy Packet types for custody signals"""
import struct
from scapy.packet import Packet, bind_layers, split_layers
from scapy.fields import (
    BitEnumField,
    BitField,
    ConditionalField,
    StrLenField,
    ByteEnumField,
    StrNullField,
    FieldListField,
    PacketListField,
)
from scapy.contrib.sdnv import SDNV2, SDNV2FieldLenField, SDNVUtil, SDNV2LenField
from scapy.contrib.bp import BPBLOCK, BP
from scapy.compat import raw

class BpBlock(Packet):
    """Bpv6 Canonical Block"""

    def guess_payload_class(self, payload):
        ul = self.underlayer
        if isinstance(ul, Bundle):
            bundle = ul
            if (bundle.processing_control_flags & 2) == 2:
                return AdminRecord
        return Payload

    fields_desc = [
        ByteEnumField(name="Type", default=1, enum={1: "payload"}),
        SDNV2("flags", 0),
        SDNV2("block_len", default=0),
    ]

    def post_build(self, pkt, pay):
        if self.block_len == 0 and pay:
            pkt = (
                struct.pack("B", self.Type)
                + raw(SDNVUtil.encode(self.flags))
                + raw(SDNVUtil.encode(len(pay)))
            )
        return pkt + pay

    def mysummary(self):
        return self.sprintf("BpBlock t:%Type% f:%flags% l:%block_len%")

def is_end_of_blocks_list(pkt, lst, cur, remain):
    if cur is None:
        return BpBlock
    if cur.flags & 8 == 8:
        return None
    return BpBlock

class Bundle(Packet):
    fields_desc = [
            ByteEnumField('version', default=6, enum={6:"BPv6"}),
            SDNV2('processing_control_flags', 0),
            SDNV2LenField('block_length', None),
            SDNV2("dest_scheme_offset", 0),
            SDNV2("dest_ssp_offset", 0),
            SDNV2("src_scheme_offset", 0),
            SDNV2("src_ssp_offset", 0),
            SDNV2("report_scheme_offset", 0),
            SDNV2("report_ssp_offset", 0),
            SDNV2("custodian_scheme_offset", 0),
            SDNV2("custodian_ssp_offset", 0),
            SDNV2("creation_timestamp_time", 0),
            SDNV2("creation_timestamp_num", 0),
            SDNV2("lifetime", 0),
            SDNV2FieldLenField("dictionary_length", 0, length_of="dictionary"),
            FieldListField("dictionary", None, 
                StrNullField("dictionary", default=b"", ),
                length_from=(lambda pkt: pkt.dictionary_length)),
            ConditionalField(SDNV2("fragment_offset", 0), 
                             lambda p: (p.processing_control_flags & 1)),
            ConditionalField(SDNV2("total_adu_length", 0),
                             lambda p: (p.processing_control_flags & 1)),
            PacketListField("blocks", [], next_cls_cb=is_end_of_blocks_list)
            ]


class Payload(Packet):
    fields_desc = [
            StrLenField("data", b"", length_from=lambda p: p.underlayer.block_len)
                    ]
    def extract_padding(self, s):
        return '', s


class AdminRecord(Packet):
    """Bpv6 Admin Record"""

    fields_desc = [
        BitEnumField(
            name="Type",
            default=0b0000,
            size=4,
            enum={0b0001: "status report", 0b0010: "custody signal"},
        ),
        BitEnumField(
            name="flags",
            default=0b0000,
            size=4,
            enum={0b0000: "None", 0b0001: "for fragment"},
        ),
    ]

    def mysummary(self):
        return self.sprintf("AdminRecord t:%Type% f:%flags%")

    def extract_padding(self, s):
        return '', s

def _is_for_fragment(pkt):
    """Is a packet for a fragment?

    Needed for optional custody signal fields"""
    if not pkt.underlayer or not "AdminRecord" in pkt.underlayer:
        return False
    return (pkt.underlayer.flags & 0b0001) == 0b0001


class CustodySignal(Packet):
    """Bpv6 Custody Signal"""

    fields_desc = [
        BitField(name="succeeded", default=0, size=1),
        BitEnumField(
            name="reason",
            default=0,
            size=7,
            enum={
                0x00: "no addl info",
                0x01: "reserved",
                0x02: "reserved",
                0x03: "extra recv",
                0x04: "depltd str",
                0x05: "dest ID bad",
                0x06: "no route",
                0x07: "no contact",
                0x08: "bad block",
            },
        ),
        ConditionalField(SDNV2("fragment_offset", 0), _is_for_fragment),
        ConditionalField(SDNV2("fragment_length", 0), _is_for_fragment),
        SDNV2("sig_time_sec", default=0),
        SDNV2("sig_time_nano", default=0),
        SDNV2("creat_time", default=0),
        SDNV2("creat_seq", default=0),
        SDNV2FieldLenField("src_eid_len", None, length_of="src_eid"),
        StrLenField("src_eid", default=b"", length_from=(lambda pkt: pkt.src_eid_len)),
    ]

    def mysummary(self):
        return self.sprintf(
            "CS ok:%succeeded%:<%reason%> "
            "st:%sig_time_sec%.%sig_time_nano% "
            "ct:%creat_time%-%creat_seq% "
            "sl: %src_eid_len% s:%src_eid%"
        )


split_layers(Bundle, BPBLOCK)
bind_layers(AdminRecord, CustodySignal, Type=2)
#bind_layers(Bundle, BpBlock)
#bind_layers(BpBlock, BpBlock)

def main():
    import argparse
    import sys

    parser = argparse.ArgumentParser(description="Dump bundle bytes")
    parser.add_argument("-b", "--bundle", help="Read bundle from arg instead of stdin")
    parser.add_argument("-f", "--format", choices=("raw", "hex"), default="hex", help="Input format for bundle")
    parser.add_argument("-a", "--action", choices=("print", "dumphex", "dumpraw"), default="hex", help="What to do with the bundle")
    parser.add_argument("-o", "--output", help="Write results to file instead of stdout")

    args = parser.parse_args()

    if args.bundle:
        bundle_in = bytes(args.bundle, encoding='utf-8')
    elif sys.stdin.isatty():
        print("Please pipe input to stdin")
        return 1
    else:
        bundle_in = sys.stdin.buffer.read()


    if args.format == "hex":
        s = bundle_in.decode('ascii')
        s = s.replace(' ', '')
        bundle_raw = bytes.fromhex(bundle_in.decode('ascii'))
    else:
        bundle_raw = bytes(bundle_in)

    print(f"Bundle raw: {bundle_raw}")
    print(f"Bundle hex: {bundle_raw.hex()}")

    p = Bundle(bundle_raw)
    p.show2()

if __name__ == "__main__":
    main()
