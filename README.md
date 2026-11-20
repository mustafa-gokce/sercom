# sercom

Packet Based Binary Serial Protocol for Linux Hosts and ESP32 Devices

## Building the project
1. Clone the project with: `git clone https://github.com/mustafa-gokce/sercom.git`
2. Go to the source directory.
3. Populate dependencies: `git submodule update --init --recursive`

### Linux
1. Open a terminal.
2. Install the tools: `sudo apt install build-essential cmake ninja-build python3 socat gdb git`
3. Go to the source directory.
4. `mkdir build && cd build`
5. `cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
6. `cmake --build .`

## Run tests
### Linux
1. Open a terminal.
2. Go to the build directory.
3. `./sercom_test --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`

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
9. Codes must be checked with `cppcheck` and `clang-tidy` through `tools/run_static_analysis.sh` and must not contain any warnings.
10. Codes must follow the coding and commenting style of [docs/STYLE.md](docs/STYLE.md), `python3 tools/analysis/check_style.py` checks its countable rules.
11. Push the branch to the fork.
12. Open a pull request.
13. Wait for the tests to pass (changes of timing need at least 10 consecutive passing runs).
14. Wait for the approval.

## Contributors
### [mustafa-gokce](https://github.com/mustafa-gokce) - Maintainer, Developer
