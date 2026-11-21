"""Pass the build values of CMakeLists.txt to a firmware build and mark the framework headers as system headers.

    pio run -d firmware/esp32
    pio run -d firmware/stm32f4

The platformio.ini of every firmware project runs this script before the build, so every firmware and the host tools
share one set of build values.
The protocol limits and the version numbers are the set() lines at the top of CMakeLists.txt.
The build number is GITHUB_RUN_NUMBER when it is set and the value of CMakeLists.txt otherwise, as in the CMake build.
The git hash is GITHUB_SHA when it is set, then the hash of git rev-parse HEAD and unknown otherwise, as in CMake.
The framework headers come from system include directories, so the house warnings stop at them.
The own headers are included by relative path and keep every warning.
"""

import os
import re
import shutil
import subprocess

Import('env')

# set() line of a plain value at the start of a line
SET_LINE_PATTERN = re.compile(r'^set\((\w+) (\w+)\)$', re.MULTILINE)

# path of the CMake file that holds the build values
CMAKE_LISTS_PATH = os.path.join(env.subst('$PROJECT_DIR'), '..', '..', 'CMakeLists.txt')

# protocol limits that must match on both peers
PROTOCOL_LIMIT_NAMES = ('MAXIMUM_PAYLOAD_SIZE', 'MAXIMUM_PENDING_FRAMES', 'DEFAULT_DUPLICATE_HISTORY_SIZE')

# mark the include directories as system include directories
env.Replace(INCPREFIX='-isystem')

# get the build values as CMake sets them
with open(CMAKE_LISTS_PATH, encoding='utf-8') as cmake_lists_file:
    cmake_values = dict(SET_LINE_PATTERN.findall(cmake_lists_file.read()))
version_major = cmake_values['APP_VERSION_MAJOR']
version_minor = cmake_values['APP_VERSION_MINOR']
version_build = os.environ.get('GITHUB_RUN_NUMBER', cmake_values['APP_VERSION_BUILD'])

# get the git hash as CMake gets it
git_hash = os.environ.get('GITHUB_SHA', '')
if not git_hash and shutil.which('git') is not None:
    git_result = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=os.path.dirname(CMAKE_LISTS_PATH),
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True)
    if git_result.returncode == 0:
        git_hash = git_result.stdout.strip()
if not git_hash:
    git_hash = 'unknown'

# pass the protocol limits to every source
env.Append(CPPDEFINES=[(name, cmake_values[name]) for name in PROTOCOL_LIMIT_NAMES])

# pass the version numbers and the git hash to every source
env.Append(CPPDEFINES=[
    ('APP_VERSION_MAJOR', version_major),
    ('APP_VERSION_MINOR', version_minor),
    ('APP_VERSION_BUILD', version_build),
    ('APP_GIT_HASH', env.StringifyMacro(git_hash)),
    ('APP_VERSION_STRING', env.StringifyMacro('%s.%s.%s' % (version_major, version_minor, version_build))),
])
