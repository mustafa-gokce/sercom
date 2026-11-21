"""Check the comments and the countable idioms of the C++ sources against the house rules.

    python3 tools/analysis/check_style.py [--source-directory DIRECTORY] [--rule RULE] [FILE ...]

Without files every C++ file of the own source directories is checked, together with the repository checks.
Each finding is printed as path:line: rule: message, the exit code is 1 when there is a finding.
The layout is checked by tools/analysis/check_format.sh, the analyzers by tools/run_static_analysis.sh.
"""

import argparse
import bisect
import os
import re
import string
import subprocess
import sys

# directories of the own C++ sources, relative to the source directory
SOURCE_DIRECTORIES = ('core', 'app', 'host', 'firmware/common/include', 'firmware/common/src', 'firmware/esp32/include',
                      'firmware/esp32/src', 'tests', 'examples',
                      'fuzz')

# extensions of the C++ sources
SOURCE_EXTENSIONS = ('.h', '.cpp')

# characters of an identifier or a number
IDENTIFIER_CHARACTERS = set(string.ascii_letters + string.digits + '_')

# prefixes of a raw string literal
RAW_STRING_PREFIXES = ('u8R', 'uR', 'UR', 'LR', 'R')

# comments that steer a tool rather than describe the code
DIRECTIVE_COMMENT = re.compile(r'^\s*(NOLINT|clang-format (on|off))\b')

# a capitalized ordinary word, acronyms, constants and type names stay allowed
CAPITALIZED_WORD = re.compile(r'^[A-Z][a-z]+([\s,.;:]|$)')

# two sentences on one line
TWO_SENTENCES = re.compile(r'[a-z0-9)\]"]\.\s+[A-Z]')

# lines a comment line may follow inside a body: an opening brace, a label or a preprocessor line
OPENING_LINE = re.compile(r'(\{|^\s*(public|protected|private)\s*:|^\s*(case\b.*|default\s*):|^\s*#.*)$')

