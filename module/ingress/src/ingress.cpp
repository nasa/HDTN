/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Ingress- CL adapter that accepts traffic in bundle format
 ****************************************************************************
 */


#include <iostream>
#include "IngressAsyncRunner.h"


int main(int argc, const char* argv[]) {


    IngressAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "m_bundleCountStorage: " << runner.m_bundleCountStorage << std::endl;
    std::cout << "m_bundleCountEgress: " << runner.m_bundleCountEgress << std::endl;
    std::cout << "m_bundleCount: " << runner.m_bundleCount << std::endl;
    std::cout << "m_bundleData: " << runner.m_bundleData << std::endl;
    return 0;

}
