#!/usr/bin/env python3

class BitString:
    def __init__(self, bits):
        self.bits = bits.replace(' ', '').lstrip('0')
        print(f"BITS: {self.bits}")

    def pop_msb(self, n):
        '''Return n bits from MSB side'''
        raise NotImplementedError("TODO")

    def pop_lsb(self, n):
        '''Return n bits from lsb side'''
        if not self.bits:
            raise Exception("No bits left")
        b = self.bits[-n:]
        self.bits = self.bits[:-n]
        rem = len(b) % n
        if rem:
            b = (n - rem) * '0' + b
        return b

    def msb_index(self):
        for i, b in enumerate(self.bits):
            print(f"Checking {b} at {i}")
            if b == '1':
                return (len(self.bits) - 1) - i
        return 0

    def __str__(self):
        return self.bits

    def __repr__(self):
        return self.bits

    def __len__(self):
        return len(self.bits)

    def __eq__(self, other):
        return self.bits == other.bits

    def to_int(self):
        if not self.bits:
            return 0
        return int(self.bits, base=2)
    def to_hex(self):
        return hex(self.to_int())

def encode(bs):
    msb_index = bs.msb_index()
    print("msb index", msb_index)
    if msb_index < 7:
        print("msb index les than 7, no-op encoding")
        return BitString(bs.bits)
    else:
        print("msb index > 7, applying sdnv encoding")
        parts = []
        s = bs.pop_lsb(7)
        enc = '0' + s
        parts.append(enc)
        print(f"Popped {s}, encoded as {enc}, length left: {len(bs)}")
        while len(bs):
            s = bs.pop_lsb(7)
            enc = '1' + s
            parts.append(enc)
            print(f"Popped {s}, encoded as {enc}, length left: {len(bs)}")
        print(parts)
    return BitString(''.join(parts[::-1]))

def decode(bs):
    parts = []
    while len(bs):
        parts.append(bs.pop_lsb(7))
        bs.pop_lsb(1) # Skip leading bit
    return BitString(''.join(parts[::-1]))


def main():
    s = '1110'
    bs = BitString('1110')
    print(bs)
    print(f'{bs.pop_lsb(1)} : {bs}')
    print(f'{bs.pop_lsb(1)} : {bs}')
    print(f'{bs.pop_lsb(1)} : {bs}')
    print(f'{bs.pop_lsb(1)} : {bs}')

    s = BitString('1110')
    print(f'encoding {s}: {encode(s)}')
    s = BitString('101010111100')
    print(f'encoding {s}: {encode(s)}')

if __name__ == "__main__":
    main()

