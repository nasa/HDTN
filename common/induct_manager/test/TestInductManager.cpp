/**
 * @file TestInductManager.cpp
 * @author  Amr Shadid <amr.shadid.2016@gmail.com>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boost/test/unit_test.hpp>
#include "InductManager.h"
#include "InductsConfig.h"
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

//On Windows the boost acceptor sets SO_REUSEADDR by default, which allows binding a port that is already
//bound by this same process, so the "port in use" condition cannot be created here.
#ifndef _WIN32

static std::string GetTcpclV4InductsConfigJson(const uint16_t boundPort) {
    return "{\"inductConfigName\":\"unit test\",\"inductVector\":[{"
        "\"name\":\"tcpclv4 induct\","
        "\"convergenceLayer\":\"tcpcl_v4\","
        "\"boundPort\":" + boost::lexical_cast<std::string>(boundPort) + ","
        "\"numRxCircularBufferElements\":200,"
        "\"numRxCircularBufferBytesPerElement\":20000,"
        "\"keepAliveIntervalSeconds\":15,"
        "\"tcpclV4MyMaxRxSegmentSizeBytes\":200000,"
        "\"tlsIsRequired\":false,"
        "\"certificatePemFile\":\"\","
        "\"privateKeyPemFile\":\"\","
        "\"diffieHellmanParametersPemFile\":\"\"}]}";
}

//A tcpcl induct binds its tcp acceptor within its constructor, which throws when the port is already in use.
//LoadInductsFromConfig must return false instead of letting that exception terminate the process.
BOOST_AUTO_TEST_CASE(InductManagerPortInUseTestCase)
{
    //bind an acceptor to an ephemeral port so that the port used below is guaranteed to be in use
    boost::asio::io_service ioService;
    boost::asio::ip::tcp::acceptor acceptor(ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    const uint16_t boundPort = acceptor.local_endpoint().port();

    InductsConfig_ptr inductsConfigPtr = InductsConfig::CreateFromJson(GetTcpclV4InductsConfigJson(boundPort));
    BOOST_REQUIRE(inductsConfigPtr);

    InductManager inductManager;
    bool loadedInducts = true;
    BOOST_REQUIRE_NO_THROW(loadedInducts = inductManager.LoadInductsFromConfig(InductProcessBundleCallback_t(), *inductsConfigPtr,
        1, 65535, 10000000, OnNewOpportunisticLinkCallback_t(), OnDeletedOpportunisticLinkCallback_t()));
    BOOST_CHECK(!loadedInducts); //the induct could not bind the port
    inductManager.Clear();

    //the same config loads successfully once that port is no longer in use
    acceptor.close();
    loadedInducts = false;
    BOOST_REQUIRE_NO_THROW(loadedInducts = inductManager.LoadInductsFromConfig(InductProcessBundleCallback_t(), *inductsConfigPtr,
        1, 65535, 10000000, OnNewOpportunisticLinkCallback_t(), OnDeletedOpportunisticLinkCallback_t()));
    BOOST_CHECK(loadedInducts);
    inductManager.Clear();
}

#endif //#ifndef _WIN32
