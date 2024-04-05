/**
 * @file HdtnVersion.hpp
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
 * Defines the current HDTN version (which also gets populated to the GUIs).
 * It is based off of boost/version.hpp
 */

#ifndef HDTN_VERSION_HPP
#define HDTN_VERSION_HPP


 //  This is the only HDTN header that is guaranteed
 //  to change with every HDTN release.
 //
 //  HDTN_VERSION % 100 is the patch level
 //  HDTN_VERSION / 100 % 1000 is the minor version
 //  HDTN_VERSION / 100000 is the major version
 //  00.000.00 where MAJOR_MINOR_PATCH

#define HDTN_VERSION 100200

#define HDTN_VERSION_PATCH (HDTN_VERSION % 100)
#define HDTN_VERSION_MINOR ((HDTN_VERSION / 100) % 1000)
#define HDTN_VERSION_MAJOR (HDTN_VERSION / 100000)

#endif
