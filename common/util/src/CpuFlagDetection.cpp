/**
 * @file CpuFlagDetection.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

//based on example from https://docs.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
//cross platform idea from https://www.boost.org/doc/libs/master/boost/beast/core/detail/cpu_info.hpp

// processor: x86, x64
// Uses the __cpuid intrinsic to get information about
// CPU extended instruction set support.
#ifndef CPU_FLAG_DETECTION_RUN_MAIN_ONLY
#include "CpuFlagDetection.h"
#endif
#include <iostream>
#include <vector>
#include <bitset>
#include <array>
#include <string>
#include <cstring>


#ifdef _MSC_VER
#include <intrin.h> // __cpuid
#else
#include <cpuid.h>  // __get_cpuid
#endif

static void CpuIdCrossPlatform(int * eaxThrougEdxRegisters, int id) {
#ifdef _MSC_VER
    __cpuid(eaxThrougEdxRegisters, id);
#else
    __get_cpuid(static_cast<unsigned int>(id),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[0]),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[1]),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[2]),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[3]));
#endif
}

static void CpuIdExCrossPlatform(int * eaxThrougEdxRegisters, int id, int subId) {
#ifdef _MSC_VER
    __cpuidex(eaxThrougEdxRegisters, id, subId);
#else
    __get_cpuid_count(static_cast<unsigned int>(id), static_cast<unsigned int>(subId),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[0]),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[1]),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[2]),
        reinterpret_cast<unsigned int*>(&eaxThrougEdxRegisters[3]));
#endif
}


class InstructionSet
{
    // forward declarations
    class InstructionSet_Internal;

public:
    // getters
    static std::string Vendor(void) { return CPU_Rep.m_vendorString; }
    static std::string Brand(void) { return CPU_Rep.m_brandString; }
    static std::string FlagsList(void) { return CPU_Rep.m_cpuFlagsString; }

    //https://en.wikipedia.org/wiki/CPUID#EAX=1:_Processor_Info_and_Feature_Bits
    static bool SSE3(void) { return CPU_Rep.m_f_1_ECX[0]; }
    static bool PCLMULQDQ(void) { return CPU_Rep.m_f_1_ECX[1]; }
    static bool MONITOR(void) { return CPU_Rep.m_f_1_ECX[3]; }
    static bool SSSE3(void) { return CPU_Rep.m_f_1_ECX[9]; }
    static bool FMA(void) { return CPU_Rep.m_f_1_ECX[12]; }
    static bool CMPXCHG16B(void) { return CPU_Rep.m_f_1_ECX[13]; }
    static bool SSE41(void) { return CPU_Rep.m_f_1_ECX[19]; }
    static bool SSE42(void) { return CPU_Rep.m_f_1_ECX[20]; }
    static bool MOVBE(void) { return CPU_Rep.m_f_1_ECX[22]; }
    static bool POPCNT(void) { return CPU_Rep.m_f_1_ECX[23]; }
    static bool AES(void) { return CPU_Rep.m_f_1_ECX[25]; }
    static bool XSAVE(void) { return CPU_Rep.m_f_1_ECX[26]; }
    static bool OSXSAVE(void) { return CPU_Rep.m_f_1_ECX[27]; }
    static bool AVX(void) { return CPU_Rep.m_f_1_ECX[28]; }
    static bool F16C(void) { return CPU_Rep.m_f_1_ECX[29]; }
    static bool RDRAND(void) { return CPU_Rep.m_f_1_ECX[30]; }
    //https://en.wikipedia.org/wiki/CPUID#EAX=1:_Processor_Info_and_Feature_Bits
    static bool MSR(void) { return CPU_Rep.m_f_1_EDX[5]; }
    static bool CX8(void) { return CPU_Rep.m_f_1_EDX[8]; }
    static bool SEP(void) { return CPU_Rep.m_f_1_EDX[11]; }
    static bool CMOV(void) { return CPU_Rep.m_f_1_EDX[15]; }
    static bool CLFSH(void) { return CPU_Rep.m_f_1_EDX[19]; }
    static bool MMX(void) { return CPU_Rep.m_f_1_EDX[23]; }
    static bool FXSR(void) { return CPU_Rep.m_f_1_EDX[24]; }
    static bool SSE(void) { return CPU_Rep.m_f_1_EDX[25]; }
    static bool SSE2(void) { return CPU_Rep.m_f_1_EDX[26]; }
    //https://en.wikipedia.org/wiki/CPUID#EAX=7,_ECX=0:_Extended_Features
    static bool FSGSBASE(void) { return CPU_Rep.m_f_7_EBX[0]; }
    static bool BMI1(void) { return CPU_Rep.m_f_7_EBX[3]; }
    static bool HLE(void) { return CPU_Rep.m_isIntel && CPU_Rep.m_f_7_EBX[4]; }
    static bool AVX2(void) { return CPU_Rep.m_f_7_EBX[5]; }
    static bool BMI2(void) { return CPU_Rep.m_f_7_EBX[8]; }
    static bool ERMS(void) { return CPU_Rep.m_f_7_EBX[9]; }
    static bool INVPCID(void) { return CPU_Rep.m_f_7_EBX[10]; }
    static bool RTM(void) { return CPU_Rep.m_isIntel && CPU_Rep.m_f_7_EBX[11]; }
    static bool AVX512F(void) { return CPU_Rep.m_f_7_EBX[16]; }
    static bool RDSEED(void) { return CPU_Rep.m_f_7_EBX[18]; }
    static bool ADX(void) { return CPU_Rep.m_f_7_EBX[19]; }
    static bool AVX512PF(void) { return CPU_Rep.m_f_7_EBX[26]; }
    static bool AVX512ER(void) { return CPU_Rep.m_f_7_EBX[27]; }
    static bool AVX512CD(void) { return CPU_Rep.m_f_7_EBX[28]; }
    static bool SHA(void) { return CPU_Rep.m_f_7_EBX[29]; }
    static bool AVX512BW(void) { return CPU_Rep.m_f_7_EBX[30]; }
    static bool AVX512VL(void) { return CPU_Rep.m_f_7_EBX[31]; }

    static bool PREFETCHWT1(void) { return CPU_Rep.m_f_7_ECX[0]; }
    static bool AVX512VBMI(void) { return CPU_Rep.m_f_7_ECX[1]; }
    static bool AVX512VBMI2(void) { return CPU_Rep.m_f_7_ECX[6]; }

    //https://en.wikipedia.org/wiki/CPUID#EAX=80000001h:_Extended_Processor_Info_and_Feature_Bits
    static bool LAHF(void) { return CPU_Rep.m_f_81_ECX[0]; }
    static bool LZCNT(void) { return CPU_Rep.m_isIntel && CPU_Rep.m_f_81_ECX[5]; }
    static bool ABM(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_ECX[5]; }
    static bool SSE4a(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_ECX[6]; }
    static bool XOP(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_ECX[11]; }
    static bool TBM(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_ECX[21]; }

    static bool SYSCALL(void) { return CPU_Rep.m_isIntel && CPU_Rep.m_f_81_EDX[11]; }
    static bool MMXEXT(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_EDX[22]; }
    static bool RDTSCP(void) { return CPU_Rep.m_isIntel && CPU_Rep.m_f_81_EDX[27]; }
    static bool _3DNOWEXT(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_EDX[30]; }
    static bool _3DNOW(void) { return CPU_Rep.m_isAMD && CPU_Rep.m_f_81_EDX[31]; }

private:
    static const InstructionSet_Internal CPU_Rep;

    class InstructionSet_Internal {
    public:
        InstructionSet_Internal();
            

        int m_numIds;
        int m_numExtIds;
        std::string m_vendorString;
        std::string m_brandString;
        std::string m_cpuFlagsString;
        bool m_isIntel;
        bool m_isAMD;
        std::bitset<32> m_f_1_ECX;
        std::bitset<32> m_f_1_EDX;
        std::bitset<32> m_f_7_EBX;
        std::bitset<32> m_f_7_ECX;
        std::bitset<32> m_f_81_ECX;
        std::bitset<32> m_f_81_EDX;
        std::vector<std::array<int, 4>> m_data;
        std::vector<std::array<int, 4>> m_extData;

    private:
        void AppendFeature(std::string & toAppend, const std::string & cpuFlag, bool isSupported);
        void GenerateFlagsString(std::string & flagsString);
    };
    

    
};

InstructionSet::InstructionSet_Internal::InstructionSet_Internal() :
    m_numIds(0),
    m_numExtIds(0),
    m_isIntel(false),
    m_isAMD(false),
    m_f_1_ECX(0),
    m_f_1_EDX(0),
    m_f_7_EBX(0),
    m_f_7_ECX(0),
    m_f_81_ECX(0),
    m_f_81_EDX(0)
{
    std::array<int, 4> cpuInfoArrayInt4;

    //https://en.wikipedia.org/wiki/CPUID#EAX=0:_Highest_Function_Parameter_and_Manufacturer_ID
    // Calling __cpuid with 0x0 as the function_id argument
    // gets the number of the highest valid function ID.
    // This returns the CPU's manufacturer ID string � a twelve-character ASCII string stored in EBX, EDX, ECX (in that order)
    // The highest basic calling parameter (largest value that EAX can be set to before calling CPUID) is returned in EAX.
    CpuIdCrossPlatform(cpuInfoArrayInt4.data(), 0);
    m_numIds = cpuInfoArrayInt4[0];

    for (int i = 0; i <= m_numIds; ++i) {
        CpuIdExCrossPlatform(cpuInfoArrayInt4.data(), i, 0);
        m_data.push_back(cpuInfoArrayInt4);
    }

    // Capture vendor string
    int32_t vendorAligned4[32 / 4];
    char * const vendor = (char*)vendorAligned4;
    memset(vendor, 0, sizeof(vendorAligned4));
    vendorAligned4[0] = m_data[0][1];
    vendorAligned4[1] = m_data[0][3];
    vendorAligned4[2] = m_data[0][2];
    m_vendorString = vendor;
    if (m_vendorString == "GenuineIntel") {
        m_isIntel = true;
    }
    else if (m_vendorString == "AuthenticAMD") {
        m_isAMD = true;
    }

    // load bitset with flags for function 0x00000001
    if (m_numIds >= 1) {
        m_f_1_ECX = m_data[1][2];
        m_f_1_EDX = m_data[1][3];
    }

    // load bitset with flags for function 0x00000007
    if (m_numIds >= 7) {
        m_f_7_EBX = m_data[7][1];
        m_f_7_ECX = m_data[7][2];
    }

    // https://en.wikipedia.org/wiki/CPUID#EAX=80000000h:_Get_Highest_Extended_Function_Implemented
    // Calling __cpuid with 0x80000000 as the function_id argument
    // gets the number of the highest valid extended ID.
    // The highest calling parameter is returned in EAX.
    CpuIdCrossPlatform(cpuInfoArrayInt4.data(), 0x80000000);
    m_numExtIds = cpuInfoArrayInt4[0];

    char brand[0x40];
    memset(brand, 0, sizeof(brand));

    for (int i = 0x80000000; i <= m_numExtIds; ++i) {
        CpuIdExCrossPlatform(cpuInfoArrayInt4.data(), i, 0);
        m_extData.push_back(cpuInfoArrayInt4);
    }

    // load bitset with flags for function 0x80000001
    if (m_numExtIds >= 0x80000001) {
        m_f_81_ECX = m_extData[1][2];
        m_f_81_EDX = m_extData[1][3];
    }

    //https://en.wikipedia.org/wiki/CPUID#EAX=80000002h,80000003h,80000004h:_Processor_Brand_String
    // Interpret CPU brand string if reported
    if (m_numExtIds >= 0x80000004) {
        memcpy(brand, m_extData[2].data(), sizeof(cpuInfoArrayInt4));
        memcpy(brand + 16, m_extData[3].data(), sizeof(cpuInfoArrayInt4));
        memcpy(brand + 32, m_extData[4].data(), sizeof(cpuInfoArrayInt4));
        m_brandString = brand;
    }

    GenerateFlagsString(m_cpuFlagsString);
}

void InstructionSet::InstructionSet_Internal::AppendFeature(std::string & toAppend, const std::string & cpuFlag, bool isSupported) {
    if (isSupported) {
        if (!toAppend.empty()) {
            toAppend += ",";
        }
        toAppend += cpuFlag;
    }
}



// Print out supported instruction set extensions
void InstructionSet::InstructionSet_Internal::GenerateFlagsString(std::string & flagsString) {
    flagsString.clear();
    flagsString.reserve(1000);

    AppendFeature(flagsString, "3DNOW", InstructionSet::_3DNOW());
    AppendFeature(flagsString, "3DNOWEXT", InstructionSet::_3DNOWEXT());
    AppendFeature(flagsString, "ABM", InstructionSet::ABM());
    AppendFeature(flagsString, "ADX", InstructionSet::ADX());
    AppendFeature(flagsString, "AES", InstructionSet::AES());
    AppendFeature(flagsString, "AVX", InstructionSet::AVX());
    AppendFeature(flagsString, "AVX2", InstructionSet::AVX2());
    AppendFeature(flagsString, "AVX512CD", InstructionSet::AVX512CD());
    AppendFeature(flagsString, "AVX512ER", InstructionSet::AVX512ER());
    AppendFeature(flagsString, "AVX512F", InstructionSet::AVX512F());
    AppendFeature(flagsString, "AVX512PF", InstructionSet::AVX512PF());
    AppendFeature(flagsString, "AVX512BW", InstructionSet::AVX512BW());
    AppendFeature(flagsString, "AVX512VL", InstructionSet::AVX512VL());
    AppendFeature(flagsString, "AVX512VBMI", InstructionSet::AVX512VBMI());
    AppendFeature(flagsString, "AVX512VBMI2", InstructionSet::AVX512VBMI2());
    AppendFeature(flagsString, "BMI1", InstructionSet::BMI1());
    AppendFeature(flagsString, "BMI2", InstructionSet::BMI2());
    AppendFeature(flagsString, "CLFSH", InstructionSet::CLFSH());
    AppendFeature(flagsString, "CMPXCHG16B", InstructionSet::CMPXCHG16B());
    AppendFeature(flagsString, "CX8", InstructionSet::CX8());
    AppendFeature(flagsString, "ERMS", InstructionSet::ERMS());
    AppendFeature(flagsString, "F16C", InstructionSet::F16C());
    AppendFeature(flagsString, "FMA", InstructionSet::FMA());
    AppendFeature(flagsString, "FSGSBASE", InstructionSet::FSGSBASE());
    AppendFeature(flagsString, "FXSR", InstructionSet::FXSR());
    AppendFeature(flagsString, "HLE", InstructionSet::HLE());
    AppendFeature(flagsString, "INVPCID", InstructionSet::INVPCID());
    AppendFeature(flagsString, "LAHF", InstructionSet::LAHF());
    AppendFeature(flagsString, "LZCNT", InstructionSet::LZCNT());
    AppendFeature(flagsString, "MMX", InstructionSet::MMX());
    AppendFeature(flagsString, "MMXEXT", InstructionSet::MMXEXT());
    AppendFeature(flagsString, "MONITOR", InstructionSet::MONITOR());
    AppendFeature(flagsString, "MOVBE", InstructionSet::MOVBE());
    AppendFeature(flagsString, "MSR", InstructionSet::MSR());
    AppendFeature(flagsString, "OSXSAVE", InstructionSet::OSXSAVE());
    AppendFeature(flagsString, "PCLMULQDQ", InstructionSet::PCLMULQDQ());
    AppendFeature(flagsString, "POPCNT", InstructionSet::POPCNT());
    AppendFeature(flagsString, "PREFETCHWT1", InstructionSet::PREFETCHWT1());
    AppendFeature(flagsString, "RDRAND", InstructionSet::RDRAND());
    AppendFeature(flagsString, "RDSEED", InstructionSet::RDSEED());
    AppendFeature(flagsString, "RDTSCP", InstructionSet::RDTSCP());
    AppendFeature(flagsString, "RTM", InstructionSet::RTM());
    AppendFeature(flagsString, "SEP", InstructionSet::SEP());
    AppendFeature(flagsString, "SHA", InstructionSet::SHA());
    AppendFeature(flagsString, "SSE", InstructionSet::SSE());
    AppendFeature(flagsString, "SSE2", InstructionSet::SSE2());
    AppendFeature(flagsString, "SSE3", InstructionSet::SSE3());
    AppendFeature(flagsString, "SSE41", InstructionSet::SSE41());
    AppendFeature(flagsString, "SSE42", InstructionSet::SSE42());
    AppendFeature(flagsString, "SSE4a", InstructionSet::SSE4a());
    AppendFeature(flagsString, "SSSE3", InstructionSet::SSSE3());
    AppendFeature(flagsString, "SYSCALL", InstructionSet::SYSCALL());
    AppendFeature(flagsString, "TBM", InstructionSet::TBM());
    AppendFeature(flagsString, "XOP", InstructionSet::XOP());
    AppendFeature(flagsString, "XSAVE", InstructionSet::XSAVE());
}

// Initialize static member data
const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

#ifndef CPU_FLAG_DETECTION_RUN_MAIN_ONLY
std::string CpuFlagDetection::GetCpuFlagsCommaSeparated() {
    return InstructionSet::FlagsList();
}
std::string CpuFlagDetection::GetCpuVendor() {
    return InstructionSet::Vendor();
}
std::string CpuFlagDetection::GetCpuBrand() {
    return InstructionSet::Brand();
}

#else
//for use in cmake try_run() only
int main() {
    std::cout << "VENDOR_BEGIN" << InstructionSet::Vendor() << "VENDOR_END " << std::endl;
    std::cout << "BRAND_BEGIN" << InstructionSet::Brand() << "BRAND_END" << std::endl;
    std::cout << "ALL_CPU_FLAGS_BEGIN" << InstructionSet::FlagsList() << "ALL_CPU_FLAGS_END" << std::endl;
    return 0;
}
#endif
