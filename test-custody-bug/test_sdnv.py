#!/usr/bin/env python3
import unittest
from sdnv import BitString, encode, decode

class TestSdnv(unittest.TestCase):
    def test_encode(self):
        cases = [
                ['1010 1011 1100', '10010101 00111100'],
                ['0001 0010 0011 0100', '10100100 00110100'],
                ['0100 0010 0011 0100', '10000001 10000100 00110100'],
                ['0111 1111', '01111111']
                    ]
        for decoded, expected_encoded in cases:
            with self.subTest(decoded):
                b = BitString(decoded)
                expected = BitString(expected_encoded)
                enc = encode(b)
                self.assertEqual(enc, expected)

    def test_to_hex(self):
        cases = [
                ['1010 1011 1100', '0xabc'],
                ['0001 0010 0011 0100', '0x1234'],
                ['0100 0010 0011 0100', '0x4234'],
                ['0111 1111', '0x7f']
                    ]
        for bits, expected_hex in cases:
            with self.subTest(bits):
                b = BitString(bits)
                h = b.to_hex()
                self.assertEqual(h.lower(), expected_hex)

    def test_to_int(self):
        cases = [
                ['1010 1011 1100', 2748],
                ['0001 0010 0011 0100', 4660],
                ['0100 0010 0011 0100', 16948],
                ['0111 1111', 127]
                    ]
        for bits, expected_int in cases:
            with self.subTest(bits):
                b = BitString(bits)
                n = b.to_int()
                self.assertEqual(n, expected_int)

    def test_decode(self):
        cases = [
                ['1010 1011 1100', '10010101 00111100'],
                ['0001 0010 0011 0100', '10100100 00110100'],
                ['0100 0010 0011 0100', '10000001 10000100 00110100'],
                ['0111 1111', '01111111']
                    ]
        for expected_decoded, encoded in cases:
            with self.subTest(encoded):
                b = BitString(encoded)
                expected = BitString(expected_decoded)
                dec = decode(b)
                self.assertEqual(dec, expected)



if __name__ == "__main__":
    unittest.main()