# idiom rules: rule, message, pattern, scope, domain, excluded lines, excluded paths
# the scopes are all, headers, sources, tests, programs (sources outside tests) and the domains are code (comments
# and literal contents blanked) and line (the line as it is)
IDIOM_RULES = (
    ('R-084', 'range-based for', r'for \([^;]*[^:]:[^:][^;]*\)', 'all', 'code', None, None),
    ('R-080', 'postfix increment or decrement', r'[A-Za-z0-9_\])](\+\+|--)[;)]|\+= 1;', 'all', 'code', None, None),
    ('R-078', 'cast to void', r'static_cast<void>|\(void\)\s*\w', 'all', 'code', None, None),
    ('R-074', 'U suffix on a shift, comparison or mask', r'(<<|>>) [0-9]+U\b|[!=<>]=? [0-9]+U\b|& 0x[0-9A-F]+U\b',
     'all', 'code', None, None),
    ('R-074', 'U suffix on a decimal literal', r'\b[0-9]+U\b', 'all', 'code', None, None),
    ('R-074', 'lower-case hex digit', r'\b0x[0-9A-Fa-f]*[a-f]', 'all', 'code', None, None),
    ('R-074', 'digit separator', r'[0-9]\'[0-9]', 'all', 'code', None, None),
    ('R-111', '_MS constant', r'\b[A-Z][A-Z0-9_]*_MS\b', 'all', 'line', None, None),
    ('R-096', 'unit in a camelCase name', r'\b[a-z][A-Za-z0-9]*(Ms|Msec|Millis)\b', 'all', 'line', None, None),
    ('R-093', 'MAX or MIN name', r'\b(MAX|MIN)_|_(MAX|MIN)_|\b(max|min)[A-Z]', 'all', 'line',
     r'std::(min|max)|numeric_limits|INT[0-9]*_MAX|UINT[0-9]*_MAX|SIZE_MAX', None),
    ('R-100', 'is, has or can accessor',
     r'^\s+(\[\[nodiscard\]\] )?(virtual )?(static )?bool (is|has|can)[A-Z]\w*\(.*\) const', 'all', 'line', None,
     None),
    ('R-019', '#ifdef or #if !defined', r'^\s*#\s*(ifdef|if\s*!\s*defined)', 'all', 'line', None, None),
    ('R-019', '#ifndef outside a guard', r'^#ifndef ', 'all', 'line', r'_H$', None),
    ('R-129', 'output outside the log functions', r'std::(cout|cerr|clog)|\bs?n?printf\(', 'all', 'code', None,
     r'^(host/src/HostUtils\.cpp|firmware/)'),
    ('R-066', 'analyzer suppression', r'NOLINTNEXTLINE|NOLINT\(|cppcheck-suppress', 'all', 'line', None, None),
    ('R-091', 'auto outside a lambda', r'\bauto\b', 'all', 'code', r'auto \w+ = \[|const auto \w+ = \[', None),
    ('R-117', 'enum name without Type or Key', r'enum class \w+', 'all', 'code', r'enum class \w+(Type|Key)\b', None),
    ('R-021', 'template', r'^\s*template\s*<', 'all', 'code', None, None),
    ('R-021', 'constexpr function', r'(\[\[nodiscard\]\] )?(static )?(inline )?constexpr [A-Za-z_:<>0-9, ]+ \w+\(',
     'all', 'code', None, r'^core/src/CRC32\.cpp$'),
    ('R-114', 'constant at file scope of a source', r'^(static )?(constexpr|const) [^(]*(=|;$)', 'sources', 'code',
     None, r'Settings\.cpp$'),
    ('R-089', 'default capture', r'\[(=|&)\]|\[(=|&),', 'all', 'code', None, None),
    ('R-089', 'candidate lambda parameter', r'\((const )?\w+ &?candidate\)', 'all', 'code', None, None),
    ('R-151', 'width helper without the type name', r'\b(read|write|load|store|parse)(U|I|F)(8|16|32|64)\b', 'all',
     'line', None, None),
    ('R-134', 'serialize or encode name', r'\b(serialize|deserialize|encode|decode)[A-Za-z]*\(', 'all', 'code', None,
     None),
    ('R-071', 'brace value-initialization', r'^\s+[A-Za-z_:<>, 0-9]+ [a-z]\w*\{\};', 'sources', 'code', None, None),
    ('R-083', 'C array of literal size', r'\b(char|uint8_t|int) \w+\[[0-9]+\]', 'all', 'code', None, r'^firmware/'),
    ('R-090', 'hand-written minimum or maximum', r'(\w+) [<>]=? (\w+) \? \1 : \2', 'all', 'code', None, None),
    ('R-024', 'nodiscard on an enum converter', r'\[\[nodiscard\]\] [^(]*\b(from|to)[A-Z]\w*(Type|Key)\(', 'headers',
     'code', None, None),
    ('R-012', 'include through an include directory', r'#include "[^.]', 'all', 'line', None, None),
    ('R-033', 'override outside the destructor of a class that others derive from', r'\boverride\b', 'all', 'code',
     r'^\s+~\w+\(\) override;$', None),
    ('R-248', 'letter-suffixed name', r'\b[a-z][a-zA-Z]*[a-z][AB]\b', 'all', 'code', None, None),
    ('R-251', 'char **argv', r'char \*\*argv', 'all', 'code', None, None),
    ('R-242', 'make_unique, make_shared or reset()', r'make_unique|make_shared|\.reset\(\)', 'all', 'code', None,
     None),
    ('R-249', 'ternary on end()', r'== [^;]*c?end\(\) \?', 'all', 'code', None, None),
    ('R-030', 'bare {} argument', r'[(,] ?\{\}\)', 'all', 'code', r'return \{\}', None),
    ('R-250', 'class or struct in a program source', r'^\s*(class|struct) \w+( final)?( :.*)?$', 'programs', 'code',
     None, None),
    ('R-131', 'snprintf on the host', r'snprintf\(', 'all', 'code', None, r'^firmware/'),
    ('R-104', 'unsigned, long or short', r'\bunsigned\b|\blong\b|\bshort\b', 'all', 'code', None, None),
    ('R-075', 'size compared with zero', r'\.size\(\) == 0\b', 'all', 'code', None, None),
    ('R-041', 'reference member without reference comment', r'^\s+[A-Za-z:]+ &\w+;\s*// (?!reference)', 'headers',
     'line', None, None),
    ('R-162', 'TEST_CASE without fixture', r'^TEST_CASE\(', 'tests', 'line', None, None),
    ('R-169', 'test name in lower case', r'TEST_CASE(_METHOD)?\([^"]*"[a-z]|^\s+"[a-z][^"]*",$', 'tests', 'line',
     None, None),
    ('R-170', 'tag in lower case', r'"\[[a-z_]+\]', 'tests', 'line', None, None),
    ('R-173', 'REQUIRE with ! or !=', r'REQUIRE\((!|[^;]*!=)', 'tests', 'code', None, None),
    ('R-160', 'Catch2 header other than catch_all.hpp', r'#include <catch2/', 'tests', 'line', r'catch_all\.hpp', None),
    ('R-161', 'using-declaration or named namespace in a test', r'^using \w+::\w+;|^namespace \w', 'tests', 'code',
     None, None),
    ('R-180', 'GENERATE', r'GENERATE\(', 'tests', 'code', None, None),
    ('R-176', 'size against a bare literal', r'\.size\(\) == [0-9]+U?\)', 'tests', 'code', None, None),
    ('R-016', 'using-declaration', r'^\s*using \w+::\w+;', 'all', 'code', None, r'^tests/'),
    ('R-021', 'typedef', r'\btypedef\b', 'all', 'code', None, None),
    ('R-024', 'nodiscard on void', r'\[\[nodiscard\]\] (virtual |static )?void\b', 'all', 'code', None, None),
    ('R-026', 'const_cast or std::move', r'\bconst_cast\b|\bstd::move\(', 'all', 'code', None, None),
    ('R-027', 'top-level const on a pointer', r'\* ?const \w', 'all', 'code', None, None),
    ('R-028', 'optional output pointer', r'\*\w+ = nullptr\)', 'headers', 'code', None, None),
    ('R-045', 'mutable member that is not a lock', r'\bmutable\b', 'all', 'code', r'mutable std::(shared_)?mutex',
     None),
    ('R-092', 'abbreviated name', r'\b(msg|buf|len|idx|rx|tx)([A-Z]\w*)?\b|[a-z](Msg|Buf|Len|Idx|Rx|Tx)\b', 'all',
     'code', None, None),
    ('R-095', 'raw prefix in a name', r'\braw[A-Z]', 'all', 'code', None, None),
    ('R-101', 'transmit or update name', r'\btransmit\w*\(|\bvoid update\(\)', 'all', 'code', None, None),
    ('R-103', 'find or By name', r'\bfind[A-Z]\w*\(|By[A-Z]\w*\(', 'all', 'code', None, None),
    ('R-106', 'time as an integer', r'\buint32_t \w*(time|Time|period|Period)\b', 'all', 'code', None, None),
    ('R-110', 'static constexpr in a header', r'^static constexpr|^\s+static constexpr', 'headers', 'code', None,
     None),
    ('R-111', 'integer timeout or period constant', r'constexpr u?int\w* \w*(TIMEOUT|PERIOD)', 'all', 'code', None,
     None),
    ('R-127', 'exception', r'\bthrow\b|\btry\b|\bcatch \(', 'all', 'code', None, None),
    ('R-150', 'literal delay', r'\bvTaskDelay\([0-9]|\bdelay\([0-9]', 'all', 'code', None, None),
    ('R-172', 'assertion other than REQUIRE and REQUIRE_FALSE',
     r'\b(CHECK|CHECK_FALSE|CHECK_THAT|CHECK_NOTHROW|CHECK_THROWS|REQUIRE_THAT|REQUIRE_NOTHROW|REQUIRE_THROWS)\w*\(',
     'tests', 'code', None, None),
    ('R-181', 'time advanced by a bare number', r'\badvance\([0-9]+(ms)?\)', 'tests', 'code', None, None),
)

