"""
This script detects new high cyclomatic complexity (CCM) functions in the current branch.
"""

import subprocess
import os

import lizard


def get_modified_files():
    """
    Get the list of modified files in the current branch
    :return: list of modified files
    """
    result = subprocess.run(['git', 'diff', '--name-only', 'development'],
                            stdout=subprocess.PIPE, check=False)
    files = result.stdout.decode('utf-8').split('\n')
    return [file for file in files if file]  # remove empty strings


def print_ccm_summary(functions):
    """
    Print a summary of new high cyclomatic complexity (CCM) functions
    :param new_high_ccm_functions: list of tuples (file_analysis, new_functions)
    """
    print('High cyclomatic complexity (CCM) functions:')
    for file_analysis, new_functions in functions:
        if not new_functions:
            continue
        print(f'\n\nFile: {file_analysis.filename}')
        for function in new_functions:
            print(f'\t CCM {function.cyclomatic_complexity}, {function.name} ' +
                  f'(Lines {function.start_line}-{function.end_line})') 


target_branch = os.getenv('CI_MERGE_REQUEST_TARGET_BRANCH_NAME')
source_branch = os.getenv('CI_MERGE_REQUEST_SOURCE_BRANCH_NAME')

# Get the list of modified files
modified_files = get_modified_files()

# Get the CCM report for the current branch
current_branch_report = list(lizard.analyze(paths=modified_files))

# Get the CCM report for the target branch
subprocess.run(['git', 'checkout', target_branch], check=False, capture_output=True)
target_branch_report = list(lizard.analyze(paths=modified_files))

# Switch back to the original branch
subprocess.run(['git', 'checkout', source_branch], check=False, capture_output=True)

# Compare reports to find any new, high-CCM functions
new_high_ccm_functions = [
    (file_analysis, [
        function for function in file_analysis.function_list
        if function.cyclomatic_complexity > 15 and
        not any(f.name == function.name and f.cyclomatic_complexity > 15 for f in
                next((f.function_list for f in target_branch_report
                      if f.filename == file_analysis.filename), []))
    ])
    for file_analysis in current_branch_report
    if any(function.cyclomatic_complexity > 15 for function in file_analysis.function_list)
]

# Print a summary and fail the script if new high CCM functions were detected
if not new_high_ccm_functions:
    print('No new high cyclomatic complexity (CCM) functions were detected in the current branch.')
    exit(0)
else:
    print_ccm_summary(new_high_ccm_functions)
    exit(1)
