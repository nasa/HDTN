#!/usr/bin/env python3

class BitString:
    def __init__(self, bits):
        self.bits = bits.replace(' ', '').lstrip('0')

    def pop_msb(self, n):
        '''Return n bits from MSB side'''
        raise NotImplementedError("TODO")

    def pop_lsb(self, n, missing_ok=False):
        '''Return n bits from lsb side'''
        if not self.bits and not missing_ok:
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
    octets = []
    octets.insert(0, '0' + bs.pop_lsb(7))
    while len(bs):
        octets.insert(0, '1' + bs.pop_lsb(7))
    return BitString(''.join(octets))

def decode(bs):
    octets = []
    octets.insert(0, bs.pop_lsb(7))
    bs.pop_lsb(1, missing_ok=True) # Skip leading bit
    while len(bs):
        octets.insert(0, bs.pop_lsb(7))
        leading_bit = bs.pop_lsb(1) # Skip leading bit
        if leading_bit != '1':
            raise Exception(f"Malformed SDNV near {len(bs)} in bitstream")
    return BitString(''.join(octets))

def main_get_bits(args):
    data = args.data.replace(' ', '')
    if args.input_type == "binary":
        bits = data
    elif args.input_type == "hex":
        as_int = int(data, base=16)
        bits = f'{as_int:b}'
    elif args.input_type == "int":
        as_int = int(data, base=10)
        bits = f'{as_int:b}'
    else:
        raise Exception(f"Bad input type: {args.input_type}")
    return BitString(bits)

def main_encode(args):
    bits = main_get_bits(args)
    enc = encode(bits)
    print(f"bin: {enc}")
    print(f"hex: {enc.to_hex()}")
    print(f"int: {enc.to_int()}")

def main_decode(args):
    bits = main_get_bits(args)
    dec = decode(bits)
    print(f"bin: {dec}")
    print(f"hex: {dec.to_hex()}")
    print(f"int: {dec.to_int()}")
def main():
    import argparse
    parser = argparse.ArgumentParser(description='Encode/decode SDNV values')
    parser.add_argument("mode", choices=('encode', 'decode'))
    parser.add_argument("data")
    parser.add_argument('-t', '--input-type', choices=('binary', 'hex', 'int'), default='hex')

    args = parser.parse_args()
    if args.mode == "encode":
        return main_encode(args)
    elif args.mode == "decode":
        return main_decode(args)
    else:
        print("Uknown mode")
        return 1 

if __name__ == "__main__":
    main()

