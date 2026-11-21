# sercom

Packet Based Binary Serial Protocol for Qt Hosts on Linux and Windows, ESP32 and STM32F4 Devices

## Building the project
1. Clone the project with: `git clone https://github.com/mustafa-gokce/sercom.git`
2. Go to the source directory.
3. Populate dependencies: `git submodule update --init --recursive`

### Linux
1. Open a terminal.
2. Install the tools: `sudo apt install build-essential cmake ninja-build python3 python3-venv socat gdb git`
3. Install aqtinstall into its own environment: `python3 -m venv ~/aqt && ~/aqt/bin/pip install aqtinstall`
4. Install Qt 5.12.12, the only Qt the host code builds with: `~/aqt/bin/aqt install-qt linux desktop 5.12.12 gcc_64 -O ~/Qt`
5. Go to the source directory.
6. `mkdir build && cd build`
7. `cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`
8. `cmake --build .`

### Windows
1. Install Git, CMake, Python 3 and PowerShell 7 (`winget install Git.Git Kitware.CMake Python.Python.3.12 Microsoft.PowerShell`).
2. Open PowerShell 7 (`pwsh`).
3. Install aqtinstall: `python -m pip install aqtinstall pyserial`
4. Install Qt 5.12.12 for MinGW 7.3.0 64-bit: `python -m aqt install-qt windows desktop 5.12.12 win64_mingw73 -O C:\Qt`
5. Install MinGW 7.3.0 64-bit, the compiler of this Qt and the only Windows compiler of the project: `python -m aqt install-tool windows desktop tools_mingw qt.tools.win64_mingw730 -O C:\Qt`
6. Install Ninja: `python -m aqt install-tool windows desktop tools_ninja qt.tools.ninja -O C:\Qt`
7. Put MinGW, Ninja and Qt on the path of the prompt: `$env:PATH = "C:\Qt\Tools\mingw730_64\bin;C:\Qt\Tools\Ninja;C:\Qt\5.12.12\mingw73_64\bin;$env:PATH"`
8. Go to the source directory.
9. `mkdir build; cd build`
10. `cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
11. `cmake --build .`

## Run tests
### Linux
1. Open a terminal.
2. Go to the build directory.
3. `./sercom_test --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`

### Windows
1. Open PowerShell 7 with the path of the build steps.
2. Go to the build directory.
3. `./sercom_test.exe --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`

## Contribution guidelines
Contributions follow the rules below, the layout of the repository and the tools are described in [docs/OVERVIEW.md](docs/OVERVIEW.md).
1. Fork the project.
2. Clone the fork.
3. Create a branch named `username-YYYY.MM.DD-NN`, never commit on the main branch.
4. Group the commits by file and name them as: `FileName: A small comment for the commit` (a header and its source share the name).
5. Codes must not contain any warnings or errors.
6. Every contribution comes with its tests.
7. A contribution must not break the auto tests.
8. Codes must be formatted with: `clang-format --style=file:tools/analysis/clang-format.yaml -i <file>`
9. Host codes use Qt 5.12.12 and contain no platform branches, `core/`, `app/` and `firmware/common/` stay plain C++ without Qt.
10. Codes must be checked with `cppcheck` and `clang-tidy` through `tools/run_static_analysis.sh` and must not contain any warnings.
11. Codes must follow the coding and commenting style of [docs/STYLE.md](docs/STYLE.md), `python3 tools/analysis/check_style.py` checks its countable rules.
12. Push the branch to the fork.
13. Open a pull request.
14. Wait for the tests to pass on Linux and Windows (changes of timing need at least 10 consecutive passing runs).
15. Wait for the approval.

## Contributors
### [mustafa-gokce](https://github.com/mustafa-gokce) - Maintainer, Developer