# forbidden CMake commands: rule, message, pattern
CMAKE_RULES = (
    ('R-207', 'FetchContent dependency', r'FetchContent'),
    ('R-206', 'per-case test discovery', r'catch_discover_tests'),
    ('R-012', 'include directory', r'include_directories'),
    ('R-183', 'Catch2 main instead of the own runner', r'Catch2WithMain'),
    ('R-210', 'forced build type', r'set\(CMAKE_BUILD_TYPE'),
)

# mock classes that examples and fuzz targets may keep
PROGRAM_MOCK = re.compile(r'^(examples|fuzz)/.*:\s*(class|struct) Mock\w+')

# opening and closing lines of a class or struct body at namespace scope
RECORD_OPENING = re.compile(r'^(class|struct) \w+')
RECORD_CLOSING = re.compile(r'^\};')

# data member with a default member initializer
MEMBER_INITIALIZER = re.compile(r'^\s+(?=\S)(?!(using|static|friend)\b)[^()=]*[\w>\]](\{[^{}()]*\}| = [^;()]*);\s*$')

# declaration that continues on the next line with a lambda
LAMBDA_CONTINUATION = re.compile(r'^\s*(const )?auto \w+ =$')

# opening line of a test case, its body ends at the next closing brace in the first column
TEST_CASE_OPENING = re.compile(r'^TEST_CASE_METHOD\(')


