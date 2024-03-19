/**
 * @file StartStorageRunner.h
 * @author Tad Kollar <tad.kollar@nasa.gov>
 *
 * @copyright Copyright (c) 2024 United States Government as represented by
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
 * This file provides the "int main()" function to wrap StorageRunner
 * and forward command line arguments to StorageRunner.
 * This file is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to the Storage module.
 */

#ifndef START_STORAGE_RUNNER_H
#define START_STORAGE_RUNNER_H

#include "storage_lib_export.h"

STORAGE_LIB_EXPORT int startStorageRunner(int argc, const char* argv[]);

#endif
