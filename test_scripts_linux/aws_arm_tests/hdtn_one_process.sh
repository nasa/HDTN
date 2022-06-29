#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
hdtn_config=$config_files/hdtn/aws_arm_test.json

cd $HDTN_SOURCE_ROOT

#Use this script as an guide. Run each command in seperate console window. This will be a lot easier to read results

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --cut-through-only-test  --hdtn-config-file=$hdtn_config 

