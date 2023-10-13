#!/bin/bash

# Enable nullglob to handle patterns with no matches
shopt -s nullglob

# Arguments
diff_version1=$1
diff_version2=$2

# Common cloc commands
# Diffing git files is broken in the most recent cloc version, so use v1.82
# Summing diff files requires cloc version 1.98
CLOC_CMD="perl /opt/cloc-1.82.pl --include-lang='C++,C/C++ Header,JavaScript,HTML,CSS' --quiet"
SUM_DIFF_CLOC_CMD="perl /opt/cloc-1.98.pl"

# Counts lines and generates reports for all folders in the provided directory
count() {
    name=$1
    dirs=$2
    extras="$3"
    for dir in $dirs
    do
        dir=${dir%*/}
        folder="${dir##*/}"
        cmd="$CLOC_CMD --report-file=reports/$name/${folder}.csv --csv $dir $extras"
        eval "$cmd"
        cmd="$CLOC_CMD --report-file=reports/$name/temp/${folder} $dir $extras"
        eval "$cmd"
    done
}

# Count diff lines and generated reports for all folders in the provided directory
count_diffs() {
    name=$1
    dirs=$2
    extras="$3"
    for dir in $dirs
    do
        dir=${dir%*/}
        folder="${dir##*/}"
        rel_path="${dir#*hdtn/}"
        cmd="$CLOC_CMD --report-file=reports/$name/diffs/${folder}.csv --csv --git --diff $diff_version1:$rel_path $diff_version2:$rel_path $extras"
        eval "$cmd"
    done
}

# Sums all reports in the provided directory
sum() {
    name=$1
    cmd="cd reports/$name/temp && $CLOC_CMD --sum-reports --report-file=../../all/$name.csv --csv * && cd ../../../"
    eval "$cmd"
    cmd="cd reports/$name/temp && $CLOC_CMD --sum-reports --report-file=../../all/temp/$name * && cd ../../../"
    eval "$cmd"

    rm -f reports/all/$name.csv.lang
    mv reports/all/$name.csv.file reports/all/$name.csv

    rm -f reports/all/temp/$name.lang
    mv reports/all/temp/$name.file reports/all/temp/$name

    cat reports/all/temp/$name
    rm -r reports/$name/temp    
}

# Sum all diff reports in the provided directory
sum_diffs() {
    name=$1
    cmd="cd reports/$name/diffs && $SUM_DIFF_CLOC_CMD --sum-reports --csv --diff --report-file=../../all/diffs/$name.csv * && cd ../../../"
    eval "$cmd"
}

###########
# Modules #
###########
count modules "module/*/" '--exclude-dir=unit_tests,test'
sum modules
dirs=("module")
count_diffs modules "${dirs[@]}" '--exclude-dir=unit_tests,test'
sum_diffs modules

##########
# Common #
##########
count common "common/*/" '--exclude-dir=unit_tests,test'
sum common
dirs=("common")
count_diffs common "${dirs[@]}" '--exclude-dir=unit_tests,test'
sum_diffs common

####################
# Integrated Tests #
####################
dirs=("tests/integrated_tests")
count integrated_tests "${dirs[@]}"
sum integrated_tests
count_diffs integrated_tests "${dirs[@]}"
sum_diffs integrated_tests

##############
# Unit Tests #
##############
count unit_tests "module/*/" "--match-d='.*/(unit_tests|test).*'"
count unit_tests "common/*/" "--match-d='.*/(unit_tests|test).*'"
sum unit_tests
dirs=("common")
count_diffs unit_tests "${dirs[@]}" "--match-d='.*/(unit_tests|test).*'"
dirs=("module")
count_diffs unit_tests "${dirs[@]}" "--match-d='.*/(unit_tests|test).*'"
sum_diffs unit_tests

#######
# All #
#######
sum all
sum_diffs all
