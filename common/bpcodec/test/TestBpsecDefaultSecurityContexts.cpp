#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/BundleViewV7.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/make_unique.hpp>
#include "PaddedVectorUint8.h"
#include "BinaryConversions.h"
#include <boost/algorithm/string.hpp>

BOOST_AUTO_TEST_CASE(TestBpsecDefaultSecurityContextsSimpleIntegrityTestCase)
{
    /*
    https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/
    Appendix A.  Examples

   This appendix is informative.

   This section presents a series of examples of constructing BPSec
   security blocks (using the security contexts defined in this
   document) and adding those blocks to a sample bundle.

   The examples presented in this appendix represent valid constructions
   of bundles, security blocks, and the encoding of security context
   parameters and results.  For this reason, they can inform unit test
   suites for individual implementations as well as interoperability
   test suites amongst implementations.  However, these examples do not
   cover every permutation of security parameters, security results, or
   use of security blocks in a bundle.

   NOTE: The bundle diagrams in this section are patterned after the
   bundle diagrams used in [I-D.ietf-dtn-bpsec] Section 3.11 "BSP Block
   Examples".

   NOTE: Figures in this section identified as "(CBOR Diagnostic
   Notation)" are represented using the CBOR diagnostic notation defined
   in [RFC8949].  This notation is used to express CBOR data structures
   in a manner that enables visual inspection.  The bundles, security
   blocks, and security context contents in these figures are
   represented using CBOR structures.  In cases where BP blocks (to
   include BPSec security blocks) are comprised of a sequence of CBOR
   objects, these objects are represented as a CBOR sequence as defined
   in [RFC8742].

   NOTE: Examples in this section use the "ipn" URI scheme for
   EndpointID naming, as defined in [I-D.ietf-dtn-bpbis].

   NOTE: The bundle source is presumed to be the security source for all
   security blocks in this section, unless otherwise noted.

A.1.  Example 1: Simple Integrity

   This example shows the addition of a BIB to a sample bundle to
   provide integrity for the payload block.

A.1.1.  Original Bundle

   The following diagram shows the original bundle before the BIB has
   been added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Payload Block                         |   1   |    1   |
        +----------------------------------------+-------+--------+

                    Figure 1: Example 1 Original Bundle

A.1.1.1.  Primary Block

   The BPv7 bundle has no special processing flags and no CRC is
   provided because the primary block is expected to be protected by an
   integrity service BIB using the BIB-HMAC-SHA2 security context.

   The bundle is sourced at the source node ipn:2.1 and destined for the
   destination node ipn:1.2.  The bundle creation time uses a DTN
   creation time of 0 indicating lack of an accurate clock and a
   sequence number of 40.  The lifetime of the bundle is given as
   1,000,000 milliseconds since the bundle creation time.

   The primary block is provided as follows.

   [
     7,           / BP version            /
     0,           / flags                 /
     0,           / CRC type              /
     [2, [1,2]],  / destination (ipn:1.2) /
     [2, [2,1]],  / source      (ipn:2.1) /
     [2, [2,1]],  / report-to   (ipn:2.1) /
     [0, 40],     / timestamp             /
     1000000      / lifetime              /
   ]

             Figure 2: Primary Block (CBOR Diagnostic Notation)

   The CBOR encoding of the primary block is
   0x88070000820282010282028202018202820201820018281a000f4240.

    */
    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    primary.m_destinationEid.Set(1, 2);
    primary.m_sourceNodeId.Set(2, 1);
    primary.m_reportToEid.Set(2, 1);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 0;
    primary.m_creationTimestamp.sequenceNumber = 40;
    primary.m_lifetimeMilliseconds = 1000000;
    bv.m_primaryBlockView.SetManuallyModified();
    {
        std::vector<uint8_t> expectedSerializedPrimary;
        static const std::string expectedSerializedPrimaryString(
            "88070000820282010282028202018202820201820018281a000f4240"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPrimaryString, expectedSerializedPrimary));
        std::vector<uint8_t> actualSerializedPrimary(500);
        const uint64_t actualSerializedPrimarySize = primary.SerializeBpv7(actualSerializedPrimary.data());
        BOOST_REQUIRE_GT(actualSerializedPrimarySize, 0);
        actualSerializedPrimary.resize(actualSerializedPrimarySize);
        BOOST_REQUIRE(expectedSerializedPrimary == actualSerializedPrimary);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPrimary, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPrimaryString);
    }

    /*
    A.1.1.2.  Payload Block

   Other than its use as a source of plaintext for security blocks, the
   payload has no required distinguishing characteristic for the purpose
   of this example.  The sample payload is a 32 byte string whose value
   is "Ready Generate a 32 byte payload".

   The payload is represented in the payload block as a byte string of
   the raw payload string.  It is NOT represented as a CBOR text string
   wrapped within a CBOR binary string.  The hex value of the payload
   "Ready Generate a 32 byte payload" is
   0x52656164792047656e657261746520612033322062797465207061796c6f6164.

   The payload block is provided as follows.
   [
     1,      / type code: Payload block /
     1,      / block number             /
     0,      / block processing flags   /
     0,      / CRC Type                 /
     h'52656164792047656e65726174652061 / type-specific-data: payload /
       2033322062797465207061796c6f6164'
   ]

             Figure 3: Payload Block (CBOR Diagnostic Notation)

   The CBOR encoding of the payload block is 0x8501010000582052656164792
   047656e657261746520612033322062797465207061796c6f6164.
   */

   //add payload block
    static const std::string payloadString("Ready Generate a 32 byte payload");
    {

        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockNumber = 1; //must be 1
        block.m_crcType = BPV7_CRC_TYPE_NONE;

        block.m_dataLength = payloadString.size();
        block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(500));
    }
    //get payload and verify
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
        std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
        //std::cout << "s: " << s << "\n";
        BOOST_REQUIRE_EQUAL(s, payloadString);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());

        //verify from example
        std::vector<uint8_t> expectedSerializedPayloadBlock;
        static const std::string expectedSerializedPayloadBlockString(
            "8501010000582052656164792047656e657261746520612033322062797465207061796c6f6164"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPayloadBlockString, expectedSerializedPayloadBlock));
        uint8_t * ptrPayloadBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedPayloadBlock(ptrPayloadBlockSerialized, ptrPayloadBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedPayloadBlock == actualSerializedPayloadBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPayloadBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
    }

    /*
    A.1.1.3.  Bundle CBOR Representation

   A BPv7 bundle is represented as an indefinite-length array consisting
   of the blocks comprising the bundle, with a terminator character at
   the end.

   The CBOR encoding of the original bundle is 0x9f880700008202820102820
   28202018202820201820018281a000f42408501010000582052656164792047656e65
   7261746520612033322062797465207061796c6f6164ff.
    */
    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f88070000820282010282028202018202820201820018281a000f42408501010000582052656164792047656e657261746520612033322062797465207061796c6f6164ff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
    }

    /*
    A.1.2.  Security Operation Overview

   This example adds a BIB to the bundle using the BIB-HMAC-SHA2
   security context to provide an integrity mechanism over the payload
   block.

   The following diagram shows the resulting bundle after the BIB is
   added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Bundle Integrity Block                |   11  |    2   |
        |  OP(bib-integrity, target=1)           |       |        |
        +----------------------------------------+-------+--------+
        |  Payload Block                         |   1   |    1   |
        +----------------------------------------+-------+--------+

                    Figure 4: Example 1 Resulting Bundle


A.1.3.  Bundle Integrity Block

   In this example, a BIB is used to carry an integrity signature over
   the payload block.

A.1.3.1.  Configuration, Parameters, and Results

   For this example, the following configuration and security parameters
   are used to generate the security results indicated.

   This BIB has a single target and includes a single security result:
   the calculated signature over the payload block.

             Key         : h'1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b'
             SHA Variant : HMAC 512/512
             Scope Flags : 0x00
             Payload Data: h'52656164792047656e65726174652061
                             2033322062797465207061796c6f6164'
             Signature   : h'0654d65992803252210e377d66d0a8dc
                             18a1e8a392269125ae9ac198a9a598be
                             4b83d5daa8be2f2d16769ec1c30cfc34
                             8e2205fba4b3be2b219074fdd5ea8ef0'

        Figure 5: Example 1: Configuration, Parameters, and Results

A.1.3.2.  Abstract Security Block

   The abstract security block structure of the BIB's block-type-
   specific-data field for this application is as follows.

[1],           / Security Target        - Payload block       /
1,             / Security Context ID    - BIB-HMAC-SHA2       /
1,             / Security Context Flags - Parameters Present  /
[2,[2, 1]],    / Security Source        - ipn:2.1             /
[              / Security Parameters    - 2 Parameters        /
   [1, 7],     / SHA Variant            - HMAC 512/512        /
   [3, 0x00]   / Scope Flags            - No Additional Scope /
],
[           / Security Results: 1 Result /
   [1, h'0654d65992803252210e377d66d0a8dc18a1e8a392269125ae9ac198a9a598b
   e4b83d5daa8be2f2d16769ec1c30cfc348e2205fba4b3be2b219074fdd5ea8ef0']
]

  Figure 6: Example 1: BIB Abstract Security Block (CBOR Diagnostic
                              Notation)

   The CBOR encoding of the BIB block-type-specific-data field (the
   abstract security block) is 0x810101018202820201828201078203008182015
   8400654d65992803252210e377d66d0a8dc18a1e8a392269125ae9ac198a9a598be4b
   83d5daa8be2f2d16769ec1c30cfc348e2205fba4b3be2b219074fdd5ea8ef0.
    */
    //add bib
    std::vector<uint8_t> expectedSecurityResult;
    static const std::string expectedSecurityResultString(
        "0654d65992803252210e377d66d0a8dc18a1e8a392269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc348e2205fba4b3be2b219074fdd5ea8ef0"
    );
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
        Bpv7BlockIntegrityBlock & bib = *(reinterpret_cast<Bpv7BlockIntegrityBlock*>(blockPtr.get()));
        //bib.SetZero();

        bib.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
        bib.m_blockNumber = 2;
        bib.m_crcType = BPV7_CRC_TYPE_NONE;
        bib.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 1 });
        //bib.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2); //handled by constructor
        bib.m_securityContextFlags = 0;
        bib.SetSecurityContextParametersPresent();
        bib.m_securitySource.Set(2, 1);
        BOOST_REQUIRE(bib.AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS::HMAC_512_512));
        BOOST_REQUIRE(bib.AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE));
        
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResultString, expectedSecurityResult));
        std::vector<uint8_t> * result1Ptr = bib.AppendAndGetExpectedHmacPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = expectedSecurityResult; //use std::move in production

        
        
        bv.PrependMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(5000));
    }
    //get bib and verify
    Bpv7BlockIntegrityBlock* bibPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bibPtrOriginal = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bibPtrOriginal);
        BOOST_REQUIRE(*bibPtrOriginal == *bibPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBib;
        static const std::string expectedSerializedBibString(
            "8101010182028202018282010782030081820158400654d65992803252210e377d66d0a8dc18a1e8a392269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc348e2205fba4b3be2b219074fdd5ea8ef0"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibString, expectedSerializedBib));
        std::vector<uint8_t> actualSerializedBib(bibPtrOriginal->m_dataPtr, bibPtrOriginal->m_dataPtr + bibPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBib == actualSerializedBib);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBib, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibString);
    

    /*
        A.1.3.3.  Representations

       The BIB wrapping this abstract security block is as follows.

    [
      11, / type code    /
      2,  / block number /
      0,  / flags        /
      0,  / CRC type     /
      h'8101010182028202018282010782030081820158400654d65992803252210e377d66
      d0a8dc18a1e8a392269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc34
      8e2205fba4b3be2b219074fdd5ea8ef0',
    ]

             Figure 7: Example 1: BIB (CBOR Diagnostic Notation)

       The CBOR encoding of the BIB block is 0x850b0200005855810101018202820
       2018282010782030081820158400654d65992803252210e377d66d0a8dc18a1e8a392
       269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc348e2205fba4b3be2
       b219074fdd5ea8ef0.
    */
        //verify whole bib block from example
        std::vector<uint8_t> expectedSerializedBibBlock;
        static const std::string expectedSerializedBibBlockString(
            "850b02000058558101010182028202018282010782030081820158400654d65992803252210e377d66d0a8dc18a1e8a392269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc348e2205fba4b3be2b219074fdd5ea8ef0"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibBlockString, expectedSerializedBibBlock));
        uint8_t * ptrBibBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBibBlock(ptrBibBlockSerialized, ptrBibBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBibBlock == actualSerializedBibBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBibBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibBlockString);


        
    }

    /*
        A.1.4.  Final Bundle

       The CBOR encoding of the full output bundle, with the BIB: 0x9f880700
       00820282010282028202018202820201820018281a000f4240850b020000585581010
       10182028202018282010782030081820158400654d65992803252210e377d66d0a8dc
       18a1e8a392269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc348e220
       5fba4b3be2b219074fdd5ea8ef08501010000582052656164792047656e6572617465
       20612033322062797465207061796c6f6164ff.*/

    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f880700"
            "00820282010282028202018202820201820018281a000f4240850b020000585581010"
            "10182028202018282010782030081820158400654d65992803252210e377d66d0a8dc"
            "18a1e8a392269125ae9ac198a9a598be4b83d5daa8be2f2d16769ec1c30cfc348e220"
            "5fba4b3be2b219074fdd5ea8ef08501010000582052656164792047656e6572617465"
            "20612033322062797465207061796c6f6164ff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);

        //load bundle to test deserialize
        {
            BundleViewV7 bv2;
            std::vector<uint8_t> toSwapIn(expectedSerializedBundle);
            BOOST_REQUIRE(bv2.SwapInAndLoadBundle(toSwapIn)); //swap in a copy
            //get bib and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockIntegrityBlock* bibPtr2 = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bibPtr2);
                BOOST_REQUIRE(*bibPtrOriginal == *bibPtr2);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::INTEGRITY);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockNumber, 2);
                std::vector<std::vector<uint8_t>*> hmacPtrs = bibPtr2->GetAllExpectedHmacPtrs();
                BOOST_REQUIRE_EQUAL(hmacPtrs.size(), 1);
                BOOST_REQUIRE(expectedSecurityResult == *(hmacPtrs[0]));

                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(TestBpsecDefaultSecurityContextsSimpleConfidentialityWithKeyWrapTestCase)
{
    /*
    A.2.  Example 2: Simple Confidentiality with Key Wrap

   This example shows the addition of a BCB to a sample bundle to
   provide confidentiality for the payload block.  AES key wrap is used
   to transmit the symmetric key used to generate the security results
   for this service.

A.2.1.  Original Bundle

   The following diagram shows the original bundle before the BCB has
   been added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Payload Block                         |   1   |    1   |
        +----------------------------------------+-------+--------+

                    Figure 8: Example 2 Original Bundle

A.2.1.1.  Primary Block

   The primary block used in this example is identical to the primary
   block presented in Example 1 Appendix A.1.1.1.

   In summary, the CBOR encoding of the primary block is
   0x88070000820282010282028202018202820201820018281a000f4240.
    */
    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    primary.m_destinationEid.Set(1, 2);
    primary.m_sourceNodeId.Set(2, 1);
    primary.m_reportToEid.Set(2, 1);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 0;
    primary.m_creationTimestamp.sequenceNumber = 40;
    primary.m_lifetimeMilliseconds = 1000000;
    bv.m_primaryBlockView.SetManuallyModified();
    {
        std::vector<uint8_t> expectedSerializedPrimary;
        static const std::string expectedSerializedPrimaryString(
            "88070000820282010282028202018202820201820018281a000f4240"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPrimaryString, expectedSerializedPrimary));
        std::vector<uint8_t> actualSerializedPrimary(500);
        const uint64_t actualSerializedPrimarySize = primary.SerializeBpv7(actualSerializedPrimary.data());
        BOOST_REQUIRE_GT(actualSerializedPrimarySize, 0);
        actualSerializedPrimary.resize(actualSerializedPrimarySize);
        BOOST_REQUIRE(expectedSerializedPrimary == actualSerializedPrimary);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPrimary, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPrimaryString);
    }

    /*
    A.2.1.2.  Payload Block

   The payload block used in this example is identical to the payload
   block presented in Example 1 Appendix A.1.1.2.

   In summary, the CBOR encoding of the payload block is 0x8501010000582
   052656164792047656e657261746520612033322062797465207061796c6f6164.
    */

    //add payload block
    static const std::string payloadString("Ready Generate a 32 byte payload");
    {

        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockNumber = 1; //must be 1
        block.m_crcType = BPV7_CRC_TYPE_NONE;

        block.m_dataLength = payloadString.size();
        block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(500));
    }
    //get payload and verify
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
        std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
        //std::cout << "s: " << s << "\n";
        BOOST_REQUIRE_EQUAL(s, payloadString);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());

        //verify from example
        std::vector<uint8_t> expectedSerializedPayloadBlock;
        static const std::string expectedSerializedPayloadBlockString(
            "8501010000582052656164792047656e657261746520612033322062797465207061796c6f6164"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPayloadBlockString, expectedSerializedPayloadBlock));
        uint8_t * ptrPayloadBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedPayloadBlock(ptrPayloadBlockSerialized, ptrPayloadBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedPayloadBlock == actualSerializedPayloadBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPayloadBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
    }

    /*
    A.2.1.3.  Bundle CBOR Representation

   A BPv7 bundle is represented as an indefinite-length array consisting
   of the blocks comprising the bundle, with a terminator character at
   the end.

   The CBOR encoding of the original bundle is 0x9f880700008202820102820
   28202018202820201820018281a000f42408501010000582052656164792047656e65
   7261746520612033322062797465207061796c6f6164ff.
    */
    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f880700008202820102820"
            "28202018202820201820018281a000f42408501010000582052656164792047656e65"
            "7261746520612033322062797465207061796c6f6164ff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
    }
    /*
    A.2.2.  Security Operation Overview

   This example adds a BCB using the BCB-AES-GCM security context using
   AES key wrap to provide a confidentiality mechanism over the payload
   block and transmit the symmetric key.

   The following diagram shows the resulting bundle after the BCB is
   added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Bundle Confidentiality Block          |   12  |    2   |
        |  OP(bcb-confidentiality, target=1)     |       |        |
        +----------------------------------------+-------+--------+
        |  Payload Block (Encrypted)             |   1   |    1   |
        +----------------------------------------+-------+--------+

                    Figure 9: Example 2 Resulting Bundle

A.2.3.  Bundle Confidentiality Block

   In this example, a BCB is used to encrypt the payload block and uses
   AES key wrap to transmit the symmetric key.

A.2.3.1.  Configuration, Parameters, and Results

   For this example, the following configuration and security parameters
   are used to generate the security results indicated.

   This BCB has a single target, the payload block.  Three security
   results are generated: cipher text which replaces the plain text
   block-type-specific data to encrypt the payload block, an
   authentication tag, and the AES wrapped key.

    Content Encryption
                   Key: h'71776572747975696f70617364666768'
    Key Encryption Key: h'6162636465666768696a6b6c6d6e6f70'
                    IV: h'5477656c7665313231323132'
           AES Variant: A128GCM
       AES Wrapped Key: h'69c411276fecddc4780df42c8a2af892
                          96fabf34d7fae700'
           Scope Flags: 0x00
          Payload Data: h'52656164792047656e65726174652061
                          2033322062797465207061796c6f6164'
    Authentication Tag: h'da08f4d8936024ad7c6b3b800e73dd97'
    Payload Ciphertext: h'3a09c1e63fe2097528a78b7c12943354
                          a563e32648b700c2784e26a990d91f9d'

        Figure 10: Example 2: Configuration, Parameters, and Results

A.2.3.2.  Abstract Security Block

   The abstract security block structure of the BCB's block-type-
   specific-data field for this application is as follows.

   [1],               / Security Target        - Payload block       /
   2,                 / Security Context ID    - BCB-AES-GCM         /
   1,                 / Security Context Flags - Parameters Present  /
   [2,[2, 1]],        / Security Source        - ipn:2.1             /
   [                  / Security Parameters    - 4 Parameters        /
     [1, h'5477656c7665313231323132'], / Initialization Vector       /
     [2, 1],                           / AES Variant - A128GCM       /
     [3, h'69c411276fecddc4780df42c8a  / AES wrapped key             /
           2af89296fabf34d7fae700'],
     [4, 0x00]                         / Scope Flags - No extra scope/
   ],
   [                                   /  Security Results: 1 Result /
     [1, h'da08f4d8936024ad7c6b3b800e73dd97']    / Payload Auth. Tag /
   ]

          Figure 11: Example 2: BCB Abstract Security Block (CBOR
                            Diagnostic Notation)

   The CBOR encoding of the BCB block-type-specific-data field (the
   abstract security block) is 0x8101020182028202018482014c5477656c76653
   132313231328202018203581869c411276fecddc4780df42c8a2af89296fabf34d7fa
   e70082040081820150da08f4d8936024ad7c6b3b800e73dd97.
    */

    //add bcb
    std::vector<uint8_t> expectedInitializationVector;
    static const std::string expectedInitializationVectorString(
        "5477656c7665313231323132"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));
    std::vector<uint8_t> expectedAesWrappedKey;
    static const std::string expectedAesWrappedKeyString(
        "69c411276fecddc4780df42c8a2af89296fabf34d7fae700"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedAesWrappedKeyString, expectedAesWrappedKey));
    std::vector<uint8_t> expectedSecurityResult;
    static const std::string expectedSecurityResultString(
        "da08f4d8936024ad7c6b3b800e73dd97"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResultString, expectedSecurityResult));
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
        Bpv7BlockConfidentialityBlock & bcb = *(reinterpret_cast<Bpv7BlockConfidentialityBlock*>(blockPtr.get()));

        bcb.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::MUST_BE_REPLICATED;
        bcb.m_blockNumber = 2;
        bcb.m_crcType = BPV7_CRC_TYPE_NONE;
        bcb.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 1 });
        //bcb.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM); //handled by constructor
        bcb.m_securityContextFlags = 0;
        bcb.SetSecurityContextParametersPresent();
        bcb.m_securitySource.Set(2, 1);

        std::vector<uint8_t> * ivPtr = bcb.AddAndGetInitializationVectorPtr();
        BOOST_REQUIRE(ivPtr);
        *ivPtr = expectedInitializationVector; //use std::move in production

        BOOST_REQUIRE(bcb.AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS::A128GCM));

        std::vector<uint8_t> * aesWrappedKeyPtr = bcb.AddAndGetAesWrappedKeyPtr();
        BOOST_REQUIRE(aesWrappedKeyPtr);
        *aesWrappedKeyPtr = expectedAesWrappedKey; //use std::move in production

        BOOST_REQUIRE(bcb.AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE));
        
        std::vector<uint8_t> * result1Ptr = bcb.AppendAndGetPayloadAuthenticationTagPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = expectedSecurityResult; //use std::move in production



        bv.PrependMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(5000));
    }
    //get bcb and verify
    Bpv7BlockConfidentialityBlock* bcbPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bcbPtrOriginal = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bcbPtrOriginal);
        BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBcb;
        static const std::string expectedSerializedBcbString(
            "8101020182028202018482014c5477656c76653"
            "132313231328202018203581869c411276fecddc4780df42c8a2af89296fabf34d7fa"
            "e70082040081820150da08f4d8936024ad7c6b3b800e73dd97"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbString, expectedSerializedBcb));
        std::vector<uint8_t> actualSerializedBcb(bcbPtrOriginal->m_dataPtr, bcbPtrOriginal->m_dataPtr + bcbPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBcb == actualSerializedBcb);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBcb, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbString);

        /*
        A.2.3.3.  Representations

   The BCB wrapping this abstract security block is as follows.

   [
     12, / type code /
     2,  / block number /
     1,  / flags - block must be replicated in every fragment /
     0,  / CRC type /
     h'8101020182028202018482014c5477656c766531323132313282020182035818
       69c411276fecddc4780df42c8a2af89296fabf34d7fae70082040081820150da
       08f4d8936024ad7c6b3b800e73dd97'
   ]

            Figure 12: Example 2: BCB (CBOR Diagnostic Notation)

   The CBOR encoding of the BCB block is 0x850c020100584f810102018202820
   2018482014c5477656c76653132313231328202018203581869c411276fecddc4780d
   f42c8a2af89296fabf34d7fae70082040081820150da08f4d8936024ad7c6b3b800e7
   3dd97.
        */
        //verify from example
        std::vector<uint8_t> expectedSerializedBcbBlock;
        static const std::string expectedSerializedBcbBlockString(
            "850c020100584f810102018202820"
            "2018482014c5477656c76653132313231328202018203581869c411276fecddc4780d"
            "f42c8a2af89296fabf34d7fae70082040081820150da08f4d8936024ad7c6b3b800e7"
            "3dd97"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbBlockString, expectedSerializedBcbBlock));
        uint8_t * ptrBcbBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBcbBlock(ptrBcbBlockSerialized, ptrBcbBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBcbBlock == actualSerializedBcbBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBcbBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbBlockString);
    }

    /*
    A.2.4.  Final Bundle

   The CBOR encoding of the full output bundle, with the BCB: 0x9f880700
   00820282010282028202018202820201820018281a000f4240850c020100584f81010
   20182028202018482014c5477656c76653132313231328202018203581869c411276f
   ecddc4780df42c8a2af89296fabf34d7fae70082040081820150da08f4d8936024ad7
   c6b3b800e73dd97850101000058203a09c1e63fe2097528a78b7c12943354a563e326
   48b700c2784e26a990d91f9dff.
    */
    //"encrypt" payload
    static const std::string payloadCipherTextString(
        "3a09c1e63fe2097528a78b7c12943354a563e32648b700c2784e26a990d91f9d"
    );
    std::vector<uint8_t> payloadCipherText;
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(payloadCipherTextString, payloadCipherText));
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        blocks[0]->headerPtr->m_dataPtr = payloadCipherText.data();
        blocks[0]->headerPtr->m_dataLength = payloadCipherText.size();
        blocks[0]->SetManuallyModified();
        BOOST_REQUIRE(bv.Render(5000));

        //verify from example
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f880700"
            "00820282010282028202018202820201820018281a000f4240850c020100584f81010"
            "20182028202018482014c5477656c76653132313231328202018203581869c411276f"
            "ecddc4780df42c8a2af89296fabf34d7fae70082040081820150da08f4d8936024ad7"
            "c6b3b800e73dd97850101000058203a09c1e63fe2097528a78b7c12943354a563e326"
            "48b700c2784e26a990d91f9dff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);

        //load bundle to test deserialize
        {
            BundleViewV7 bv2;
            std::vector<uint8_t> toSwapIn(expectedSerializedBundle);
            BOOST_REQUIRE(bv2.SwapInAndLoadBundle(toSwapIn)); //swap in a copy
            //get bib and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockConfidentialityBlock* bcbPtr2 = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bcbPtr2);
                BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtr2);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockNumber, 2);
                std::vector<std::vector<uint8_t>*> patPtrs = bcbPtr2->GetAllPayloadAuthenticationTagPtrs();
                BOOST_REQUIRE_EQUAL(patPtrs.size(), 1);
                BOOST_REQUIRE(expectedSecurityResult == *(patPtrs[0]));

                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(TestBpsecDefaultSecurityContextsSecurityBlocksFromMultipleSourcesTestCase)
{
    /*
    A.3.  Example 3: Security Blocks from Multiple Sources

   This example shows the addition of a BIB and BCB to a sample bundle.
   These two security blocks are added by two different nodes.  The BCB
   is added by the source endpoint and the BIB is added by a forwarding
   node.

   The resulting bundle contains a BCB to encrypt the Payload Block and
   a BIB to provide integrity to the Primary and Bundle Age Block.

A.3.1.  Original Bundle

   The following diagram shows the original bundle before the security
   blocks have been added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Extension Block: Bundle Age Block     |   7   |    2   |
        +----------------------------------------+-------+--------+
        |  Payload Block                         |   1   |    1   |
        +----------------------------------------+-------+--------+

                    Figure 13: Example 3 Original Bundle

A.3.1.1.  Primary Block

   The primary block used in this example is identical to the primary
   block presented in Example 1 Appendix A.1.1.1.

   In summary, the CBOR encoding of the primary block is
   0x88070000820282010282028202018202820201820018281a000f4240.
    */
    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    primary.m_destinationEid.Set(1, 2);
    primary.m_sourceNodeId.Set(2, 1);
    primary.m_reportToEid.Set(2, 1);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 0;
    primary.m_creationTimestamp.sequenceNumber = 40;
    primary.m_lifetimeMilliseconds = 1000000;
    bv.m_primaryBlockView.SetManuallyModified();
    {
        std::vector<uint8_t> expectedSerializedPrimary;
        static const std::string expectedSerializedPrimaryString(
            "88070000820282010282028202018202820201820018281a000f4240"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPrimaryString, expectedSerializedPrimary));
        std::vector<uint8_t> actualSerializedPrimary(500);
        const uint64_t actualSerializedPrimarySize = primary.SerializeBpv7(actualSerializedPrimary.data());
        BOOST_REQUIRE_GT(actualSerializedPrimarySize, 0);
        actualSerializedPrimary.resize(actualSerializedPrimarySize);
        BOOST_REQUIRE(expectedSerializedPrimary == actualSerializedPrimary);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPrimary, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPrimaryString);
    }

    /*
    A.3.1.2.  Bundle Age Block

   A bundle age block is added to the bundle to help other nodes in the
   network determine the age of the bundle.  The use of this block is as
   recommended because the bundle source does not have an accurate clock
   (as indicated by the DTN time of 0).

   Because this block is specified at the time the bundle is being
   forwarded, the bundle age represents the time that has elapsed from
   the time the bundle was created to the time it is being prepared for
   forwarding.  In this case, the value is given as 300 milliseconds.

   The bundle age extension block is provided as follows.

   [
     7,      / type code: Bundle Age block /
     2,      / block number /
     0,      / block processing flags /
     0,      / CRC Type /
     <<300>> / type-specific-data: age /
   ]

           Figure 14: Bundle Age Block (CBOR Diagnostic Notation)

   The CBOR encoding of the bundle age block is 0x85070200004319012c.
    */

    //add bundle age block
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BundleAgeCanonicalBlock>();
        Bpv7BundleAgeCanonicalBlock & block = *(reinterpret_cast<Bpv7BundleAgeCanonicalBlock*>(blockPtr.get()));
        //block.SetZero();

        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
        block.m_blockNumber = 2;
        block.m_crcType = BPV7_CRC_TYPE_NONE;
        block.m_bundleAgeMilliseconds = 300;

        bv.PrependMoveCanonicalBlock(blockPtr);
    }

    /*
    A.3.1.3.  Payload Block

   The payload block used in this example is identical to the payload
   block presented in Example 1 Appendix A.1.1.2.

   In summary, the CBOR encoding of the payload block is 0x8501010000582
   052656164792047656e657261746520612033322062797465207061796c6f6164.
    */
    //add payload block
    static const std::string payloadString("Ready Generate a 32 byte payload");
    {

        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockNumber = 1; //must be 1
        block.m_crcType = BPV7_CRC_TYPE_NONE;

        block.m_dataLength = payloadString.size();
        block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(500));
    }
    //get bundle age and verify
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        
        //verify from example
        std::vector<uint8_t> expectedSerializedBundleAgeBlock;
        static const std::string expectedSerializedBundleAgeBlockString(
            "85070200004319012c"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleAgeBlockString, expectedSerializedBundleAgeBlock));
        uint8_t * ptrBundleAgeBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBundleAgeBlock(ptrBundleAgeBlockSerialized, ptrBundleAgeBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBundleAgeBlock == actualSerializedBundleAgeBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBundleAgeBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleAgeBlockString);
    }
    //get payload and verify
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
        std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
        //std::cout << "s: " << s << "\n";
        BOOST_REQUIRE_EQUAL(s, payloadString);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());

        //verify from example
        std::vector<uint8_t> expectedSerializedPayloadBlock;
        static const std::string expectedSerializedPayloadBlockString(
            "8501010000582"
            "052656164792047656e657261746520612033322062797465207061796c6f6164"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPayloadBlockString, expectedSerializedPayloadBlock));
        uint8_t * ptrPayloadBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedPayloadBlock(ptrPayloadBlockSerialized, ptrPayloadBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedPayloadBlock == actualSerializedPayloadBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPayloadBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
    }

    /*
    A.3.1.4.  Bundle CBOR Representation

   A BPv7 bundle is represented as an indefinite-length array consisting
   of the blocks comprising the bundle, with a terminator character at
   the end.

   The CBOR encoding of the original bundle is 0x9f880700008202820102820
   28202018202820201820018281a000f424085070200004319012c8501010000582052
   656164792047656e657261746520612033322062797465207061796c6f6164ff.
    */
    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f880700008202820102820"
            "28202018202820201820018281a000f424085070200004319012c8501010000582052"
            "656164792047656e657261746520612033322062797465207061796c6f6164ff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
    }

    /*
    A.3.2.  Security Operation Overview

   This example provides:

      a BIB with the BIB-HMAC-SHA2 security context to provide an
      integrity mechanism over the primary block and bundle age block.

      a BCB with the BCB-AES-GCM security context to provide a
      confidentiality mechanism over the payload block.

   The following diagram shows the resulting bundle after the security
   blocks are added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Bundle Integrity Block                |   11  |    3   |
        |  OP(bib-integrity, targets=0, 2)       |       |        |
        +----------------------------------------+-------+--------+
        |  Bundle Confidentiality Block          |   12  |    4   |
        |  OP(bcb-confidentiality, target=1)     |       |        |
        +----------------------------------------+-------+--------+
        |  Extension Block: Bundle Age Block     |   7   |    2   |
        +----------------------------------------+-------+--------+
        |  Payload Block (Encrypted)             |   1   |    1   |
        +----------------------------------------+-------+--------+

                   Figure 15: Example 3 Resulting Bundle

A.3.3.  Bundle Integrity Block

   In this example, a BIB is used to carry an integrity signature over
   the bundle age block and an additional signature over the payload
   block.  The BIB is added by a waypoint node, ipn:3.0.

A.3.3.1.  Configuration, Parameters, and Results

   For this example, the following configuration and security parameters
   are used to generate the security results indicated.

   This BIB has two security targets and includes two security results,
   holding the calculated signatures over the bundle age block and
   primary block.

                   Key: h'1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b'
           SHA Variant: HMAC 256/256
           Scope Flags: 0x00
    Primary Block Data: h'88070000820282010282028202018202
                          820201820018281a000f4240'
    Bundle Age Block
                  Data: h'85070200004319012c'
    Primary Block
             Signature: h'8e059b8e71f7218264185a666bf3e453
                          076f2b883f4dce9b3cdb6464ed0dcf0f'
    Bundle Age Block
             Signature: h'72dee8eba049a22978e84a95d0496466
                          8eb131b1ca4800c114206d70d9065c80'

      Figure 16: Example 3: Configuration, Parameters, and Results for
                                  the BIB

A.3.3.2.  Abstract Security Block

   The abstract security block structure of the BIB's block-type-
   specific-data field for this application is as follows.

   [0, 2],         / Security Targets                             /
   1,              / Security Context ID    - BIB-HMAC-SHA2       /
   1,              / Security Context Flags - Parameters Present  /
   [2,[3, 0]],     / Security Source        - ipn:3.0             /
   [               / Security Parameters    - 2 Parameters        /
      [1, 5],      / SHA Variant            - HMAC 256/256        /
      [3, 0x00]    / Scope Flags            - No Additional Scope /
   ],
   [           / Security Results: 2 Results /
      [1, h'8e059b8e71f7218264185a666bf3e453
            076f2b883f4dce9b3cdb6464ed0dcf0f'], / Primary Block   /
      [1, h'72dee8eba049a22978e84a95d0496466
            8eb131b1ca4800c114206d70d9065c80'] / Bundle Age Block /
   ]

          Figure 17: Example 3: BIB Abstract Security Block (CBOR
                            Diagnostic Notation)

   The CBOR encoding of the BIB block-type-specific-data field (the
   abstract security block) is 0x820002010182028203008282010582030082820
   158208e059b8e71f7218264185a666bf3e453076f2b883f4dce9b3cdb6464ed0dcf0f
   8201582072dee8eba049a22978e84a95d04964668eb131b1ca4800c114206d70d9065
   c80.
    */

    //add bib
    std::vector<uint8_t> expectedSecurityResult1PrimaryBlock;
    static const std::string expectedSecurityResult1PrimaryBlockString(
        "8e059b8e71f7218264185a666bf3e453076f2b883f4dce9b3cdb6464ed0dcf0f"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResult1PrimaryBlockString, expectedSecurityResult1PrimaryBlock));
    std::vector<uint8_t> expectedSecurityResult2BundleAgeBlock;
    static const std::string expectedSecurityResult2BundleAgeBlockString(
        "72dee8eba049a22978e84a95d04964668eb131b1ca4800c114206d70d9065c80"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResult2BundleAgeBlockString, expectedSecurityResult2BundleAgeBlock));
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
        Bpv7BlockIntegrityBlock & bib = *(reinterpret_cast<Bpv7BlockIntegrityBlock*>(blockPtr.get()));
        //bib.SetZero();

        bib.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
        bib.m_blockNumber = 3;
        bib.m_crcType = BPV7_CRC_TYPE_NONE;
        bib.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 0, 2 });
        //bib.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2); //handled by constructor
        bib.m_securityContextFlags = 0;
        bib.SetSecurityContextParametersPresent();
        bib.m_securitySource.Set(3, 0);
        BOOST_REQUIRE(bib.AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS::HMAC_256_256));
        BOOST_REQUIRE(bib.AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE));

        
        std::vector<uint8_t> * result1Ptr = bib.AppendAndGetExpectedHmacPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = expectedSecurityResult1PrimaryBlock; //use std::move in production

        std::vector<uint8_t> * result2Ptr = bib.AppendAndGetExpectedHmacPtr();
        BOOST_REQUIRE(result2Ptr);
        *result2Ptr = expectedSecurityResult2BundleAgeBlock; //use std::move in production

        BOOST_REQUIRE_EQUAL(bib.GetAllExpectedHmacPtrs().size(), 2);

        bv.PrependMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(5000));
    }
    //get bib and verify
    Bpv7BlockIntegrityBlock* bibPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bibPtrOriginal = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bibPtrOriginal);
        BOOST_REQUIRE(*bibPtrOriginal == *bibPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBib;
        static const std::string expectedSerializedBibString(
            "820002010182028203008282010582030082820"
            "158208e059b8e71f7218264185a666bf3e453076f2b883f4dce9b3cdb6464ed0dcf0f"
            "8201582072dee8eba049a22978e84a95d04964668eb131b1ca4800c114206d70d9065"
            "c80"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibString, expectedSerializedBib));
        std::vector<uint8_t> actualSerializedBib(bibPtrOriginal->m_dataPtr, bibPtrOriginal->m_dataPtr + bibPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBib == actualSerializedBib);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBib, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibString);


        /*
            A.3.3.3.  Representations

   The BIB wrapping this abstract security block is as follows.

   [
     11, / type code /
     3,  / block number /
     0,  / flags  /
     0,  / CRC type /
     h'820002010182028203008282010582030082820158208e059b8e71f721826418
       5a666bf3e453076f2b883f4dce9b3cdb6464ed0dcf0f8201582072dee8eba049
       a22978e84a95d04964668eb131b1ca4800c114206d70d9065c80',
   ]

            Figure 18: Example 3: BIB (CBOR Diagnostic Notation)

   The CBOR encoding of the BIB block is 0x850b030000585a820002010182028
   203008282010582030082820158208e059b8e71f7218264185a666bf3e453076f2b88
   3f4dce9b3cdb6464ed0dcf0f8201582072dee8eba049a22978e84a95d04964668eb13
   1b1ca4800c114206d70d9065c80.
        */
        //verify whole bib block from example
        std::vector<uint8_t> expectedSerializedBibBlock;
        static const std::string expectedSerializedBibBlockString(
            "850b030000585a820002010182028"
            "203008282010582030082820158208e059b8e71f7218264185a666bf3e453076f2b88"
            "3f4dce9b3cdb6464ed0dcf0f8201582072dee8eba049a22978e84a95d04964668eb13"
            "1b1ca4800c114206d70d9065c80"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibBlockString, expectedSerializedBibBlock));
        uint8_t * ptrBibBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBibBlock(ptrBibBlockSerialized, ptrBibBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBibBlock == actualSerializedBibBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBibBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibBlockString);
    }

    /*
    A.3.4.  Bundle Confidentiality Block

   In this example, a BCB is used encrypt the payload block.  The BCB is
   added by the bundle source node, ipn:2.1.

A.3.4.1.  Configuration, Parameters, and Results

   For this example, the following configuration and security parameters
   are used to generate the security results indicated.

   This BCB has a single target, the payload block.  Two security
   results are generated: cipher text which replaces the plain text
   block-type-specific data to encrypt the payload block, and an
   authentication tag.

    Content Encryption
                   Key: h'71776572747975696f70617364666768'
                    IV: h'5477656c7665313231323132'
           AES Variant: A128GCM
           Scope Flags: 0x00
          Payload Data: h'52656164792047656e65726174652061
                          2033322062797465207061796c6f6164'
    Authentication Tag: h'da08f4d8936024ad7c6b3b800e73dd97'
    Payload Ciphertext: h'3a09c1e63fe2097528a78b7c12943354
                          a563e32648b700c2784e26a990d91f9d'

      Figure 19: Example 3: Configuration, Parameters, and Results for
                                  the BCB

A.3.4.2.  Abstract Security Block

   The abstract security block structure of the BCB's block-type-
   specific-data field for this application is as follows.

   [1],             / Security Target        - Payload block      /
   2,               / Security Context ID    - BCB-AES-GCM        /
   1,               / Security Context Flags - Parameters Present /
   [2,[2, 1]],      / Security Source        - ipn:2.1            /
   [                / Security Parameters    - 3 Parameters       /
     [1, h'5477656c7665313231323132'],    / Initialization Vector /
     [2, 1],                              / AES Variant - AES 128 /
     [4, 0x00]                / Scope Flags - No Additional Scope /
   ],
   [                                 / Security Results: 1 Result /
     [1, h'da08f4d8936024ad7c6b3b800e73dd97'] / Payload Auth. Tag /
   ]

          Figure 20: Example 3: BCB Abstract Security Block (CBOR
                            Diagnostic Notation)

   The CBOR encoding of the BCB block-type-specific-data field (the
   abstract security block) is 0x8101020182028202018382014c5477656c76653
   1323132313282020182040081820150da08f4d8936024ad7c6b3b800e73dd97.
    */

    //add bcb
    std::vector<uint8_t> expectedInitializationVector;
    static const std::string expectedInitializationVectorString(
        "5477656c7665313231323132"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));
    
    std::vector<uint8_t> expectedSecurityResult1PayloadAuthTag;
    static const std::string expectedSecurityResult1PayloadAuthTagString(
        "da08f4d8936024ad7c6b3b800e73dd97"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResult1PayloadAuthTagString, expectedSecurityResult1PayloadAuthTag));
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
        Bpv7BlockConfidentialityBlock & bcb = *(reinterpret_cast<Bpv7BlockConfidentialityBlock*>(blockPtr.get()));

        bcb.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::MUST_BE_REPLICATED;
        bcb.m_blockNumber = 4;
        bcb.m_crcType = BPV7_CRC_TYPE_NONE;
        bcb.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 1 });
        //bcb.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM); //handled by constructor
        bcb.m_securityContextFlags = 0;
        bcb.SetSecurityContextParametersPresent();
        bcb.m_securitySource.Set(2, 1);

        std::vector<uint8_t> * ivPtr = bcb.AddAndGetInitializationVectorPtr();
        BOOST_REQUIRE(ivPtr);
        *ivPtr = expectedInitializationVector; //use std::move in production

        BOOST_REQUIRE(bcb.AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS::A128GCM));

        BOOST_REQUIRE(bcb.AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE));

        std::vector<uint8_t> * result1Ptr = bcb.AppendAndGetPayloadAuthenticationTagPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = expectedSecurityResult1PayloadAuthTag; //use std::move in production



        bv.InsertMoveCanonicalBlockAfterBlockNumber(blockPtr, 3); //insert after first bib which is block number 3 to match example
        BOOST_REQUIRE(bv.Render(5000));
    }
    //get bcb and verify
    Bpv7BlockConfidentialityBlock* bcbPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bcbPtrOriginal = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bcbPtrOriginal);
        BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBcb;
        static const std::string expectedSerializedBcbString(
            "8101020182028202018382014c5477656c76653"
            "1323132313282020182040081820150da08f4d8936024ad7c6b3b800e73dd97"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbString, expectedSerializedBcb));
        std::vector<uint8_t> actualSerializedBcb(bcbPtrOriginal->m_dataPtr, bcbPtrOriginal->m_dataPtr + bcbPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBcb == actualSerializedBcb);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBcb, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbString);

        /*
        A.3.4.3.  Representations

   The BCB wrapping this abstract security block is as follows.

   [
     12, / type code /
     4,  / block number /
     1,  / flags - block must be replicated in every fragment /
     0,  / CRC type /
     h'8101020182028202018382014c5477656c766531323132313282020182040081
       820150da08f4d8936024ad7c6b3b800e73dd97',
   ]

            Figure 21: Example 3: BCB (CBOR Diagnostic Notation)

   The CBOR encoding of the BCB block is 0x850c0401005833810102018202820
   2018382014c5477656c766531323132313282020182040081820150da08f4d8936024
   ad7c6b3b800e73dd97.
        */
        //verify from example
        std::vector<uint8_t> expectedSerializedBcbBlock;
        static const std::string expectedSerializedBcbBlockString(
            "850c0401005833810102018202820"
            "2018382014c5477656c766531323132313282020182040081820150da08f4d8936024"
            "ad7c6b3b800e73dd97"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbBlockString, expectedSerializedBcbBlock));
        uint8_t * ptrBcbBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBcbBlock(ptrBcbBlockSerialized, ptrBcbBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBcbBlock == actualSerializedBcbBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBcbBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbBlockString);
    }
    /*
    A.3.5.  Final Bundle

   The CBOR encoding of the full output bundle, with the BIB and BCB
   added is: 0x9f88070000820282010282028202018202820201820018281a000f424
   0850b030000585a820002010182028203008282010582030082820158208e059b8e71
   f7218264185a666bf3e453076f2b883f4dce9b3cdb6464ed0dcf0f8201582072dee8e
   ba049a22978e84a95d04964668eb131b1ca4800c114206d70d9065c80850c04010058
   338101020182028202018382014c5477656c766531323132313282020182040081820
   150da08f4d8936024ad7c6b3b800e73dd9785070200004319012c850101000058203a
   09c1e63fe2097528a78b7c12943354a563e32648b700c2784e26a990d91f9dff.
    */
    //"encrypt" payload
    static const std::string payloadCipherTextString(
        "3a09c1e63fe2097528a78b7c12943354a563e32648b700c2784e26a990d91f9d"
    );
    std::vector<uint8_t> payloadCipherText;
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(payloadCipherTextString, payloadCipherText));
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        blocks[0]->headerPtr->m_dataPtr = payloadCipherText.data();
        blocks[0]->headerPtr->m_dataLength = payloadCipherText.size();
        blocks[0]->SetManuallyModified();
        BOOST_REQUIRE(bv.Render(5000));

        //verify from example
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f88070000820282010282028202018202820201820018281a000f424"
            "0850b030000585a820002010182028203008282010582030082820158208e059b8e71"
            "f7218264185a666bf3e453076f2b883f4dce9b3cdb6464ed0dcf0f8201582072dee8e"
            "ba049a22978e84a95d04964668eb131b1ca4800c114206d70d9065c80850c04010058"
            "338101020182028202018382014c5477656c766531323132313282020182040081820"
            "150da08f4d8936024ad7c6b3b800e73dd9785070200004319012c850101000058203a"
            "09c1e63fe2097528a78b7c12943354a563e32648b700c2784e26a990d91f9dff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);

        //load bundle to test deserialize
        {
            BundleViewV7 bv2;
            std::vector<uint8_t> toSwapIn(expectedSerializedBundle);
            BOOST_REQUIRE(bv2.SwapInAndLoadBundle(toSwapIn)); //swap in a copy
            //get bcb and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockConfidentialityBlock* bcbPtr2 = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bcbPtr2);
                BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtr2);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockNumber, 4);
                std::vector<std::vector<uint8_t>*> patPtrs = bcbPtr2->GetAllPayloadAuthenticationTagPtrs();
                BOOST_REQUIRE_EQUAL(patPtrs.size(), 1);
                BOOST_REQUIRE(expectedSecurityResult1PayloadAuthTag == *(patPtrs[0]));

                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }

            //get bib and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockIntegrityBlock* bibPtr2 = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bibPtr2);
                BOOST_REQUIRE(*bibPtrOriginal == *bibPtr2);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::INTEGRITY);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockNumber, 3);
                std::vector<std::vector<uint8_t>*> hmacPtrs = bibPtr2->GetAllExpectedHmacPtrs();
                BOOST_REQUIRE_EQUAL(hmacPtrs.size(), 2);
                BOOST_REQUIRE(expectedSecurityResult1PrimaryBlock == *(hmacPtrs[0]));
                BOOST_REQUIRE(expectedSecurityResult2BundleAgeBlock == *(hmacPtrs[1]));

                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(TestBpsecDefaultSecurityContextsSecurityBlocksWithFullScopeTestCase)
{
    /*
    A.4.  Example 4: Security Blocks with Full Scope

   This example shows the addition of a BIB and BCB to a sample bundle.
   A BIB is added to provide integrity over the payload block and a BCB
   is added for confidentiality over the payload and BIB.

   The integrity scope and additional authentication data will bind the
   primary block, target header, and the security header.

A.4.1.  Original Bundle

   The following diagram shows the original bundle before the security
   blocks have been added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Payload Block                         |   1   |    1   |
        +----------------------------------------+-------+--------+

                    Figure 22: Example 4 Original Bundle

A.4.1.1.  Primary Block

   The primary block used in this example is identical to the primary
   block presented in Example 1 Appendix A.1.1.1.

   In summary, the CBOR encoding of the primary block is
   0x88070000820282010282028202018202820201820018281a000f4240.
    */
    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    primary.m_destinationEid.Set(1, 2);
    primary.m_sourceNodeId.Set(2, 1);
    primary.m_reportToEid.Set(2, 1);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 0;
    primary.m_creationTimestamp.sequenceNumber = 40;
    primary.m_lifetimeMilliseconds = 1000000;
    bv.m_primaryBlockView.SetManuallyModified();
    {
        std::vector<uint8_t> expectedSerializedPrimary;
        static const std::string expectedSerializedPrimaryString(
            "88070000820282010282028202018202820201820018281a000f4240"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPrimaryString, expectedSerializedPrimary));
        std::vector<uint8_t> actualSerializedPrimary(500);
        const uint64_t actualSerializedPrimarySize = primary.SerializeBpv7(actualSerializedPrimary.data());
        BOOST_REQUIRE_GT(actualSerializedPrimarySize, 0);
        actualSerializedPrimary.resize(actualSerializedPrimarySize);
        BOOST_REQUIRE(expectedSerializedPrimary == actualSerializedPrimary);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPrimary, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPrimaryString);
    }

    /*
    A.4.1.2.  Payload Block

   The payload block used in this example is identical to the payload
   block presented in Example 1 Appendix A.1.1.2.

   In summary, the CBOR encoding of the payload block is 0x8501010000582
   052656164792047656e657261746520612033322062797465207061796c6f6164.
    */
    //add payload block
    static const std::string payloadString("Ready Generate a 32 byte payload");
    {

        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockNumber = 1; //must be 1
        block.m_crcType = BPV7_CRC_TYPE_NONE;

        block.m_dataLength = payloadString.size();
        block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(500));
    }
    //get payload and verify
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
        std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
        //std::cout << "s: " << s << "\n";
        BOOST_REQUIRE_EQUAL(s, payloadString);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());

        //verify from example
        std::vector<uint8_t> expectedSerializedPayloadBlock;
        static const std::string expectedSerializedPayloadBlockString(
            "8501010000582052656164792047656e657261746520612033322062797465207061796c6f6164"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPayloadBlockString, expectedSerializedPayloadBlock));
        uint8_t * ptrPayloadBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedPayloadBlock(ptrPayloadBlockSerialized, ptrPayloadBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedPayloadBlock == actualSerializedPayloadBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPayloadBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
    }

    /*
    A.4.1.3.  Bundle CBOR Representation

   A BPv7 bundle is represented as an indefinite-length array consisting
   of the blocks comprising the bundle, with a terminator character at
   the end.

   The CBOR encoding of the original bundle is 0x9f880700008202820102820
   28202018202820201820018281a000f42408501010000582052656164792047656e65
   7261746520612033322062797465207061796c6f6164ff.
    */
    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f880700008202820102820"
            "28202018202820201820018281a000f42408501010000582052656164792047656e65"
            "7261746520612033322062797465207061796c6f6164ff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
    }

    /*
    A.4.2.  Security Operation Overview

   This example provides:

      a BIB with the BIB-HMAC-SHA2 security context to provide an
      integrity mechanism over the payload block.

      a BCB with the BCB-AES-GCM security context to provide a
      confidentiality mechanism over the payload block and BIB.

   The following diagram shows the resulting bundle after the security
   blocks are added.

                          Block                    Block   Block
                        in Bundle                  Type    Number
        +========================================+=======+========+
        |  Primary Block                         |  N/A  |    0   |
        +----------------------------------------+-------+--------+
        |  Bundle Integrity Block (Encrypted)    |   11  |    3   |
        |  OP(bib-integrity, target=1)           |       |        |
        +----------------------------------------+-------+--------+
        |  Bundle Confidentiality Block          |   12  |    2   |
        |  OP(bcb-confidentiality, targets=1, 3) |       |        |
        +----------------------------------------+-------+--------+
        |  Payload Block (Encrypted)             |   1   |    1   |
        +----------------------------------------+-------+--------+

                   Figure 23: Example 4 Resulting Bundle

A.4.3.  Bundle Integrity Block

   In this example, a BIB is used to carry an integrity signature over
   the payload block.  The IPPT contains the payload block block-type-
   specific data, primary block data, the payload block header, and the
   BIB header.  That is, all additional headers are included in the
   IPPT.

A.4.3.1.  Configuration, Parameters, and Results

   For this example, the following configuration and security parameters
   are used to generate the security results indicated.

   This BIB has a single target and includes a single security result:
   the calculated signature over the Payload block.

                         Key: h'1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b'
                 SHA Variant: HMAC 384/384
                 Scope Flags: 0x07  (all additional headers)
          Primary Block Data: h'88070000820282010282028202018202
                                820201820018281a000f4240
                Payload Data: h'52656164792047656e65726174652061
                                2033322062797465207061796c6f6164'
              Payload Header: h'85010100005820'
                  BIB Header: h'850b0300005845'
           Payload Signature: h'07c84d929f83bee4690130729d77a1bd
                                da9611cd6598e73d0659073ea74e8c27
                                523b02193cb8ba64be58dbc556887aca

      Figure 24: Example 4: Configuration, Parameters, and Results for
                                  the BIB

A.4.3.2.  Abstract Security Block

   The abstract security block structure of the BIB's block-type-
   specific-data field for this application is as follows.

 [1],           / Security Target            - Payload block           /
 1,             / Security Context ID        - BIB-HMAC-SHA2           /
 1,             / Security Context Flags     - Parameters Present      /
 [2,[2, 1]],    / Security Source            - ipn:2.1                 /
 [              / Security Parameters        - 2 Parameters            /
    [1, 6],     / SHA Variant                - HMAC 384/384            /
    [3, 0x07]   / Scope Flags - All additional headers in the SHA Hash /
 ],
 [              / Security Results: 1 Result /
    [1, h'07c84d929f83bee4690130729d77a1bdda9611cd6598e73d
          0659073ea74e8c27523b02193cb8ba64be58dbc556887aca']
 ]

        Figure 25: Example 4: BIB Abstract Security Block (CBOR
                          Diagnostic Notation)

   The CBOR encoding of the BIB block-type-specific-data field (the
   abstract security block) is 0x810101018202820201828201068203078182015
   83007c84d929f83bee4690130729d77a1bdda9611cd6598e73d0659073ea74e8c2752
   3b02193cb8ba64be58dbc556887aca.
    */
    //add bib
    std::vector<uint8_t> expectedSecurityResultBib;
    static const std::string expectedSecurityResultBibString(
        "07c84d929f83bee4690130729d77a1bdda9611cd6598e73d"
        "0659073ea74e8c27523b02193cb8ba64be58dbc556887aca"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResultBibString, expectedSecurityResultBib));
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
        Bpv7BlockIntegrityBlock & bib = *(reinterpret_cast<Bpv7BlockIntegrityBlock*>(blockPtr.get()));
        //bib.SetZero();

        bib.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
        bib.m_blockNumber = 3;
        bib.m_crcType = BPV7_CRC_TYPE_NONE;
        bib.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 1 });
        //bib.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2); //handled by constructor
        bib.m_securityContextFlags = 0;
        bib.SetSecurityContextParametersPresent();
        bib.m_securitySource.Set(2, 1);
        BOOST_REQUIRE(bib.AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS::HMAC_384_384));
        BOOST_REQUIRE(bib.AddSecurityParameterIntegrityScope(
            BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_PRIMARY_BLOCK |
            BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_SECURITY_HEADER |
            BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_TARGET_HEADER
        ));

        
        std::vector<uint8_t> * result1Ptr = bib.AppendAndGetExpectedHmacPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = expectedSecurityResultBib; //use std::move in production



        bv.PrependMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(5000));
    }
    //get bib and verify
    Bpv7BlockIntegrityBlock* bibPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bibPtrOriginal = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bibPtrOriginal);
        BOOST_REQUIRE(*bibPtrOriginal == *bibPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBib;
        static const std::string expectedSerializedBibString(
            "810101018202820201828201068203078182015"
            "83007c84d929f83bee4690130729d77a1bdda9611cd6598e73d0659073ea74e8c2752"
            "3b02193cb8ba64be58dbc556887aca"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibString, expectedSerializedBib));
        std::vector<uint8_t> actualSerializedBib(bibPtrOriginal->m_dataPtr, bibPtrOriginal->m_dataPtr + bibPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBib == actualSerializedBib);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBib, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibString);


        /*
            A.4.3.3.  Representations

   The BIB wrapping this abstract security block is as follows.

   [
     11, / type code /
     3,  / block number /
     0,  / flags  /
     0,  / CRC type /
     h'81010101820282020182820106820307818201583007c84d929f83bee4690130
       729d77a1bdda9611cd6598e73d0659073ea74e8c27523b02193cb8ba64be58db
       c556887aca',
   ]

            Figure 26: Example 4: BIB (CBOR Diagnostic Notation)

   The CBOR encoding of the BIB block is 0x850b0300005845810101018202820
   20182820106820307818201583007c84d929f83bee4690130729d77a1bdda9611cd65
   98e73d0659073ea74e8c27523b02193cb8ba64be58dbc556887aca.
        */
        //verify whole bib block from example
        std::vector<uint8_t> expectedSerializedBibBlock;
        static const std::string expectedSerializedBibBlockString(
            "850b0300005845810101018202820"
            "20182820106820307818201583007c84d929f83bee4690130729d77a1bdda9611cd65"
            "98e73d0659073ea74e8c27523b02193cb8ba64be58dbc556887aca"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibBlockString, expectedSerializedBibBlock));
        uint8_t * ptrBibBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBibBlock(ptrBibBlockSerialized, ptrBibBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBibBlock == actualSerializedBibBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBibBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibBlockString);

    }

    /*
    A.4.4.  Bundle Confidentiality Block

   In this example, a BCB is used encrypt the payload block and the BIB
   that provides integrity over the payload.

A.4.4.1.  Configuration, Parameters, and Results

   For this example, the following configuration and security parameters
   are used to generate the security results indicated.

   This BCB has two targets: the payload block and BIB.  Four security
   results are generated: cipher text which replaces the plain text
   block-type-specific data of the payload block, cipher text to encrypt
   the BIB, and authentication tags for both the payload block and BIB.

                         Key: h'71776572747975696f70617364666768
                                71776572747975696f70617364666768'
                          IV: h'5477656c7665313231323132'
                 AES Variant: A256GCM
                 Scope Flags: 0x07  (All additional headers)
                Payload Data: h'52656164792047656e65726174652061
                                2033322062797465207061796c6f6164'
                    BIB Data: h'81010101820282020182820106820307
                                818201583007c84d929f83bee4690130
                                729d77a1bdda9611cd6598e73d065907
                                3ea74e8c27523b02193cb8ba64be58db
                                c556887aca
                         BIB
          Authentication Tag: h'c95ed4534769b046d716e1cdfd00830e'
               Payload Block
          Authentication Tag: h'0e365c700e4bb19c0d991faff5345aff'
          Payload Ciphertext: h'90eab64575930498d6aa654107f15e96
                                319bb227706000abc8fcac3b9bb9c87e'
              BIB Ciphertext: h'438ed6208eb1c1ffb94d952175167df0
                                902a815f221ebc837a134efc13bfa82a
                                2d5d317747da3eb54acef4ca839bd961
                                487284404259b60be12b8aed2f3e8a36
                                2836529f66'

      Figure 27: Example 4: Configuration, Parameters, and Results for
                                  the BCB

A.4.4.2.  Abstract Security Block

   The abstract security block structure of the BCB's block-type-
   specific-data field for this application is as follows.

   [3, 1],          / Security Targets                            /
   2,               / Security Context ID    - BCB-AES-GCM        /
   1,               / Security Context Flags - Parameters Present /
   [2,[2, 1]],      / Security Source        - ipn:2.1            /
   [                / Security Parameters    - 3 Parameters       /
     [1, h'5477656c7665313231323132'],    / Initialization Vector /
     [2, 3],                              / AES Variant - AES 256 /
     [4, 0x07]            / Scope Flags - All headers in SHA hash /
   ],
   [                                / Security Results: 2 Results /
     [1, h'c95ed4534769b046d716e1cdfd00830e'],    / BIB Auth. Tag /
     [1, h'0e365c700e4bb19c0d991faff5345aff'] / Payload Auth. Tag /
   ]

          Figure 28: Example 4: BCB Abstract Security Block (CBOR
                            Diagnostic Notation)

   The CBOR encoding of the BCB block-type-specific-data field (the
   abstract security block) is 0x820301020182028202018382014c5477656c766
   531323132313282020382040782820150c95ed4534769b046d716e1cdfd00830e8201
   500e365c700e4bb19c0d991faff5345aff.
    */
    //add bcb
    std::vector<uint8_t> expectedInitializationVector;
    static const std::string expectedInitializationVectorString(
        "5477656c7665313231323132"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));

    std::vector<uint8_t> expectedSecurityResult1BibAuthTag;
    static const std::string expectedSecurityResult1BibAuthTagString(
        "c95ed4534769b046d716e1cdfd00830e"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResult1BibAuthTagString, expectedSecurityResult1BibAuthTag));

    std::vector<uint8_t> expectedSecurityResult2PayloadAuthTag;
    static const std::string expectedSecurityResult2PayloadAuthTagString(
        "0e365c700e4bb19c0d991faff5345aff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSecurityResult2PayloadAuthTagString, expectedSecurityResult2PayloadAuthTag));
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
        Bpv7BlockConfidentialityBlock & bcb = *(reinterpret_cast<Bpv7BlockConfidentialityBlock*>(blockPtr.get()));

        bcb.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::MUST_BE_REPLICATED;
        bcb.m_blockNumber = 2;
        bcb.m_crcType = BPV7_CRC_TYPE_NONE;
        bcb.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 3, 1 });
        //bcb.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM); //handled by constructor
        bcb.m_securityContextFlags = 0;
        bcb.SetSecurityContextParametersPresent();
        bcb.m_securitySource.Set(2, 1);

        std::vector<uint8_t> * ivPtr = bcb.AddAndGetInitializationVectorPtr();
        BOOST_REQUIRE(ivPtr);
        *ivPtr = expectedInitializationVector; //use std::move in production

        BOOST_REQUIRE(bcb.AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS::A256GCM));

        BOOST_REQUIRE(bcb.AddSecurityParameterScope(
            BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_PRIMARY_BLOCK |
            BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_SECURITY_HEADER |
            BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_TARGET_HEADER
        ));

        std::vector<uint8_t> * result1Ptr = bcb.AppendAndGetPayloadAuthenticationTagPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = expectedSecurityResult1BibAuthTag; //use std::move in production

        std::vector<uint8_t> * result2Ptr = bcb.AppendAndGetPayloadAuthenticationTagPtr();
        BOOST_REQUIRE(result2Ptr);
        *result2Ptr = expectedSecurityResult2PayloadAuthTag; //use std::move in production



        bv.InsertMoveCanonicalBlockAfterBlockNumber(blockPtr, 3); //insert after first bib which is block number 3 to match example
        BOOST_REQUIRE(bv.Render(5000));
    }
    //get bcb and verify
    Bpv7BlockConfidentialityBlock* bcbPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bcbPtrOriginal = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bcbPtrOriginal);
        BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBcb;
        static const std::string expectedSerializedBcbString(
            "820301020182028202018382014c5477656c766"
            "531323132313282020382040782820150c95ed4534769b046d716e1cdfd00830e8201"
            "500e365c700e4bb19c0d991faff5345aff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbString, expectedSerializedBcb));
        std::vector<uint8_t> actualSerializedBcb(bcbPtrOriginal->m_dataPtr, bcbPtrOriginal->m_dataPtr + bcbPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBcb == actualSerializedBcb);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBcb, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbString);

        /*
        A.4.4.3.  Representations

   The BCB wrapping this abstract security block is as follows.

   [
     12, / type code /
     2,  / block number /
     1,  / flags - block must be replicated in every fragment /
     0,  / CRC type /
     h'820301020182028202018382014c5477656c7665313231323132820203820407
       82820150c95ed4534769b046d716e1cdfd00830e8201500e365c700e4bb19c0d
       991faff5345aff',
   ]

            Figure 29: Example 4: BCB (CBOR Diagnostic Notation)

   The CBOR encoding of the BCB block is 0x850c0201005847820301020182028
   202018382014c5477656c766531323132313282020382040782820150c95ed4534769
   b046d716e1cdfd00830e8201500e365c700e4bb19c0d991faff5345aff.
        */
        //verify from example
        std::vector<uint8_t> expectedSerializedBcbBlock;
        static const std::string expectedSerializedBcbBlockString(
            "850c0201005847820301020182028"
            "202018382014c5477656c766531323132313282020382040782820150c95ed4534769"
            "b046d716e1cdfd00830e8201500e365c700e4bb19c0d991faff5345aff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbBlockString, expectedSerializedBcbBlock));
        uint8_t * ptrBcbBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBcbBlock(ptrBcbBlockSerialized, ptrBcbBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBcbBlock == actualSerializedBcbBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBcbBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbBlockString);
    }

    /*
    A.4.5.  Final Bundle

   The CBOR encoding of the full output bundle, with the security blocks
   added and payload block and BIB encrypted is: 0x9f8807000082028201028
   2028202018202820201820018281a000f4240850b0300005845438ed6208eb1c1ffb9
   4d952175167df0902a815f221ebc837a134efc13bfa82a2d5d317747da3eb54acef4c
   a839bd961487284404259b60be12b8aed2f3e8a362836529f66 850c0201005847820
   301020182028202018382014c5477656c766531323132313282020382040782820150
   c95ed4534769b046d716e1cdfd00830e8201500e365c700e4bb19c0d991faff5345af
   f8501010000582090eab64575930498d6aa654107f15e96319bb227706000abc8fcac
   3b9bb9c87eff.

   Payload Ciphertext: h'90eab64575930498d6aa654107f15e96
                                319bb227706000abc8fcac3b9bb9c87e'
              BIB Ciphertext: h'438ed6208eb1c1ffb94d952175167df0
                                902a815f221ebc837a134efc13bfa82a
                                2d5d317747da3eb54acef4ca839bd961
                                487284404259b60be12b8aed2f3e8a36
                                2836529f66'
    */
    //"encrypt" payload
    static const std::string payloadCipherTextString(
        "90eab64575930498d6aa654107f15e96319bb227706000abc8fcac3b9bb9c87e"
    );
    std::vector<uint8_t> payloadCipherText;
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(payloadCipherTextString, payloadCipherText));
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        blocks[0]->headerPtr->m_dataPtr = payloadCipherText.data();
        blocks[0]->headerPtr->m_dataLength = payloadCipherText.size();
        blocks[0]->SetManuallyModified();
    }

    //"encrypt" bib
    static const std::string bibCipherTextString(
        "438ed6208eb1c1ffb94d952175167df0902a815f221ebc837a134efc13bfa82a2d5d317747da3eb54acef4ca839bd961487284404259b60be12b8aed2f3e8a362836529f66"
    );
    std::vector<uint8_t> bibCipherText;
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(bibCipherTextString, bibCipherText));
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        blocks[0]->headerPtr->m_dataPtr = bibCipherText.data();
        blocks[0]->headerPtr->m_dataLength = bibCipherText.size();
        blocks[0]->SetManuallyModified();
        blocks[0]->isEncrypted = true;
        BOOST_REQUIRE(bv.Render(5000));

        //verify from example
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f8807000082028201028"
            "2028202018202820201820018281a000f4240850b0300005845438ed6208eb1c1ffb9"
            "4d952175167df0902a815f221ebc837a134efc13bfa82a2d5d317747da3eb54acef4c"
            "a839bd961487284404259b60be12b8aed2f3e8a362836529f66850c0201005847820"
            "301020182028202018382014c5477656c766531323132313282020382040782820150"
            "c95ed4534769b046d716e1cdfd00830e8201500e365c700e4bb19c0d991faff5345af"
            "f8501010000582090eab64575930498d6aa654107f15e96319bb227706000abc8fcac"
            "3b9bb9c87eff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
        
        //load bundle to test deserialize
        {
            BundleViewV7 bv2;
            std::vector<uint8_t> toSwapIn(expectedSerializedBundle);
            BOOST_REQUIRE(bv2.SwapInAndLoadBundle(toSwapIn)); //swap in a copy
            //get bcb and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockConfidentialityBlock* bcbPtr2 = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bcbPtr2);
                BOOST_REQUIRE(!blocks2[0]->isEncrypted); //bcb not encrypted
                BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtr2);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockNumber, 2);
                std::vector<std::vector<uint8_t>*> patPtrs = bcbPtr2->GetAllPayloadAuthenticationTagPtrs();
                BOOST_REQUIRE_EQUAL(patPtrs.size(), 2);
                BOOST_REQUIRE(expectedSecurityResult1BibAuthTag == *(patPtrs[0]));
                BOOST_REQUIRE(expectedSecurityResult2PayloadAuthTag == *(patPtrs[1]));
                

                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }

            //get bib and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockIntegrityBlock* bibPtr2 = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bibPtr2);
                BOOST_REQUIRE(blocks2[0]->isEncrypted); //this bib is encrypted
                //BOOST_REQUIRE(*bibPtrOriginal == *bibPtr2);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::INTEGRITY);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockNumber, 3);
                //std::vector<std::vector<uint8_t>*> hmacPtrs = bibPtr2->GetAllExpectedHmacPtrs();
                //BOOST_REQUIRE_EQUAL(hmacPtrs.size(), 1);
                //BOOST_REQUIRE(expectedSecurityResultBib == *(hmacPtrs[0]));

                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }
        }
    }
}