class Comment:
    """One comment of a source with its position."""

    def __init__(self, kind, start, end, text):
        self.kind = kind
        self.start = start
        self.end = end
        self.text = text
        self.first_line = 0
        self.last_line = 0
        self.alone = False


def blank(characters, start, end):
    """Replace the characters of a range by spaces, line ends stay."""
    for index in range(start, end):
        if characters[index] != '\n':
            characters[index] = ' '


def find_quoted_end(text, index, quote):
    """Return the index after the closing quote of a literal that starts at index."""
    position = index + 1
    while position < len(text):
        character = text[position]
        if character == '\\':
            position += 2
            continue
        if character == quote:
            return position + 1
        if character == '\n':
            return position
        position += 1
    return len(text)


def find_string_end(text, index):
    """Return the index after a string literal that starts with the quote at index."""
    for prefix in RAW_STRING_PREFIXES:
        start = index - len(prefix)
        if start < 0 or text[start:index] != prefix:
            continue
        if start > 0 and text[start - 1] in IDENTIFIER_CHARACTERS:
            continue
        opening = text.find('(', index)
        closing = ')' + text[index + 1:opening] + '"'
        end = text.find(closing, opening)
        return len(text) if opening < 0 or end < 0 else end + len(closing)
    return find_quoted_end(text, index, '"')


def scan_source(text):
    """Return the code view of a source, with comments and literal contents blanked, and its comments."""
    characters = list(text)
    comments = []
    index = 0
    while index < len(text):
        # line comment
        if text.startswith('//', index):
            end = text.find('\n', index)
            end = len(text) if end < 0 else end
            comments.append(Comment('line', index, end, text[index + 2:end]))
            blank(characters, index, end)
            index = end
            continue

        # block comment
        if text.startswith('/*', index):
            end = text.find('*/', index + 2)
            end = len(text) if end < 0 else end + 2
            comments.append(Comment('block', index, end, text[index:end]))
            blank(characters, index, end)
            index = end
            continue

        # string and character literals, a quote after a digit separates digits
        character = text[index]
        if character == '"':
            end = find_string_end(text, index)
            blank(characters, index + 1, end - 1)
            index = end
            continue
        if character == '\'' and (index == 0 or text[index - 1] not in IDENTIFIER_CHARACTERS):
            end = find_quoted_end(text, index, '\'')
            blank(characters, index + 1, end - 1)
            index = end
            continue
        index += 1
    return ''.join(characters), comments


