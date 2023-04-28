/**
 * @file Bpv7BpSecExtensionBlocks.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "codec/bpv7.h"
#include "codec/Bpv7Crc.h"
#include "CborUint.h"
#include <boost/format.hpp>
#include <boost/make_unique.hpp>

/////////////////////////////////////////
// ABSTRACT SECURITY (EXTENSION) BLOCK
/////////////////////////////////////////

Bpv7AbstractSecurityBlock::Bpv7AbstractSecurityBlock() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    //m_blockTypeCode = ???;
}
Bpv7AbstractSecurityBlock::~Bpv7AbstractSecurityBlock() { } //a destructor: ~X()
Bpv7AbstractSecurityBlock::Bpv7AbstractSecurityBlock(Bpv7AbstractSecurityBlock&& o) :
    Bpv7CanonicalBlock(std::move(o)),
    m_securityTargets(std::move(o.m_securityTargets)),
    m_securityContextId(o.m_securityContextId),
    m_securityContextFlags(o.m_securityContextFlags),
    m_securitySource(std::move(o.m_securitySource)),
    m_securityContextParametersOptional(std::move(o.m_securityContextParametersOptional)),
    m_securityResults(std::move(o.m_securityResults)) { } //a move constructor: X(X&&)
Bpv7AbstractSecurityBlock& Bpv7AbstractSecurityBlock::operator=(Bpv7AbstractSecurityBlock && o) { //a move assignment: operator=(X&&)
    m_securityTargets = std::move(o.m_securityTargets);
    m_securityContextId = o.m_securityContextId;
    m_securityContextFlags = o.m_securityContextFlags;
    m_securitySource = std::move(o.m_securitySource);
    m_securityContextParametersOptional = std::move(o.m_securityContextParametersOptional);
    m_securityResults = std::move(o.m_securityResults);
    return static_cast<Bpv7AbstractSecurityBlock&>(Bpv7CanonicalBlock::operator=(std::move(o)));
}
bool Bpv7AbstractSecurityBlock::operator==(const Bpv7AbstractSecurityBlock & o) const {
    const bool initialTest = (m_securityTargets == o.m_securityTargets)
        && (m_securityContextId == o.m_securityContextId)
        && (m_securityContextFlags == o.m_securityContextFlags)
        && (m_securitySource == o.m_securitySource)
        //&& ((IsCipherSuiteParametersPresent()) ? (m_cipherSuiteParametersOptional == o.m_cipherSuiteParametersOptional) : true)
        //&& (m_securityResults == o.m_securityResults)
        && Bpv7CanonicalBlock::operator==(o);
    if (!initialTest) {
        return false;
    }
    if (IsSecurityContextParametersPresent()) {
        if (!Bpv7AbstractSecurityBlock::IsEqual(m_securityContextParametersOptional, o.m_securityContextParametersOptional)) {
            return false;
        }
    }
    return Bpv7AbstractSecurityBlock::IsEqual(m_securityResults, o.m_securityResults);
}
bool Bpv7AbstractSecurityBlock::operator!=(const Bpv7AbstractSecurityBlock & o) const {
    return !(*this == o);
}
void Bpv7AbstractSecurityBlock::SetZero() {
    Bpv7CanonicalBlock::SetZero();
    m_securityTargets.clear();
    m_securityContextId = 0;
    m_securityContextFlags = 0;
    m_securitySource.SetZero();
    m_securityContextParametersOptional.clear();
    m_securityResults.clear();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PRIMARY_IMPLICIT_ZERO; //??
}


//Security Context Flags:
//This field identifies which optional fields are present in the
//security block.  This field SHALL be represented as a CBOR
//unsigned integer whose contents shall be interpreted as a bit
//field.  Each bit in this bit field indicates the presence (bit
//set to 1) or absence (bit set to 0) of optional data in the
//security block.  The association of bits to security block data
//is defined as follows.
//
//Bit 0  (the least-significant bit, 0x01): Security Context
//    Parameters Present Flag.
//
//Bit >0 Reserved
//
//Implementations MUST set reserved bits to 0 when writing this
//field and MUST ignore the values of reserved bits when reading
//this field.  For unreserved bits, a value of 1 indicates that
//the associated security block field MUST be included in the
//security block.  A value of 0 indicates that the associated
//security block field MUST NOT be in the security block.
bool Bpv7AbstractSecurityBlock::IsSecurityContextParametersPresent() const {
    return ((m_securityContextFlags & 0x1) != 0);
}
void Bpv7AbstractSecurityBlock::SetSecurityContextParametersPresent() {
    m_securityContextFlags |= 0x1;
}
void Bpv7AbstractSecurityBlock::ClearSecurityContextParametersPresent() {
    m_securityContextFlags &= ~(static_cast<uint8_t>(0x1));
}

uint64_t Bpv7AbstractSecurityBlock::SerializeBpv7(uint8_t * serialization) {
    //m_blockTypeCode = ???
    m_dataPtr = NULL;
    m_dataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv7CanonicalBlock::SerializeBpv7(serialization);
    uint64_t bufferSize = m_dataLength;
    uint8_t * blockSpecificDataSerialization = m_dataPtr;
    uint64_t thisSerializationSize;

    //The fields of the ASB SHALL be as follows, listed in the order in
    //which they must appear.  The encoding of these fields MUST be in
    //accordance with the canonical forms provided in Section 4.

    
    //Security Targets:
    //This field identifies the block(s) targeted by the security
    //operation(s) represented by this security block.  Each target
    //block is represented by its unique Block Number.  This field
    //SHALL be represented by a CBOR array of data items.  Each
    //target within this CBOR array SHALL be represented by a CBOR
    //unsigned integer.  This array MUST have at least 1 entry and
    //each entry MUST represent the Block Number of a block that
    //exists in the bundle.  There MUST NOT be duplicate entries in
    //this array.  The order of elements in this list has no semantic
    //meaning outside of the context of this block.  Within the
    //block, the ordering of targets must match the ordering of
    //results associated with these targets.
    thisSerializationSize = CborArbitrarySizeUint64ArraySerialize(blockSpecificDataSerialization, m_securityTargets, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;


    //Security Context Id:
    //This field identifies the security context used to implement
    //the security service represented by this block and applied to
    //each security target.  This field SHALL be represented by a
    //CBOR unsigned integer.  The values for this Id should come from
    //the registry defined in Section 11.3
    thisSerializationSize = CborEncodeU64(blockSpecificDataSerialization, m_securityContextId, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //Security Context Flags:
    //This field identifies which optional fields are present in the
    //security block.  This field SHALL be represented as a CBOR
    //unsigned integer whose contents shall be interpreted as a bit
    //field.  Each bit in this bit field indicates the presence (bit
    //set to 1) or absence (bit set to 0) of optional data in the
    //security block.  The association of bits to security block data
    //is defined as follows.
    //
    //Bit 0  (the least-significant bit, 0x01): Security Context
    //    Parameters Present Flag.
    //
    //Bit >0 Reserved
    //
    //Implementations MUST set reserved bits to 0 when writing this
    //field and MUST ignore the values of reserved bits when reading
    //this field.  For unreserved bits, a value of 1 indicates that
    //the associated security block field MUST be included in the
    //security block.  A value of 0 indicates that the associated
    //security block field MUST NOT be in the security block.
    thisSerializationSize = CborEncodeU64(blockSpecificDataSerialization, m_securityContextFlags, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //Security Source:
    //This field identifies the Endpoint that inserted the security
    //block in the bundle.  This field SHALL be represented by a CBOR
    //array in accordance with [I-D.ietf-dtn-bpbis] rules for
    //representing Endpoint Identifiers (EIDs).
    thisSerializationSize = m_securitySource.SerializeBpv7(blockSpecificDataSerialization, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //Security Context Parameters (Optional):
    //This field captures one or more security context parameters
    //that should be used when processing the security service
    //described by this security block.  This field SHALL be
    //represented by a CBOR array.  Each entry in this array is a
    //single security context parameter.  A single parameter SHALL
    //also be represented as a CBOR array comprising a 2-tuple of the
    //id and value of the parameter, as follows.
    //
    //*  Parameter Id.  This field identifies which parameter is
    //   being specified.  This field SHALL be represented as a CBOR
    //   unsigned integer.  Parameter Ids are selected as described
    //   in Section 3.10.
    //
    //*  Parameter Value.  This field captures the value associated
    //   with this parameter.  This field SHALL be represented by the
    //   applicable CBOR representation of the parameter, in
    //   accordance with Section 3.10.
    if (IsSecurityContextParametersPresent()) {
        thisSerializationSize = SerializeIdValuePairsVecBpv7(blockSpecificDataSerialization, m_securityContextParametersOptional, bufferSize, false);
        blockSpecificDataSerialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }

    //Security Results:
    //This field captures the results of applying a security service
    //to the security targets of the security block.  This field
    //SHALL be represented as a CBOR array of target results.  Each
    //entry in this array represents the set of security results for
    //a specific security target.  The target results MUST be ordered
    //identically to the Security Targets field of the security
    //block.  This means that the first set of target results in this
    //array corresponds to the first entry in the Security Targets
    //field of the security block, and so on.  There MUST be one
    //entry in this array for each entry in the Security Targets
    //field of the security block.
    //
    //The set of security results for a target is also represented as
    //a CBOR array of individual results.  An individual result is
    /////////////BEGIN OLD DRAFT!!!!!/////////////////////////////
    // represented as a 2-tuple of a result id and a result value,
    /////////////END OLD DRAFT!!!!!!!
    /////////////BEGIN CURRENT RFC!!!!/////////////////////////////
    //  represented as a CBOR array comprising a 2-tuple of a result Id and a result value
    /////////////END CURRENT RFC!!!!/////////////////////////////
    //, defined as follows.
    //
    //*  Result Id.  This field identifies which security result is
    //   being specified.  Some security results capture the primary
    //   output of a cipher suite.  Other security results contain
    //   additional annotative information from cipher suite
    //   processing.  This field SHALL be represented as a CBOR
    //   unsigned integer.  Security result Ids will be as specified
    //   in Section 3.10.
    //
    //*  Result Value.  This field captures the value associated with
    //   the result.  This field SHALL be represented by the
    //   applicable CBOR representation of the result value, in
    //   accordance with Section 3.10.
    thisSerializationSize = SerializeIdValuePairsVecBpv7(blockSpecificDataSerialization, m_securityResults, bufferSize, true);
    blockSpecificDataSerialization += thisSerializationSize;
    
    RecomputeCrcAfterDataModification(serialization, serializationSizeCanonical);
    return serializationSizeCanonical;
}
uint64_t Bpv7AbstractSecurityBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    uint64_t serializationSize = CborArbitrarySizeUint64ArraySerializationSize(m_securityTargets);
    serializationSize += CborGetEncodingSizeU64(m_securityContextId);
    serializationSize += CborGetEncodingSizeU64(m_securityContextFlags);
    serializationSize += m_securitySource.GetSerializationSizeBpv7();
    if (IsSecurityContextParametersPresent()) {
        serializationSize += IdValuePairsVecBpv7SerializationSize(m_securityContextParametersOptional, false);
    }
    serializationSize += IdValuePairsVecBpv7SerializationSize(m_securityResults, true);
    return serializationSize;
}

bool Bpv7AbstractSecurityBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    static constexpr uint64_t maxElements = 1000; //todo
    uint8_t numBytesTakenToDecode;
    if (m_dataPtr == NULL) {
        return false;
    }

    uint8_t * serialization = m_dataPtr;
    uint64_t bufferSize = m_dataLength;
    uint8_t cborUintSizeDecoded;
    uint64_t tmpNumBytesTakenToDecode64;
    if (!CborArbitrarySizeUint64ArrayDeserialize(serialization, tmpNumBytesTakenToDecode64, bufferSize, m_securityTargets, maxElements)) {
        return false; //failure
    }
    serialization += tmpNumBytesTakenToDecode64;
    bufferSize -= tmpNumBytesTakenToDecode64;

    m_securityContextId = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
    if (cborUintSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    const uint64_t tmpCipherSuiteFlags64 = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
    if (cborUintSizeDecoded == 0) {
        return false; //failure
    }
    if(tmpCipherSuiteFlags64 > 0x1f) {
        return false; //failure
    }
    m_securityContextFlags = static_cast<uint8_t>(tmpCipherSuiteFlags64);
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    if(!m_securitySource.DeserializeBpv7(serialization, &numBytesTakenToDecode, m_dataLength)) {
        return false; //failure
    }
    serialization += numBytesTakenToDecode;
    bufferSize -= numBytesTakenToDecode;

    if (IsSecurityContextParametersPresent()) {
        if(!DeserializeIdValuePairsVecBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, m_securityContextParametersOptional,
            static_cast<BPSEC_SECURITY_CONTEXT_IDENTIFIERS>(m_securityContextId), true, maxElements, false))
        {
            return false; //failure
        }
        serialization += tmpNumBytesTakenToDecode64;
        bufferSize -= tmpNumBytesTakenToDecode64;
    }

    if (!DeserializeIdValuePairsVecBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, m_securityResults,
        static_cast<BPSEC_SECURITY_CONTEXT_IDENTIFIERS>(m_securityContextId), false, maxElements, true))
    {
        return false; //failure
    }
    serialization += tmpNumBytesTakenToDecode64;
    bufferSize -= tmpNumBytesTakenToDecode64;

    return (bufferSize == 0);
}

//static helpers
//This field SHALL be represented by a CBOR array. Each entry in this array is also a CBOR array
//comprising a 2-tuple of the id and value, as follows.
//    Id. This field SHALL be represented as a CBOR unsigned integer.
//    Value. This field SHALL be represented by the applicable CBOR representation (treate as already raw cbor serialization here)
uint64_t Bpv7AbstractSecurityBlock::SerializeIdValuePairsVecBpv7(uint8_t * serialization,
    const id_value_pairs_vec_t & idValuePairsVec, uint64_t bufferSize, bool encapsulatePairInArrayOfSizeOne)
{
    uint8_t * const serializationBase = serialization;
    uint64_t thisSerializationSize;
    uint8_t * const arrayHeaderStartPtr = serialization;
    thisSerializationSize = CborEncodeU64(serialization, idValuePairsVec.size(), bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    *arrayHeaderStartPtr |= (4U << 5); //change from major type 0 (unsigned integer) to major type 4 (array)

    for (std::size_t i = 0; i < idValuePairsVec.size(); ++i) {
        if (encapsulatePairInArrayOfSizeOne) {
            ////CURRENT RFC puts the "individual (security) result" in an array of size 1:
            *serialization++ = (4U << 5) | 1; //major type 4, additional information 1 (add cbor array of size 1)
            --bufferSize;
        }
        //now the 2-tuple of a result Id and a result value
        const id_value_pair_t & idValuePair = idValuePairsVec[i];
        if (bufferSize == 0) {
            return 0;
        }
        *serialization++ = (4U << 5) | 2; //major type 4, additional information 2 (add cbor array of size 2)
        --bufferSize;

        thisSerializationSize = CborEncodeU64(serialization, idValuePair.first, bufferSize); //id
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        thisSerializationSize = idValuePair.second->SerializeBpv7(serialization, bufferSize); //value
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    return serialization - serializationBase;
}
uint64_t Bpv7AbstractSecurityBlock::IdValuePairsVecBpv7SerializationSize(const id_value_pairs_vec_t & idValuePairsVec, bool encapsulatePairInArrayOfSizeOne) {
    uint64_t serializationSize = CborGetEncodingSizeU64(idValuePairsVec.size());
    serializationSize += idValuePairsVec.size(); //for major type 4, additional information 2 (cbor array of size 2) headers within the following loop
    for (std::size_t i = 0; i < idValuePairsVec.size(); ++i) {
        serializationSize += encapsulatePairInArrayOfSizeOne; ////CURRENT RFC puts the "individual result" in an array of size 1:
        const id_value_pair_t & idValuePair = idValuePairsVec[i];
        serializationSize += CborGetEncodingSizeU64(idValuePair.first);
        serializationSize += idValuePair.second->GetSerializationSize(); //value
    }
    return serializationSize;
}
bool Bpv7AbstractSecurityBlock::DeserializeIdValuePairsVecBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize,
    id_value_pairs_vec_t & idValuePairsVec, const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext,
    const bool isForSecurityParameters, const uint64_t maxElements, bool pairIsEncapsulateInArrayOfSizeOne)
{
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) {
        return false;
    }
    const uint8_t initialCborByte = *serialization; //buffer size verified above
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        //An implementation of the Bundle Protocol MAY accept a sequence of
        //bytes that does not conform to the Bundle Protocol specification
        //(e.g., one that represents data elements in fixed-length arrays
        //rather than indefinite-length arrays) and transform it into
        //conformant BP structure before processing it.
        ++serialization;
        --bufferSize;
        idValuePairsVec.resize(0);
        idValuePairsVec.reserve(maxElements);
        for (std::size_t i = 0; i < maxElements; ++i) {
            if (pairIsEncapsulateInArrayOfSizeOne) {
                ////CURRENT RFC puts the "individual security result" in an array of size 1:
                if (*serialization++ != ((4U << 5) | 1)) { //major type 4, additional information 1 (add cbor array of size 1)
                    return false;
                }
                --bufferSize;
                //now the 2-tuple of a result Id and a result value
            }
            idValuePairsVec.emplace_back();
            id_value_pair_t & idValuePair = idValuePairsVec.back();
            uint64_t tmpNumBytesTakenToDecode64;
            if (!DeserializeIdValuePairBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, idValuePair, securityContext, isForSecurityParameters)) {
                return false; //failure
            }
            serialization += tmpNumBytesTakenToDecode64;
            bufferSize -= tmpNumBytesTakenToDecode64;
        }
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }
    else {
        uint8_t * const arrayHeaderStartPtr = serialization; //buffer size verified above
        const uint8_t cborMajorTypeArray = (*arrayHeaderStartPtr) >> 5;
        if (cborMajorTypeArray != 4) {
            return false; //failure
        }
        *arrayHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
        uint8_t cborUintSizeDecoded;
        const uint64_t numElements = CborDecodeU64(arrayHeaderStartPtr, &cborUintSizeDecoded, bufferSize);
        *arrayHeaderStartPtr |= (4U << 5); // restore to major type to 4 (change from major type 0 (unsigned integer) to major type 4 (array))
        if (cborUintSizeDecoded == 0) {
            return false; //failure
        }
        if (numElements > maxElements) {
            return false; //failure
        }
        serialization += cborUintSizeDecoded;
        bufferSize -= cborUintSizeDecoded;

        idValuePairsVec.resize(numElements);
        for (std::size_t i = 0; i < numElements; ++i) {
            if (pairIsEncapsulateInArrayOfSizeOne) {
                ////CURRENT RFC puts the "individual security result" in an array of size 1:
                if (*serialization++ != ((4U << 5) | 1)) { //major type 4, additional information 1 (add cbor array of size 1)
                    return false;
                }
                --bufferSize;
                //now the 2-tuple of a result Id and a result value
            }
            id_value_pair_t & idValuePair = idValuePairsVec[i];
            uint64_t tmpNumBytesTakenToDecode64;
            if (!DeserializeIdValuePairBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize, idValuePair, securityContext, isForSecurityParameters)) {
                return false; //failure
            }
            serialization += tmpNumBytesTakenToDecode64;
            bufferSize -= tmpNumBytesTakenToDecode64;
        }
    }

    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}

bool Bpv7AbstractSecurityBlock::DeserializeIdValuePairBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize,
    id_value_pair_t & idValuePair, const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters) {
    uint8_t cborUintSize;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) {
        return false;
    }
    --bufferSize;
    const uint8_t initialCborByte = *serialization++;
    if ((initialCborByte != ((4U << 5) | 2U)) && //major type 4, additional information 2 (array of length 2)
        (initialCborByte != ((4U << 5) | 31U))) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }

    idValuePair.first = CborDecodeU64(serialization, &cborUintSize, bufferSize);
    if (cborUintSize == 0) {
        return false; //failure
    }
    serialization += cborUintSize;
    bufferSize -= cborUintSize;

    uint64_t tmpNumBytesTakenToDecode64;
    if (isForSecurityParameters) {
        switch (securityContext) {
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2:
                switch (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(idValuePair.first)) {
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT:
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
                        break;
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::WRAPPED_KEY:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM:
                switch (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(idValuePair.first)) {
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT:
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
                        break;
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::INITIALIZATION_VECTOR:
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::WRAPPED_KEY:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            default:
                return false;
        }
    }
    else {
        switch (securityContext) {
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2:
                switch (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS>(idValuePair.first)) {
                    case BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS::EXPECTED_HMAC:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            case BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM:
                switch (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS>(idValuePair.first)) {
                    case BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS::AUTHENTICATION_TAG:
                        idValuePair.second = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
                        break;
                    default:
                        return false;
                }
                break;
            default:
                return false;
        }
    }
    if(!idValuePair.second->DeserializeBpv7(serialization, tmpNumBytesTakenToDecode64, bufferSize)) {
        return false; //failure
    }
    serialization += tmpNumBytesTakenToDecode64;
    

    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        bufferSize -= tmpNumBytesTakenToDecode64; //from value above
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }

    numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}
bool Bpv7AbstractSecurityBlock::IsEqual(const id_value_pairs_vec_t & pVec1, const id_value_pairs_vec_t & pVec2) {
    if (pVec1.size() != pVec2.size()) {
        return false;
    }
    for (std::size_t i = 0; i < pVec1.size(); ++i) {
        const id_value_pair_t & p1 = pVec1[i];
        const id_value_pair_t & p2 = pVec2[i];
        if (p1.first != p2.first) {
            return false;
        }
        if ((p1.second) && (p2.second)) { //both not null
            if (!p1.second->IsEqual(p2.second.get())) {
                return false;
            }
        }
        else if ((!p1.second) && (!p2.second)) { //both null
            continue;
        }
        else {
            return false;
        }
    }
    return true;
}
void Bpv7AbstractSecurityBlock::SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS id) {
    m_securityContextId = static_cast<security_context_id_t>(id);
}
std::vector<uint8_t> * Bpv7AbstractSecurityBlock::Protected_AppendAndGetSecurityResultByteStringPtr(uint64_t resultType) {
    //add it
    m_securityResults.emplace_back();
    m_securityResults.back().first = resultType;
    std::unique_ptr<Bpv7AbstractSecurityBlockValueByteString> v = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
    std::vector<uint8_t> * retVal = &(v->m_byteString);
    m_securityResults.back().second = std::move(v);
    return retVal;
}
std::vector<std::vector<uint8_t>*> Bpv7AbstractSecurityBlock::Protected_GetAllSecurityResultsByteStringPtrs(uint64_t resultType) {
    std::vector<std::vector<uint8_t>*> ptrs;
    for (std::size_t i = 0; i < m_securityResults.size(); ++i) {
        security_result_t & res = m_securityResults[i];
        if (res.first == resultType) {
            if (Bpv7AbstractSecurityBlockValueByteString * valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(res.second.get())) {
                ptrs.push_back(&(valueByteString->m_byteString));
            }
        }
    }
    return ptrs;
}

/////////////////////////////////////////
// BLOCK INTEGRITY BLOCK
/////////////////////////////////////////

Bpv7BlockIntegrityBlock::Bpv7BlockIntegrityBlock() : Bpv7AbstractSecurityBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::INTEGRITY;
    SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2);
}
Bpv7BlockIntegrityBlock::~Bpv7BlockIntegrityBlock() { } //a destructor: ~X()
Bpv7BlockIntegrityBlock::Bpv7BlockIntegrityBlock(Bpv7BlockIntegrityBlock&& o) :
    Bpv7AbstractSecurityBlock(std::move(o)) { } //a move constructor: X(X&&)
Bpv7BlockIntegrityBlock& Bpv7BlockIntegrityBlock::operator=(Bpv7BlockIntegrityBlock && o) { //a move assignment: operator=(X&&)
    return static_cast<Bpv7BlockIntegrityBlock&>(Bpv7AbstractSecurityBlock::operator=(std::move(o)));
}
bool Bpv7BlockIntegrityBlock::operator==(const Bpv7BlockIntegrityBlock & o) const {
    return Bpv7AbstractSecurityBlock::operator==(o);
}
bool Bpv7BlockIntegrityBlock::operator!=(const Bpv7BlockIntegrityBlock & o) const {
    return !(*this == o);
}
void Bpv7BlockIntegrityBlock::SetZero() {
    Bpv7AbstractSecurityBlock::SetZero();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::INTEGRITY;
}
bool Bpv7BlockIntegrityBlock::AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS alg) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue = static_cast<uint64_t>(alg);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(alg);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
COSE_ALGORITHMS Bpv7BlockIntegrityBlock::GetSecurityParameterShaVariant(bool & success) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::SHA_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                success = true;
                return static_cast<COSE_ALGORITHMS>(valueUint->m_uintValue);
            }
            else {
                success = false;
                return static_cast<COSE_ALGORITHMS>(0);
            }
        }
    }
    success = false;
    return static_cast<COSE_ALGORITHMS>(0);
}
bool Bpv7BlockIntegrityBlock::AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue |= static_cast<uint64_t>(integrityScope);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(integrityScope);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
bool Bpv7BlockIntegrityBlock::IsSecurityParameterIntegrityScopePresentAndSet(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::INTEGRITY_SCOPE_FLAGS) {
            if (const Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                return (valueUint->m_uintValue & static_cast<uint64_t>(integrityScope)) == static_cast<uint64_t>(integrityScope);
            }
            else {
                return false;
            }
        }
    }
    return false;
}
std::vector<uint8_t> * Bpv7BlockIntegrityBlock::AddAndGetWrappedKeyPtr() {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS>(param.first) == BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::WRAPPED_KEY) {
            if (Bpv7AbstractSecurityBlockValueByteString * valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(param.second.get())) {
                return &(valueByteString->m_byteString);
            }
            else {
                return NULL;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS::WRAPPED_KEY);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueByteString> v = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
    std::vector<uint8_t> * retVal = &(v->m_byteString);
    m_securityContextParametersOptional.back().second = std::move(v);
    return retVal;
}
std::vector<uint8_t> * Bpv7BlockIntegrityBlock::AppendAndGetExpectedHmacPtr() {
    return Protected_AppendAndGetSecurityResultByteStringPtr(static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS::EXPECTED_HMAC));
}
std::vector<std::vector<uint8_t>*> Bpv7BlockIntegrityBlock::GetAllExpectedHmacPtrs() {
    return Protected_GetAllSecurityResultsByteStringPtrs(static_cast<uint64_t>(BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS::EXPECTED_HMAC));
}

/////////////////////////////////////////
// BLOCK CONFIDENTIALITY BLOCK
/////////////////////////////////////////

Bpv7BlockConfidentialityBlock::Bpv7BlockConfidentialityBlock() : Bpv7AbstractSecurityBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY;
    SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM);
}
Bpv7BlockConfidentialityBlock::~Bpv7BlockConfidentialityBlock() { } //a destructor: ~X()
Bpv7BlockConfidentialityBlock::Bpv7BlockConfidentialityBlock(Bpv7BlockConfidentialityBlock&& o) :
    Bpv7AbstractSecurityBlock(std::move(o)) { } //a move constructor: X(X&&)
Bpv7BlockConfidentialityBlock& Bpv7BlockConfidentialityBlock::operator=(Bpv7BlockConfidentialityBlock && o) { //a move assignment: operator=(X&&)
    return static_cast<Bpv7BlockConfidentialityBlock&>(Bpv7AbstractSecurityBlock::operator=(std::move(o)));
}
bool Bpv7BlockConfidentialityBlock::operator==(const Bpv7BlockConfidentialityBlock & o) const {
    return Bpv7AbstractSecurityBlock::operator==(o);
}
bool Bpv7BlockConfidentialityBlock::operator!=(const Bpv7BlockConfidentialityBlock & o) const {
    return !(*this == o);
}
void Bpv7BlockConfidentialityBlock::SetZero() {
    Bpv7AbstractSecurityBlock::SetZero();
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY;
}

bool Bpv7BlockConfidentialityBlock::AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS alg) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue = static_cast<uint64_t>(alg);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(alg);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
COSE_ALGORITHMS Bpv7BlockConfidentialityBlock::GetSecurityParameterAesVariant(bool & success) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AES_VARIANT) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                success = true;
                return static_cast<COSE_ALGORITHMS>(valueUint->m_uintValue);
            }
            else {
                success = false;
                return static_cast<COSE_ALGORITHMS>(0);
            }
        }
    }
    success = false;
    return static_cast<COSE_ALGORITHMS>(0);
}
bool Bpv7BlockConfidentialityBlock::AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS) {
            if (Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                valueUint->m_uintValue |= static_cast<uint64_t>(scope);
                return true;
            }
            else {
                return false;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueUint> v = boost::make_unique<Bpv7AbstractSecurityBlockValueUint>();
    v->m_uintValue = static_cast<uint64_t>(scope);
    m_securityContextParametersOptional.back().second = std::move(v);
    return true;
}
bool Bpv7BlockConfidentialityBlock::IsSecurityParameterScopePresentAndSet(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope) const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS) {
            if (const Bpv7AbstractSecurityBlockValueUint * valueUint = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                return (valueUint->m_uintValue & static_cast<uint64_t>(scope)) == static_cast<uint64_t>(scope);
            }
            else {
                return false;
            }
        }
    }
    return false;
}
BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS Bpv7BlockConfidentialityBlock::GetSecurityParameterScope() const {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        const security_context_parameter_t& param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::AAD_SCOPE_FLAGS) {
            if (const Bpv7AbstractSecurityBlockValueUint* valueUint = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(param.second.get())) {
                return static_cast<BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS>(valueUint->m_uintValue);
            }
            else {
                //4.3.4.  AAD Scope Flags
                //When not provided, implementations SHOULD assume a value of 7
                //(indicating all assigned fields), unless an alternate default is
                //established by local security policy at the security source,
                //verifier, or acceptor of this integrity service.
                return BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::ALL_FLAGS_SET;
            }
        }
    }
    //4.3.4.  AAD Scope Flags
    //When not provided, implementations SHOULD assume a value of 7
    //(indicating all assigned fields), unless an alternate default is
    //established by local security policy at the security source,
    //verifier, or acceptor of this integrity service.
    return BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::ALL_FLAGS_SET;
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::AddAndGetAesWrappedKeyPtr() {
    return Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::WRAPPED_KEY);
}
std::vector<uint8_t>* Bpv7BlockConfidentialityBlock::GetAesWrappedKeyPtr() {
    return Private_GetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::WRAPPED_KEY);
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS parameter) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t & param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == parameter) {
            if (Bpv7AbstractSecurityBlockValueByteString * valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(param.second.get())) {
                return &(valueByteString->m_byteString);
            }
            else {
                return NULL;
            }
        }
    }
    //doesn't exist, add it
    m_securityContextParametersOptional.emplace_back();
    m_securityContextParametersOptional.back().first = static_cast<uint64_t>(parameter);
    std::unique_ptr<Bpv7AbstractSecurityBlockValueByteString> v = boost::make_unique<Bpv7AbstractSecurityBlockValueByteString>();
    std::vector<uint8_t> * retVal = &(v->m_byteString);
    m_securityContextParametersOptional.back().second = std::move(v);
    return retVal;
}
std::vector<uint8_t>* Bpv7BlockConfidentialityBlock::Private_GetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS parameter) {
    for (std::size_t i = 0; i < m_securityContextParametersOptional.size(); ++i) {
        security_context_parameter_t& param = m_securityContextParametersOptional[i];
        if (static_cast<BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS>(param.first) == parameter) {
            if (Bpv7AbstractSecurityBlockValueByteString* valueByteString = dynamic_cast<Bpv7AbstractSecurityBlockValueByteString*>(param.second.get())) {
                return &(valueByteString->m_byteString);
            }
            else {
                return NULL;
            }
        }
    }
    //doesn't exist
    return NULL;
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::AddAndGetInitializationVectorPtr() {
    return Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::INITIALIZATION_VECTOR);
}
std::vector<uint8_t>* Bpv7BlockConfidentialityBlock::GetInitializationVectorPtr() {
    return Private_GetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS::INITIALIZATION_VECTOR);
}
std::vector<uint8_t> * Bpv7BlockConfidentialityBlock::AppendAndGetPayloadAuthenticationTagPtr() {
    return Protected_AppendAndGetSecurityResultByteStringPtr(static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS::AUTHENTICATION_TAG));
}
std::vector<std::vector<uint8_t>*> Bpv7BlockConfidentialityBlock::GetAllPayloadAuthenticationTagPtrs() {
    return Protected_GetAllSecurityResultsByteStringPtrs(static_cast<uint64_t>(BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS::AUTHENTICATION_TAG));
}


/////////////////////////////////////////
// VALUES FOR ABSTRACT SECURITY BLOCK
/////////////////////////////////////////
Bpv7AbstractSecurityBlockValueBase::~Bpv7AbstractSecurityBlockValueBase() {}

Bpv7AbstractSecurityBlockValueUint::~Bpv7AbstractSecurityBlockValueUint() {}
uint64_t Bpv7AbstractSecurityBlockValueUint::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) {
    return CborEncodeU64(serialization, m_uintValue, bufferSize);
}
uint64_t Bpv7AbstractSecurityBlockValueUint::GetSerializationSize() const {
    return CborGetEncodingSizeU64(m_uintValue);
}
bool Bpv7AbstractSecurityBlockValueUint::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t cborUintSize;
    m_uintValue = CborDecodeU64(serialization, &cborUintSize, bufferSize);
    numBytesTakenToDecode = cborUintSize;
    return (numBytesTakenToDecode != 0);
}
bool Bpv7AbstractSecurityBlockValueUint::IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const {
    if (const Bpv7AbstractSecurityBlockValueUint * asUintPtr = dynamic_cast<const Bpv7AbstractSecurityBlockValueUint*>(otherPtr)) {
        return (asUintPtr->m_uintValue == m_uintValue);
    }
    else {
        return false;
    }
}

Bpv7AbstractSecurityBlockValueByteString::~Bpv7AbstractSecurityBlockValueByteString() {}
uint64_t Bpv7AbstractSecurityBlockValueByteString::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) {
    uint8_t * const byteStringHeaderStartPtr = serialization; //uint8_t * const serializationBase = serialization;
    uint64_t thisSerializationSize;
    thisSerializationSize = CborEncodeU64(serialization, m_byteString.size(), bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    *byteStringHeaderStartPtr |= (2U << 5); //change from major type 0 (unsigned integer) to major type 2 (byte string)

    if (bufferSize < m_byteString.size()) {
        return 0;
    }
    memcpy(serialization, m_byteString.data(), m_byteString.size());
    serialization += m_byteString.size(); //todo safety check on data length
    return serialization - byteStringHeaderStartPtr;// - serializationBase;
}
uint64_t Bpv7AbstractSecurityBlockValueByteString::GetSerializationSize() const {
    return CborGetEncodingSizeU64(m_byteString.size()) + m_byteString.size();
}
bool Bpv7AbstractSecurityBlockValueByteString::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    if (bufferSize == 0) { //for byteStringHeader
        return false;
    }
    uint8_t * const byteStringHeaderStartPtr = serialization; //buffer size verified above
    const uint8_t cborMajorTypeByteString = (*byteStringHeaderStartPtr) >> 5;
    if (cborMajorTypeByteString != 2) {
        return false; //failure
    }
    *byteStringHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
    uint8_t cborSizeDecoded;
    const uint64_t dataLength = CborDecodeU64(byteStringHeaderStartPtr, &cborSizeDecoded, bufferSize);
    *byteStringHeaderStartPtr |= (2U << 5); // restore to major type to 2 (change from major type 0 (unsigned integer) to major type 2 (byte string))
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    if (dataLength > bufferSize) {
        return false;
    }
    m_byteString.assign(serialization, serialization + dataLength);
    serialization += dataLength;

    numBytesTakenToDecode = serialization - byteStringHeaderStartPtr;
    return true;
}
bool Bpv7AbstractSecurityBlockValueByteString::IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const {
    if (const Bpv7AbstractSecurityBlockValueByteString * asByteStringPtr = dynamic_cast<const Bpv7AbstractSecurityBlockValueByteString*>(otherPtr)) {
        return (asByteStringPtr->m_byteString == m_byteString);
    }
    else {
        return false;
    }
}
