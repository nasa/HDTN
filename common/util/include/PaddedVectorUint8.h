#ifndef PADDED_VECTOR_UINT8_H
#define PADDED_VECTOR_UINT8_H 1

#include <cstdlib>
#include <new>
#include <limits>
#include <iostream>
#include <vector>

//https://en.cppreference.com/w/cpp/named_req/Allocator

template <class T>
struct PaddedMallocator {

    typedef T value_type;
    static constexpr std::size_t PADDING_ELEMENTS_BEFORE = 128;
    static constexpr std::size_t PADDING_ELEMENTS_AFTER = 32;
    static constexpr std::size_t TOTAL_PADDING_ELEMENTS = PADDING_ELEMENTS_BEFORE + PADDING_ELEMENTS_AFTER;

    PaddedMallocator() = default;
    template <class U> constexpr PaddedMallocator(const PaddedMallocator <U>&) noexcept {}

    //[[nodiscard]]
    T* allocate(std::size_t n) {
        const std::size_t elementsWithPadding = n + TOTAL_PADDING_ELEMENTS;
        //if (elementsWithPadding > std::numeric_limits<std::size_t>::max() / sizeof(T))
        //    throw std::bad_array_new_length();

        if (T* p = static_cast<T*>(std::malloc(elementsWithPadding * sizeof(T)))) {
#ifdef PADDED_VECTOR_UNIT_TESTING
            static const std::vector<std::string> testStringsVec = { "padding_start", "before_data", "after_reserved", "padding_end" };
            //std::cout << "n " << n << "\n";
            T * paddingStart = p;
            T * dataStart = p + PADDING_ELEMENTS_BEFORE;
            T * rightAfterReservedSpace = p + (PADDING_ELEMENTS_BEFORE + n);
            T * paddingEnd = p + (TOTAL_PADDING_ELEMENTS + n);
            memcpy(paddingStart, testStringsVec[0].data(), testStringsVec[0].size()); //start of padding
            memcpy(dataStart - testStringsVec[1].size(), testStringsVec[1].data(), testStringsVec[1].size()); //right before data
            memcpy(rightAfterReservedSpace, testStringsVec[2].data(), testStringsVec[2].size()); //right after reserved size
            memcpy(paddingEnd - testStringsVec[3].size(), testStringsVec[3].data(), testStringsVec[3].size()); //right before data
#endif
            return p + PADDING_ELEMENTS_BEFORE;
        }

        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t n) noexcept {
        std::free(p - PADDING_ELEMENTS_BEFORE);
    }
};

template <class T, class U>
bool operator==(const PaddedMallocator <T>&, const PaddedMallocator <U>&) { return true; }
template <class T, class U>
bool operator!=(const PaddedMallocator <T>&, const PaddedMallocator <U>&) { return false; }

typedef std::vector<uint8_t, PaddedMallocator<uint8_t> > padded_vector_uint8_t;

#endif //PADDED_VECTOR_UINT8_H