class Source:
    """One C++ source with its lines, its code view and its comments."""

    def __init__(self, path, relative_path):
        with open(path, encoding='utf-8', errors='replace', newline='') as source:
            original_text = source.read()
        self.carriage_return_lines = [number for number, line in enumerate(original_text.split('\n'), 1)
                                      if '\r' in line]
        self.text = original_text.replace('\r\n', '\n')
        self.relative_path = relative_path
        self.lines = self.text.split('\n')
        code, self.comments = scan_source(self.text)
        self.code_lines = code.split('\n')
        line_starts = [0] + [match.end() for match in re.finditer('\n', self.text)]
        for comment in self.comments:
            comment.first_line = bisect.bisect_right(line_starts, comment.start)
            comment.last_line = bisect.bisect_right(line_starts, max(comment.start, comment.end - 1))
            comment.alone = self.code_lines[comment.first_line - 1].strip() == ''

    def get_line(self, number):
        """Return a line by its number, an empty text outside the file."""
        if number < 1 or number > len(self.lines):
            return ''
        return self.lines[number - 1]

    def get_code_line(self, number):
        """Return the code view of a line by its number, an empty text outside the file."""
        if number < 1 or number > len(self.code_lines):
            return ''
        return self.code_lines[number - 1]


def check_line_comments(source, findings):
    """Check the // comments of a source."""
    alone_lines = set()
    for comment in source.comments:
        if comment.kind != 'line' or DIRECTIVE_COMMENT.match(comment.text):
            continue
        text = comment.text.strip()
        if comment.alone:
            alone_lines.add(comment.first_line)

        # wording of the comment
        if re.search(r'[.;:]$', text) and 'http' not in text:
            findings.append((comment.first_line, 'R-058', 'comment ends with punctuation'))
        if CAPITALIZED_WORD.match(text):
            findings.append((comment.first_line, 'R-058', 'comment starts with a capital letter'))
        if text.startswith('NOTE:') and (not text.startswith('NOTE: ') or CAPITALIZED_WORD.match(text[6:])):
            findings.append((comment.first_line, 'R-061', 'NOTE without a lowercase clause'))

    # one // line, placed after a blank line or an opening line and followed by code
    for number in sorted(alone_lines):
        if number - 1 in alone_lines:
            if number - 2 not in alone_lines:
                findings.append((number - 1, 'R-051', 'two // comment lines in a row'))
            continue
        previous_line = source.get_code_line(number - 1).rstrip()
        if number > 1 and previous_line.strip() != '' and not OPENING_LINE.search(previous_line):
            findings.append((number, 'R-060', 'comment line follows code'))
        if source.get_line(number + 1).strip() == '':
            findings.append((number, 'R-060', 'comment line followed by a blank line'))


def check_block_comments(source, findings):
    """Check the block comments of a source."""
    for comment in source.comments:
        if comment.kind != 'block':
            continue
        first_line = comment.first_line

        # opening and closing lines of their own
        if comment.first_line == comment.last_line:
            findings.append((first_line, 'R-050', 'block comment on one line'))
            continue
        if not comment.alone or source.get_line(first_line).strip() != '/*':
            findings.append((first_line, 'R-050', 'block comment opening is not /* alone'))
        if source.get_line(comment.last_line).strip() != '*/':
            findings.append((comment.last_line, 'R-050', 'block comment closing is not */ alone'))

        # one sentence per inner line
        for number in range(first_line + 1, comment.last_line):
            line = source.get_line(number)
            match = re.match(r'^\s*\* (\S.*)$', line)
            if line.strip() == '*':
                findings.append((number, 'R-050', 'blank line in a block comment'))
                continue
            if match is None:
                findings.append((number, 'R-050', 'block comment line does not start with " * "'))
                continue
            text = match.group(1).rstrip()
            if text[0] in '-@\\|#=':
                findings.append((number, 'R-050', 'list, table, tag or banner in a block comment'))
            if not re.search(r'[.!?:]$', text):
                findings.append((number, 'R-050', 'block comment line without full stop'))
            elif TWO_SENTENCES.search(text):
                findings.append((number, 'R-050', 'block comment line with two sentences'))


