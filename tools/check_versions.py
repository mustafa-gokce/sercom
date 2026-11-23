"""Compare the version numbers of CMakeLists.txt with the library and firmware build files.

    python3 tools/check_versions.py [--source-directory DIRECTORY]

A release changes APP_VERSION_MAJOR and APP_VERSION_MINOR in CMakeLists.txt and the version MAJOR.MINOR.0 of
core/library.json, app/library.json and firmware/common/library.json together. The firmware builds read
CMakeLists.txt, version flags written into firmware/esp32/platformio.ini or firmware/stm32f4/platformio.ini must match
it too.
Each version is printed, the exit code is 1 when one of them differs from CMakeLists.txt.
"""

import argparse
import json
import os
import re
import sys

# library manifests that carry the version
LIBRARY_FILES = ('core/library.json', 'app/library.json', 'firmware/common/library.json')

# firmware build files that may pass the version
FIRMWARE_FILES = ('firmware/esp32/platformio.ini', 'firmware/stm32f4/platformio.ini')


def read_text(source_directory, relative_path):
    """Return the text of a repository file."""
    with open(os.path.join(source_directory, relative_path), encoding='utf-8') as source:
        return source.read()


def get_cmake_version(source_directory):
    """Return the major and minor version of CMakeLists.txt, None when one is missing."""
    text = read_text(source_directory, 'CMakeLists.txt')
    major = re.search(r'^set\(APP_VERSION_MAJOR ([0-9]+)\)$', text, re.MULTILINE)
    minor = re.search(r'^set\(APP_VERSION_MINOR ([0-9]+)\)$', text, re.MULTILINE)
    if major is None or minor is None:
        return None
    return (int(major.group(1)), int(minor.group(1)))


def get_firmware_versions(source_directory, firmware_file):
    """Return the APP_VERSION_* flags of a firmware build file by name."""
    text = read_text(source_directory, firmware_file)
    versions = {}
    for name, value in re.findall(r'-D(APP_VERSION_\w+)=\\?"?([^\s"\'\\]+)', text):
        versions[name] = value
    return versions


def main():
    """Print the versions and compare them."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--source-directory', default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        help='root of the repository (default the repository of this tool)')
    arguments = parser.parse_args()
    source_directory = os.path.abspath(arguments.source_directory)
    failures = []

    # version of CMakeLists.txt
    cmake_version = get_cmake_version(source_directory)
    if cmake_version is None:
        print('CMakeLists.txt: APP_VERSION_MAJOR or APP_VERSION_MINOR is missing')
        return 1
    expected_version = '%d.%d' % cmake_version
    print('CMakeLists.txt: %s' % expected_version)

    # versions of the library manifests
    for relative_path in LIBRARY_FILES:
        version = json.loads(read_text(source_directory, relative_path)).get('version', '')
        print('%s: %s' % (relative_path, version))
        if version != '%s.0' % expected_version:
            failures.append('%s has version %s instead of %s.0' % (relative_path, version, expected_version))

    # version flags of the firmware build files
    for firmware_file in FIRMWARE_FILES:
        firmware_versions = get_firmware_versions(source_directory, firmware_file)
        if not firmware_versions:
            print('%s: no version flags, the firmware build reads CMakeLists.txt' % firmware_file)
        else:
            firmware_version = '%s.%s' % (firmware_versions.get('APP_VERSION_MAJOR', '?'),
                                          firmware_versions.get('APP_VERSION_MINOR', '?'))
            print('%s: %s' % (firmware_file, firmware_version))
            if firmware_version != expected_version:
                failures.append('%s has version %s instead of %s' % (firmware_file, firmware_version,
                                                                     expected_version))
        version_string = firmware_versions.get('APP_VERSION_STRING')
        if version_string is not None and not version_string.startswith('%s.' % expected_version):
            failures.append('%s has version string %s instead of %s.*' % (firmware_file, version_string,
                                                                          expected_version))

    # notify the user
    for failure in failures:
        print('version mismatch: %s' % failure)
    if failures:
        return 1
    print('versions match')
    return 0


if __name__ == '__main__':
    sys.exit(main())
