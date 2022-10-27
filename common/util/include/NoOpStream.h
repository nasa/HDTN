/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ***************************************************************************
 */

#ifndef _NO_OP_STREAM_H
#define _NO_OP_STREAM_H

#include <iostream>

/**
 * _NO_OP_STREAM is a macro to efficiently discard a streaming expression. Since the "else"
 * branch is never taken, the compiler should optimize it out.
 */
#define _NO_OP_STREAM if (true) {} else std::cout

#endif