def check_comment_texts(source, findings):
    """Check words that no comment may hold."""
    for comment in source.comments:
        if re.search(r'\b(TODO|FIXME)\b', comment.text):
            findings.append((comment.first_line, 'R-061', 'TODO or FIXME'))
        if re.search(r'\bfolders?\b', comment.text, re.IGNORECASE):
            findings.append((comment.first_line, 'R-064', 'folder instead of directory'))


def check_file_rules(source, findings):
    """Check the rules about the whole file."""
    if source.get_line(1).lstrip().startswith(('//', '/*')):
        findings.append((1, 'R-008', 'file starts with a comment'))

    # line ends are LF only
    if source.carriage_return_lines:
        findings.append((source.carriage_return_lines[0], 'R-004', 'carriage return in a line end'))

    # settings headers hold no comment lines inside their namespace
    if not source.relative_path.endswith('Settings.h'):
        return
    inside = False
    for number, line in enumerate(source.lines, 1):
        if re.match(r'^namespace [\w:]+ \{$', line):
            inside = True
        elif line.startswith('} // namespace'):
            inside = False
        elif inside and re.match(r'^\s*(//|/\*)', line):
            findings.append((number, 'R-108', 'comment line in a settings header'))


def check_class_members(source, findings):
    """Check that the data members of a class have no default member initializer."""
    record_kind = None
    depth = 0
    for number, line in enumerate(source.code_lines, 1):
        opening = RECORD_OPENING.match(line)
        if record_kind is None:
            if opening is not None and not line.rstrip().endswith(';'):
                record_kind = opening.group(1)
                depth = 0
            continue
        if RECORD_CLOSING.match(line):
            record_kind = None
            continue

        # data members stand directly inside the class braces
        if record_kind == 'class' and depth == 1 and MEMBER_INITIALIZER.match(line):
            findings.append((number, 'R-042', 'default member initializer in a class'))
        depth += line.count('{') - line.count('}')


def check_test_bodies(source, findings):
    """Check that test case bodies reach the fixture members without this->."""
    if not get_scope_match(source.relative_path, 'tests'):
        return
    inside = False
    for number, line in enumerate(source.code_lines, 1):
        if TEST_CASE_OPENING.match(line):
            inside = True
        elif inside and line.startswith('}'):
            inside = False
        elif inside and re.search(r'\bthis->', line):
            findings.append((number, 'R-168', 'this-> in a test case body'))


def get_scope_match(relative_path, scope):
    """Get whether a file belongs to an idiom rule scope."""
    is_test = relative_path.startswith('tests/')
    is_source = relative_path.endswith('.cpp')
    if scope == 'headers':
        return relative_path.endswith('.h')
    if scope == 'sources':
        return is_source
    if scope == 'tests':
        return is_test and is_source
    if scope == 'programs':
        return is_source and not is_test
    return True


def check_idioms(source, findings):
    """Check the countable idioms of a source."""
    for rule, message, pattern, scope, domain, excluded_lines, excluded_paths in IDIOM_RULES:
        if not get_scope_match(source.relative_path, scope):
            continue
        if excluded_paths is not None and re.search(excluded_paths, source.relative_path):
            continue
        lines = source.code_lines if domain == 'code' else source.lines
        for number, line in enumerate(lines, 1):
            if not re.search(pattern, line):
                continue
            if excluded_lines is not None and re.search(excluded_lines, line):
                continue
            if rule == 'R-250' and PROGRAM_MOCK.match('%s:%s' % (source.relative_path, line)):
                continue
            if rule == 'R-091' and LAMBDA_CONTINUATION.match(line.rstrip()) and source.get_code_line(
                    number + 1).lstrip().startswith('['):
                continue
            findings.append((number, rule, message))


