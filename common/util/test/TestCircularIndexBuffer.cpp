#include <boost/test/unit_test.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>


BOOST_AUTO_TEST_CASE(CircularIndexBuffer_TestCase)
{
    static const unsigned int SIZE_CB = 30;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable cib(SIZE_CB);
    std::vector<uint32_t> cbData(SIZE_CB);
    cib.Init();
    for (uint32_t i = 0; i < (SIZE_CB * 10); ++i) {

        //empty
        BOOST_REQUIRE(!cib.IsFull()); //not full
        BOOST_REQUIRE(cib.IsEmpty()); //is empty
        BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 0); //num bytes 0

        //push 1 byte
        const unsigned int writeIndex = cib.GetIndexForWrite();
        BOOST_REQUIRE(writeIndex != CIRCULAR_INDEX_BUFFER_FULL); //push check
        cbData[writeIndex] = i;
        cib.CommitWrite(); //pushed
        BOOST_REQUIRE(!cib.IsFull()); //not full
        BOOST_REQUIRE(!cib.IsEmpty()); //not empty
        BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 1); //num bytes 1


        //pop 1 byte
        uint32_t valueReadBack = CIRCULAR_INDEX_BUFFER_EMPTY;
        const unsigned int readIndex = cib.GetIndexForRead();
        BOOST_REQUIRE(readIndex != CIRCULAR_INDEX_BUFFER_EMPTY); //pop check
        valueReadBack = cbData[readIndex];
        cib.CommitRead();//pop success
        BOOST_REQUIRE(!cib.IsFull()); //not full
        BOOST_REQUIRE(cib.IsEmpty()); //is empty
        BOOST_REQUIRE_EQUAL(valueReadBack, i);
        BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 0); //num bytes 0
    }


    cib.Init();
    for (uint32_t i = 0; i < (SIZE_CB * 10); ++i) {
        //empty
        BOOST_REQUIRE(!cib.IsFull()); //not full
        BOOST_REQUIRE(cib.IsEmpty()); //is empty
        BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 0); //num bytes 0

        //push 1 byte
        {
            const unsigned int writeIndex = cib.GetIndexForWrite();
            BOOST_REQUIRE(writeIndex != CIRCULAR_INDEX_BUFFER_FULL); //push check
            cbData[writeIndex] = i;
            cib.CommitWrite(); //pushed
            BOOST_REQUIRE(!cib.IsFull()); //not full
            BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 1); //num bytes 1
        }

        //push byte 2
        {
            const unsigned int writeIndex = cib.GetIndexForWrite();
            BOOST_REQUIRE(writeIndex != CIRCULAR_INDEX_BUFFER_FULL); //push check
            cbData[writeIndex] = i + 10;
            cib.CommitWrite(); //pushed
            BOOST_REQUIRE(!cib.IsFull()); //not full
            BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 2); //num bytes 2
        }


        //pop byte 1
        {
            uint32_t valueReadBack = CIRCULAR_INDEX_BUFFER_EMPTY;
            const unsigned int readIndex = cib.GetIndexForRead();
            BOOST_REQUIRE(readIndex != CIRCULAR_INDEX_BUFFER_EMPTY); //pop check
            valueReadBack = cbData[readIndex];
            cib.CommitRead();//pop success
            BOOST_REQUIRE(!cib.IsFull()); //not full
            BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            BOOST_REQUIRE_EQUAL(valueReadBack, i);
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 1); //num bytes 1
        }

        //pop byte 2
        {
            uint32_t valueReadBack = CIRCULAR_INDEX_BUFFER_EMPTY;
            const unsigned int readIndex = cib.GetIndexForRead();
            BOOST_REQUIRE(readIndex != CIRCULAR_INDEX_BUFFER_EMPTY); //pop check
            valueReadBack = cbData[readIndex];
            cib.CommitRead();//pop success
            BOOST_REQUIRE(!cib.IsFull()); //not full
            BOOST_REQUIRE(cib.IsEmpty()); //is empty
            BOOST_REQUIRE_EQUAL(valueReadBack, i + 10);
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 0); //num bytes 0
        }
    }

    cib.Init();
    for (uint32_t i = 0; i < 10; ++i) {
        //std::cout << "i=" << i << "\n";
        for (uint32_t j = 1; j < (SIZE_CB); ++j) {
            //push
            const unsigned int writeIndex = cib.GetIndexForWrite();
            BOOST_REQUIRE(writeIndex != CIRCULAR_INDEX_BUFFER_FULL); //push check
            cbData[writeIndex] = j + i;
            cib.CommitWrite(); //pushed
            if (j == (SIZE_CB - 1)) {
                BOOST_REQUIRE(cib.IsFull()); //is full
            }
            else {
                BOOST_REQUIRE(!cib.IsFull()); //not full
            }

            BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), j); //num bytes j
        }


        //push another but fail
        const unsigned int writeIndex = cib.GetIndexForWrite();
        BOOST_REQUIRE(writeIndex == CIRCULAR_INDEX_BUFFER_FULL); //push fail
        BOOST_REQUIRE(cib.IsFull()); //is full
        BOOST_REQUIRE(!cib.IsEmpty()); //not empty
        BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), SIZE_CB - 1); //num bytes still

        for (uint32_t j = 1; j < (SIZE_CB); ++j) {
            //pop
            uint32_t valueReadBack = CIRCULAR_INDEX_BUFFER_EMPTY;
            const unsigned int readIndex = cib.GetIndexForRead();
            BOOST_REQUIRE(readIndex != CIRCULAR_INDEX_BUFFER_EMPTY); //pop check
            valueReadBack = cbData[readIndex];
            cib.CommitRead();//pop success
            BOOST_REQUIRE(!cib.IsFull()); //not full
            if (j == (SIZE_CB - 1)) {
                BOOST_REQUIRE(cib.IsEmpty()); //is empty
            }
            else {
                BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            }

            BOOST_REQUIRE_EQUAL(valueReadBack, j + i);
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), (SIZE_CB - j) - 1); //num bytes
        }



        //try to pop another byte but fail
        {
            const unsigned int readIndex = cib.GetIndexForRead();
            BOOST_REQUIRE(readIndex == CIRCULAR_INDEX_BUFFER_EMPTY); //pop fail
            BOOST_REQUIRE(!cib.IsFull() ); //not full
            BOOST_REQUIRE(cib.IsEmpty()); //is empty
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 0); //num bytes still 0
        }

    }

    cib.Init();
    for (uint32_t i = 0; i < (SIZE_CB * 2); ++i) {
        //std::cout << "i=" << i << "\n";
        for (uint32_t j = 1; j <= 7; ++j) {
            //push
            const unsigned int writeIndex = cib.GetIndexForWrite();
            BOOST_REQUIRE(writeIndex != CIRCULAR_INDEX_BUFFER_FULL); //push check
            cbData[writeIndex] = j + i;
            cib.CommitWrite(); //pushed
            BOOST_REQUIRE(!cib.IsFull()); //not full
            BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), j); //num bytes j
        }


        for (uint32_t j = 1; j <= 7; ++j) {
            //pop
            uint32_t valueReadBack = CIRCULAR_INDEX_BUFFER_EMPTY;
            const unsigned int readIndex = cib.GetIndexForRead();
            BOOST_REQUIRE(readIndex != CIRCULAR_INDEX_BUFFER_EMPTY); //pop check
            valueReadBack = cbData[readIndex];
            cib.CommitRead();//pop success
            BOOST_REQUIRE(!cib.IsFull()); //not full
            if (j == 7) {
                BOOST_REQUIRE(cib.IsEmpty()); //is empty
            }
            else {
                BOOST_REQUIRE(!cib.IsEmpty()); //not empty
            }

            BOOST_REQUIRE_EQUAL(valueReadBack, j + i);
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 7 - j); //num bytes
        }



        //try to pop another byte but fail
        {
            const unsigned int readIndex = cib.GetIndexForRead();
            BOOST_REQUIRE(readIndex == CIRCULAR_INDEX_BUFFER_EMPTY); //pop fail
            BOOST_REQUIRE(!cib.IsFull()); //not full
            BOOST_REQUIRE(cib.IsEmpty()); //is empty
            BOOST_REQUIRE_EQUAL(cib.NumInBuffer(), 0); //num bytes still 0
        }

    }



}