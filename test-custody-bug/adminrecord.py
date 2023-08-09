from scapy.packet import Packet, bind_layers, split_layers
from scapy.fields import BitEnumField, BitField, ConditionalField, StrLenField, ByteEnumField
from scapy.contrib.sdnv import SDNV2, SDNV2FieldLenField, SDNV2LenField
from scapy.contrib.bp import BPBLOCK, BP

print("EXECUTING ADMINRECORD SCRIPT")

class BpBlock(Packet):
    def guess_payload_class(self, payload):
        ul = self.underlayer
        if isinstance(ul, BP):
            bundle = ul
            if (bundle.ProcFlags & 2) == 2:
                return AdminRecord
            else:
                print(f"DBG: flags {bundle.ProcFlags} don't have 1 set")
        else:
            print(f"DBG: underlayer {ul} not BP ({type(ul)})")
        return Packet.guess_payload_class(self, payload)
    fields_desc = [
            ByteEnumField(name="Type", default=1, enum={1 : "payload"}),
            SDNV2("flags", 0),
            SDNV2LenField("block_len", default=0)
            ]
    def mysummary(self):
        return self.sprintf("BpBlock t:%Type% f:%flags% l:%block_len%")

class AdminRecord(Packet):
    fields_desc = [
        BitEnumField(
            name="Type",
            default=0b0000,
            size=4,
            enum={0b0001: "status report", 0b0010: "custody signal"},
        ),
        BitEnumField(
            name="flags", default=0b0000, size=4, enum={0b0000: "None", 0b0001: "for fragment"}
        ),
    ]

    def mysummary(self):
        return self.sprintf("AdminRecord t:%Type% f:%flags%")


def _is_for_fragment(pkt):
    if not pkt.underlayer or not 'AdminRecord' in pkt.underlayer:
        return False
    return (pkt.underlayer.flags & 0b0001) == 0b0001

#            enum={
#                0x00: "no additional info",
#                0x01: "reserved",
#                0x02: "reserved",
#                0x03: "redundant reception",
#                0x04: "depleted storage",
#                0x05: "destination ID unintelligible",
#                0x06: "no route",
#                0x07: "no contact",
#                0x08: "block unintelligible;",
#            },
class CustodySignal(Packet):
    fields_desc = [
        BitField(name="succeeded", default=0, size=1),
        BitField(
            name="reason",
            default=0,
            size=7,
        ),
        #ConditionalField(SDNV2("fragment_offset", 0), _is_for_fragment),
        #ConditionalField(SDNV2("fragment_length", 0), _is_for_fragment),
        SDNV2("sig_time_sec", default=0),
        SDNV2("sig_time_nano", default=0),
        SDNV2("creat_time", default=0),
        SDNV2("creat_seq", default=0),
        SDNV2FieldLenField("src_eid_len", default=0, length_of="src_eid"),
        StrLenField(
            "src_eid",
            default=b"",
            length_from=(lambda pkt: pkt.src_eid_len+1),
            max_length=512,
        ),
    ]

    def mysummary(self):
        print(f"DBG under type: {self.underlayer.Type}")
        print(f"DBG under flags: {self.underlayer.flags}")
        print(f"DBG success {self.succeeded}")
        print(f"DBG reason: {self.reason}")
        print(f"DBG sts hex: {hex(self.sig_time_sec)}")
        print(f"DBG stn hex: {hex(self.sig_time_nano)}")
        print(f"DBG ct hex: {hex(self.creat_time)}")
        return self.sprintf("CS ok:? st:%sig_time_sec%.%sig_time_nano% ct:%creat_time% cn:%creat_seq% sl:%src_eid_len% s:%src_eid%")

split_layers(BP, BPBLOCK)
bind_layers(AdminRecord, CustodySignal, Type=2)
bind_layers(BP, BpBlock)