def check_repository(source_directory):
    """Check the repository files, return the findings as (path, line, rule, message)."""
    findings = []

    # analyzer configuration and cmake modules live outside the root
    for name in ('.clang-format', '.clang-tidy', '.editorconfig', 'cmake'):
        if os.path.exists(os.path.join(source_directory, name)):
            findings.append((name, 0, 'R-224', 'analyzer configuration or cmake directory in the root'))

    # build directories and patches are ignored
    if os.path.isdir(os.path.join(source_directory, '.git')):
        for name in ('build/x', 'x.patch'):
            ignored = subprocess.call(['git', '-C', source_directory, 'check-ignore', '-q', '--no-index', name])
            if ignored != 0:
                findings.append(('.gitignore', 0, 'R-214', '%s is not ignored' % name))

    # each test file holds its own helpers
    tests_directory = os.path.join(source_directory, 'tests')
    for name in sorted(os.listdir(tests_directory)) if os.path.isdir(tests_directory) else ():
        if name.endswith('.h'):
            findings.append(('tests/%s' % name, 0, 'R-159', 'shared test header'))

    # build file
    cmake_path = os.path.join(source_directory, 'CMakeLists.txt')
    if not os.path.exists(cmake_path):
        return findings
    with open(cmake_path, encoding='utf-8') as cmake_file:
        cmake_lines = cmake_file.read().split('\n')
    option_count = 0
    for number, line in enumerate(cmake_lines, 1):
        for rule, message, pattern in CMAKE_RULES:
            if re.search(pattern, line):
                findings.append(('CMakeLists.txt', number, rule, message))
        if re.search(r'\b(if|elseif|foreach|while)\(', line):
            findings.append(('CMakeLists.txt', number, 'R-194', 'no blank between the command and ('))
        if re.search(r'target_link_libraries\(.*\)$|target_link_libraries\(.*\bPUBLIC\b', line):
            findings.append(('CMakeLists.txt', number, 'R-253', 'one-line or PUBLIC target_link_libraries'))
        if re.search(r'\bSTATIC\b|\bSHARED\b|\bGLOB|set\(\w*SOURCES\b', line):
            findings.append(('CMakeLists.txt', number, 'R-202', 'library type, glob or source list variable'))
        if re.search(r'\bLABELS\b', line):
            findings.append(('CMakeLists.txt', number, 'R-206', 'test labels'))
        if re.match(r'^\s*option\(', line):
            option_count += 1
    if option_count != 2:
        findings.append(('CMakeLists.txt', 0, 'R-196', '%d option() calls instead of 2' % option_count))
    return findings


def list_sources(source_directory):
    """Return the relative paths of the own C++ sources."""
    paths = []
    for directory in SOURCE_DIRECTORIES:
        for base, directories, names in os.walk(os.path.join(source_directory, directory)):
            directories[:] = sorted(name for name in directories if not name.startswith('.'))
            for name in sorted(names):
                if name.endswith(SOURCE_EXTENSIONS):
                    paths.append(os.path.relpath(os.path.join(base, name), source_directory))
    return paths


def main():
    """Check the sources and print the findings."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('files', nargs='*', help='C++ files to check, every own source without one')
    parser.add_argument('--source-directory', default=os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))), help='root of the repository (default the repository of this tool)')
    parser.add_argument('--rule', default=None, help='report only the findings of this rule, for example R-050')
    arguments = parser.parse_args()
    source_directory = os.path.abspath(arguments.source_directory)

    # select the files
    findings = []
    if arguments.files:
        paths = [os.path.relpath(os.path.abspath(path), source_directory) for path in arguments.files]
    else:
        paths = list_sources(source_directory)
        findings += check_repository(source_directory)

    # check every file
    for relative_path in paths:
        source = Source(os.path.join(source_directory, relative_path), relative_path)
        file_findings = []
        check_line_comments(source, file_findings)
        check_block_comments(source, file_findings)
        check_comment_texts(source, file_findings)
        check_file_rules(source, file_findings)
        check_class_members(source, file_findings)
        check_test_bodies(source, file_findings)
        check_idioms(source, file_findings)
        findings += [(relative_path, number, rule, message) for number, rule, message in file_findings]

    # notify the user
    if arguments.rule is not None:
        findings = [finding for finding in findings if finding[2] == arguments.rule]
    for path, number, rule, message in sorted(findings):
        print('%s:%d: %s: %s' % (path, number, rule, message))
    print('%d findings in %d files' % (len(findings), len(paths)))
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
