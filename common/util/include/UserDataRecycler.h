/**
 * @file UserDataRecycler.h
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
 *
 * @section DESCRIPTION
 *
 * This class maintains generic data that gets recycled by a
 * forward_list (who's elements itself are also recycled).
 */

#ifndef USER_DATA_RECYCLER_H
#define USER_DATA_RECYCLER_H 1

#include <algorithm>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <forward_list>
#include <vector>
#include "FreeListAllocator.h"

template <typename userDataType>
class UserDataRecycler {
private:
    UserDataRecycler() = delete;
public:
    /**
     * Set max size of user data recycle bin.
     * @param maxSize The max size of user data recycle bin.
     */
    UserDataRecycler(const uint64_t maxSize) : m_currentSize(0), m_maxSize(maxSize) {
        //set max number of recyclable allocated max elements for the list
        // - once maxSize has been reached, operater new ops will cease
        // - if maxSize is never exceeded, operator delete will never occur
        m_list.get_allocator().SetMaxListSizeFromGetAllocatorCopy(maxSize);
    }

    /**
     * Give user data back to recycler.
     * @param userData The user data to try to recylce (i.e. move).
     * @return True if the user data was moved (i.e. returned),
     * or False if the recycle bin was full or False if the data had zero size() and zero capacity().
     */
    bool ReturnUserData(userDataType&& userData) {
        if (m_currentSize < m_maxSize) {
            if (userData.size() || userData.capacity()) {
                m_list.emplace_front(std::move(userData));
                ++m_currentSize;
                return true;
            }
        }
        return false;
    }

    /**
     * Get user data from the recycler.
     * @param userData The user data to try to reuse.
     * The user data was from the recycle bin if and only if !userData.empty()
     * @return True if the user data was moved (i.e. returned), or False if the recycle bin was full.
     */
    void GetRecycledOrCreateNewUserData(userDataType& userData) {
        if (m_currentSize) {
            userData = std::move(m_list.front());
            m_list.pop_front();
            --m_currentSize;
        }
    }

    /**
     * Get list size.
     * @return list size
     */
    uint64_t GetListSize() const noexcept {
        return m_currentSize;
    }

    /**
     * Get list capacity.
     * @return list capacity
     */
    uint64_t GetListCapacity() const noexcept {
        return m_maxSize;
    }
private:
    typedef std::forward_list<userDataType,
        FreeListAllocatorDynamic<userDataType> > flist_t;
    /// Recycle bin singly linked list
    flist_t m_list;
    /// Current size of the list to keep track of list size
    uint64_t m_currentSize;
    /// Max capacity of list
    const uint64_t m_maxSize;
};

typedef UserDataRecycler<std::vector<uint8_t> > UserDataRecyclerVecUint8;


#endif //USER_DATA_RECYCLER_H

