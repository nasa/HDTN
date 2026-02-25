/**
 * @file main.cpp
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
 *
 */

#include "Telemetry.h"
#include "Logger.h"

int main(int argc, const char* argv[]) {
    {
        hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::telem);

        Telemetry telemetry;
        std::atomic<bool> running;
        telemetry.Run(argc, argv, running);
    }
    return 0;
}
