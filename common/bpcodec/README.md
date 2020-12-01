## BPCodec ##

bpcodec is a stand-alone library designed to support encoding and decoding the bundle protocol format.  Both version 6 and version 7 of the bundle protocol are supported.  Encoding / decoding for a number of extension blocks is supported by the library as well.

Note that the use of this library will produce results that are compatible with the bundle protocol from a wire perspective.  It does not, however, intend to offer a complete implementation of either version 6 or version 7 of the Bundle Protocol - semantics related to receipt, forwarding, and custody must be independently implemented and observed by a system that wishes to advertise compatibility with such.

## Overview ##

The core of the library is found in bpv6.h and bpv7.h - each header file is responsible for the corresponding protocol version.  Common extension blocks for version 6 of the protocol are defined within bpv6-ext-block.h.  Extension blocks for version 7 are a work in progress.

The library is geared around two primary operations:

* Taking a byte string and properly decoding it into a C structure, and
* Taking a C structure and properly encoding it into a byte string

The library expects some familiarity with the bundle protocol: for example, one should know that the primary block should come first, and that the primary block is followed by additional canonical blocks.  The library also does not presently enforce constraints (e.g. "the payload block shall always be the last block in a bundle").

The "test" directory includes a small collection of bundles used to validate that encoding / decoding work as expected.  These bundles are utilized by a small collection of benchmark utilities included with the library.  The utilities will be described in additional detail later in this guide.

Note that this library does not yet support the generation of bundle security headers.
