/**
 * @file PaddedVectorUint8.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This PaddedVectorUint8 templated class is a custom allocator intended for std::vector<uint8_t>.
 * This allocator adds contiguous bytes of padding before and after a vector (used by an induct)
 * so that bundles can be manipulated in place (grow a few bytes in either direction) without
 * the need to reallocate/copy a modified bundle.
 */

#ifndef PADDED_VECTOR_UINT8_H
#define PADDED_VECTOR_UINT8_H 1

#include <cstdlib>
#include <new>
#include <limits>
#include <iostream>
#include <vector>

struct PaddedMallocatorConstants {
    static constexpr std::size_t PADDING_ELEMENTS_BEFORE = 256;
    static constexpr std::size_t PADDING_ELEMENTS_AFTER = 32;
    static constexpr std::size_t TOTAL_PADDING_ELEMENTS = PADDING_ELEMENTS_BEFORE + PADDING_ELEMENTS_AFTER;
};

//https://en.cppreference.com/w/cpp/named_req/Allocator

template <class T>
struct PaddedMallocator {

    typedef T value_type;
    

    PaddedMallocator() = default;
    template <class U> constexpr PaddedMallocator(const PaddedMallocator <U>&) noexcept {}


    //[[nodiscard]]
    T* allocate(std::size_t n) {
        const std::size_t elementsWithPadding = n + PaddedMallocatorConstants::TOTAL_PADDING_ELEMENTS;
        //if (elementsWithPadding > std::numeric_limits<std::size_t>::max() / sizeof(T))
        //    throw std::bad_array_new_length();

        if (T* p = static_cast<T*>(std::malloc(elementsWithPadding * sizeof(T)))) {
            return p + PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE;
        }

        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t n) noexcept {
        (void)n;
        std::free(p - PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE);
    }

    //force default initialize construction
    template <typename U>
    void construct(U* ptr) noexcept(std::is_nothrow_default_constructible<U>::value) {
        (void)ptr;
        //::new(static_cast<void*>(ptr)) U; //should be no-op anyway
    }
};

template <class T>
struct PaddedMallocatorUnitTest {

    void DoUnitTest(T* p, std::size_t n) {
        static const std::vector<std::string> testStringsVec = { "padding_start", "before_data", "after_reserved", "padding_end" };
        //std::cout << "n " << n << "\n";
        T* paddingStart = p;
        T* dataStart = p + PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE;
        T* rightAfterReservedSpace = p + (PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE + n);
        T* paddingEnd = p + (PaddedMallocatorConstants::TOTAL_PADDING_ELEMENTS + n);
        memcpy(paddingStart, testStringsVec[0].data(), testStringsVec[0].size()); //start of padding
        memcpy(dataStart - testStringsVec[1].size(), testStringsVec[1].data(), testStringsVec[1].size()); //right before data
        memcpy(rightAfterReservedSpace, testStringsVec[2].data(), testStringsVec[2].size()); //right after reserved size
        memcpy(paddingEnd - testStringsVec[3].size(), testStringsVec[3].data(), testStringsVec[3].size()); //right before data
    }

    typedef T value_type;

    PaddedMallocatorUnitTest() = default;
    template <class U> constexpr PaddedMallocatorUnitTest(const PaddedMallocatorUnitTest <U>&) noexcept {}


    //[[nodiscard]]
    T* allocate(std::size_t n) {
        const std::size_t elementsWithPadding = n + PaddedMallocatorConstants::TOTAL_PADDING_ELEMENTS;
        //if (elementsWithPadding > std::numeric_limits<std::size_t>::max() / sizeof(T))
        //    throw std::bad_array_new_length();

        if (T* p = static_cast<T*>(std::malloc(elementsWithPadding * sizeof(T)))) {
            DoUnitTest(p, n);
            return p + PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE;
        }

        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t n) noexcept {
        (void)n;
        std::free(p - PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE);
    }

    //force default initialize construction
    template <typename U>
    void construct(U* ptr) noexcept(std::is_nothrow_default_constructible<U>::value) {
        (void)ptr;
        //::new(static_cast<void*>(ptr)) U; //should be no-op anyway
    }
};

//non-unit testing
template <class T, class U>
bool operator==(const PaddedMallocator <T>&, const PaddedMallocator <U>&) { return true; }
template <class T, class U>
bool operator!=(const PaddedMallocator <T>&, const PaddedMallocator <U>&) { return false; }
typedef std::vector<uint8_t, PaddedMallocator<uint8_t> > padded_vector_uint8_t;

//unit testing
template <class T, class U>
bool operator==(const PaddedMallocatorUnitTest <T>&, const PaddedMallocatorUnitTest <U>&) { return true; }
template <class T, class U>
bool operator!=(const PaddedMallocatorUnitTest <T>&, const PaddedMallocatorUnitTest <U>&) { return false; }
typedef std::vector<uint8_t, PaddedMallocatorUnitTest<uint8_t> > padded_vector_uint8_unit_test_t;

#endif //PADDED_VECTOR_UINT8_H

