#
# run_consumers.ps1
#
# Build and run the consumer projects of examples 28 and 29 against this checkout of sercom.
#
# The PowerShell twin of tools/run_consumers.sh for Windows, with the compiler, Ninja and Qt on the path of the README.
# Example 28 adds the checkout with add_subdirectory and is configured with Qt disabled, so sercom must not ask for Qt.
# CMake then warns that the two variables that disable Qt were not used, no find_package of Qt ran.
# Example 29 takes the checkout through FETCHCONTENT_SOURCE_DIR_SERCOM in place of the download and uses Qt 5.12.12.
# A third consumer builds the source of example 28 without a C++ standard of its own, sercom must require C++17 of it.
# Both builds export their compile commands, and no command may hold -Werror or -Wold-style-cast of sercom.
# Example 29 runs on the first com0com pair against the device simulator and is skipped when the pair is missing.
# CMAKE_PREFIX_PATH names the Qt and CXX the compiler, as CMake reads them.
# It needs PowerShell 7 (pwsh), the Windows PowerShell 5.1 of the system is too old.
#
# usage: tools/run_consumers.ps1 <sercom_device_sim> [--build-dir DIR]
#

#Requires -Version 7.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# settings with overridable defaults
$SOURCE_DIRECTORY = Split-Path -Parent $PSScriptRoot
$BUILD_DIRECTORY = if ($env:BUILD_DIRECTORY) { $env:BUILD_DIRECTORY } else {
    Join-Path $SOURCE_DIRECTORY 'build-consumers'
}
$BUILD_TYPE = if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { 'Debug' }
$GENERATOR = if ($env:CMAKE_GENERATOR) { $env:CMAKE_GENERATOR } else { 'Ninja' }
$TIMEOUT_SECONDS = if ($env:TIMEOUT_SECONDS) { [int]$env:TIMEOUT_SECONDS } else { 60 }
$PORT_PAIR = if ($env:PORT_PAIR) { $env:PORT_PAIR } else { 'COM10,COM11' }
$EXECUTABLE_SUFFIX = if ($IsWindows) { '.exe' } else { '' }
$ADD_SUBDIRECTORY_PROJECT = '28_consume_with_add_subdirectory'
$FETCH_CONTENT_PROJECT = '29_consume_with_fetch_content'
$STANDARD_PROBE_PROJECT = 'consume_without_cxx_standard'

# print the usage text
function print_usage {
    Write-Output 'usage: tools/run_consumers.ps1 <sercom_device_sim> [--build-dir DIR]'
    Write-Output '  sercom_device_sim  device simulator of the main build, example 29 pings it'
    Write-Output "  --build-dir DIR    build directory of the projects (default $BUILD_DIRECTORY)"
    Write-Output "  BUILD_TYPE sets the build type (default $BUILD_TYPE), CMAKE_GENERATOR the generator (default"
    Write-Output "  $GENERATOR), TIMEOUT_SECONDS stops a program after that time (default $TIMEOUT_SECONDS), PORT_PAIR"
    Write-Output "  names the com0com pair of example 29 (default $PORT_PAIR), CMAKE_PREFIX_PATH names Qt 5.12.12"
}

# parse the arguments
$simulatorArgument = ''
for ($index = 0; $index -lt $args.Count; $index += 1) {
    $argument = $args[$index]
    if ($argument -eq '--help') {
        print_usage
        exit 0
    }
    if ($argument -eq '--build-dir' -and $index + 1 -lt $args.Count) {
        $index += 1
        $BUILD_DIRECTORY = $args[$index]
        continue
    }
    if ($argument.StartsWith('-')) {
        [Console]::Error.WriteLine("unknown option: $argument")
        print_usage
        exit 64
    }
    if ($simulatorArgument -ne '') {
        [Console]::Error.WriteLine("unexpected argument: $argument")
        print_usage
        exit 64
    }
    $simulatorArgument = $argument
}

# device simulator must be given
if ($simulatorArgument -eq '') {
    [Console]::Error.WriteLine('missing argument: <sercom_device_sim> is required')
    print_usage
    exit 64
}
if (-not (Test-Path $simulatorArgument -PathType Leaf)) {
    [Console]::Error.WriteLine("$simulatorArgument is not found")
    exit 66
}
$SIMULATOR = (Resolve-Path $simulatorArgument).Path
$FIRST_PORT, $SECOND_PORT = $PORT_PAIR.Split(',')
$WORK_DIRECTORY = Join-Path ([System.IO.Path]::GetTempPath()) "sercom-consumers-$PID"

# log the Qt messages to standard error also without a console window
$env:QT_FORCE_STDERR_LOGGING = '1'

# start a program with its output in a log file and its errors next to it
function start_program([string] $program, [string[]] $arguments, [string] $logFile) {
    $startArguments = @{
        FilePath = $program
        PassThru = $true
        NoNewWindow = $true
        RedirectStandardOutput = $logFile
        RedirectStandardError = "$logFile.err"
    }
    if ($arguments.Count -gt 0) {
        $startArguments.ArgumentList = $arguments
    }
    $process = Start-Process @startArguments
    $null = $process.Handle
    return $process
}

# wait until a program exits, stop it after the timeout and expose its exit code
function wait_for_program([System.Diagnostics.Process] $process) {
    if (-not $process.WaitForExit($TIMEOUT_SECONDS * 1000)) {
        Stop-Process -Id $process.Id -Force
        return 124
    }
    $process.WaitForExit()
    return $process.ExitCode
}

# wait until a file holds a line, fail after five seconds
function wait_for_line([string] $file, [string] $text) {
    foreach ($attempt in 1..100) {
        if ((Test-Path $file) -and (Select-String -Path $file -Pattern $text -SimpleMatch -Quiet)) {
            return $true
        }
        Start-Sleep -Milliseconds 50
    }
    [Console]::Error.WriteLine("$file did not show $text")
    return $false
}

# configure and build a consumer project with its compile commands, stop on a failure
function build_project([string] $project, [string[]] $options) {
    Write-Output "build    $project"
    $projectBuildDirectory = Join-Path $BUILD_DIRECTORY $project
    & cmake -S (Join-Path $SOURCE_DIRECTORY "examples/$project") -B $projectBuildDirectory -G $GENERATOR `
            "-DCMAKE_BUILD_TYPE=$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON @options
    if ($LASTEXITCODE -ne 0) {
        [Console]::Error.WriteLine("failed   $project (configure exit code $LASTEXITCODE)")
        exit 1
    }
    & cmake --build $projectBuildDirectory --parallel
    if ($LASTEXITCODE -ne 0) {
        [Console]::Error.WriteLine("failed   $project (build exit code $LASTEXITCODE)")
        exit 1
    }
}

# write, configure and build a consumer of the example 28 source without a C++ standard of its own
function build_standard_probe {
    Write-Output "build    $STANDARD_PROBE_PROJECT"
    $probeDirectory = Join-Path $BUILD_DIRECTORY $STANDARD_PROBE_PROJECT
    $sourceDirectory = $SOURCE_DIRECTORY -replace '\\', '/'
    $source = "$sourceDirectory/examples/$ADD_SUBDIRECTORY_PROJECT/$ADD_SUBDIRECTORY_PROJECT.cpp"
    New-Item -ItemType Directory -Force -Path $probeDirectory | Out-Null
    Set-Content -Path (Join-Path $probeDirectory 'CMakeLists.txt') -Value @(
        'cmake_minimum_required(VERSION 3.16)'
        "project($STANDARD_PROBE_PROJECT LANGUAGES CXX)"
        "add_subdirectory(""$sourceDirectory"" sercom EXCLUDE_FROM_ALL)"
        "add_executable($STANDARD_PROBE_PROJECT"
        "    ""$source"""
        ')'
        "target_link_libraries($STANDARD_PROBE_PROJECT PRIVATE"
        '    sercom::settings'
        '    sercom::core'
        '    sercom::app'
        ')'
    )
    $probeBuildDirectory = Join-Path $probeDirectory 'build'
    & cmake -S $probeDirectory -B $probeBuildDirectory -G $GENERATOR "-DCMAKE_BUILD_TYPE=$BUILD_TYPE" `
            -DCMAKE_DISABLE_FIND_PACKAGE_QT=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON
    if ($LASTEXITCODE -ne 0) {
        [Console]::Error.WriteLine("failed   $STANDARD_PROBE_PROJECT (configure exit code $LASTEXITCODE)")
        exit 1
    }
    & cmake --build $probeBuildDirectory --parallel
    if ($LASTEXITCODE -ne 0) {
        [Console]::Error.WriteLine("failed   $STANDARD_PROBE_PROJECT (build exit code $LASTEXITCODE)")
        exit 1
    }
    Write-Output "passed   ${STANDARD_PROBE_PROJECT}: the sercom headers build with the C++17 of sercom::settings"
}

# check that no compile command of a consumer project holds a warning flag of sercom
function test_compile_commands([string] $project) {
    $compileCommands = Join-Path $BUILD_DIRECTORY "$project/compile_commands.json"
    $commands = @(Get-Content $compileCommands | Where-Object { $_ -match '"command"' })
    $flaggedCommands = @($commands | Where-Object { $_ -match '-Werror|-Wold-style-cast' })
    $flags = '-Werror or -Wold-style-cast'
    if ($commands.Count -eq 0 -or $flaggedCommands.Count -ne 0) {
        $countText = "$($flaggedCommands.Count) of $($commands.Count)"
        [Console]::Error.WriteLine("failed   ${project}: $countText compile commands hold $flags")
        return $false
    }
    [Console]::Out.WriteLine("passed   ${project}: none of $($commands.Count) compile commands holds $flags")
    return $true
}

# run a consumer program and check that the device answered its ping
function test_consumer_program([string] $program, [string[]] $arguments, [string] $logFile) {
    $exitCode = wait_for_program (start_program $program $arguments $logFile)
    Get-Content $logFile, "$logFile.err" -ErrorAction SilentlyContinue | ForEach-Object {
        [Console]::Out.WriteLine($_)
    }
    if ($exitCode -ne 0) {
        return $false
    }

    # device must have answered the ping
    return [bool](Select-String -Path $logFile -Pattern '^Ping answered by ' -Quiet)
}

# run the consumer project of example 29 on the port pair against the device simulator
function test_fetch_content_project {
    $simulatorLogFile = Join-Path $WORK_DIRECTORY 'simulator.log'
    $simulator = start_program $SIMULATOR @($SECOND_PORT) $simulatorLogFile
    try {
        if (-not (wait_for_line "$simulatorLogFile.err" 'Started the DeviceSimulator')) {
            return $false
        }
        $program = Join-Path $BUILD_DIRECTORY "$FETCH_CONTENT_PROJECT/consume_with_fetch_content$EXECUTABLE_SUFFIX"
        return test_consumer_program $program @($FIRST_PORT) (Join-Path $WORK_DIRECTORY "$FETCH_CONTENT_PROJECT.log")
    } finally {
        Stop-Process -Id $simulator.Id -Force -ErrorAction SilentlyContinue
    }
}

# check whether both ports of the com0com pair open for example 29
function test_port_pair {
    try {
        foreach ($portName in @($FIRST_PORT, $SECOND_PORT)) {
            $port = New-Object System.IO.Ports.SerialPort $portName, 115200
            $port.Open()
            $port.Close()
        }
        return $true
    } catch {
        return $false
    }
}

# build example 28 with Qt disabled, sercom must configure without it
build_project $ADD_SUBDIRECTORY_PROJECT @('-DCMAKE_DISABLE_FIND_PACKAGE_QT=ON', '-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON')

# build example 29 with this checkout in place of the download
build_project $FETCH_CONTENT_PROJECT @("-DFETCHCONTENT_SOURCE_DIR_SERCOM=$SOURCE_DIRECTORY")

# build a consumer without a C++ standard of its own
build_standard_probe

# check the compile commands of both projects
$failedSteps = @()
foreach ($project in @($ADD_SUBDIRECTORY_PROJECT, $FETCH_CONTENT_PROJECT)) {
    if (-not (test_compile_commands $project)) {
        $failedSteps += "$project compile commands"
    }
}

# run both projects
$skippedCount = 0
New-Item -ItemType Directory -Path $WORK_DIRECTORY | Out-Null
try {
    # run example 28
    $program = Join-Path $BUILD_DIRECTORY "$ADD_SUBDIRECTORY_PROJECT/consume_with_add_subdirectory$EXECUTABLE_SUFFIX"
    if (test_consumer_program $program @() (Join-Path $WORK_DIRECTORY "$ADD_SUBDIRECTORY_PROJECT.log")) {
        Write-Output "passed   $ADD_SUBDIRECTORY_PROJECT"
    } else {
        [Console]::Error.WriteLine("failed   $ADD_SUBDIRECTORY_PROJECT")
        $failedSteps += $ADD_SUBDIRECTORY_PROJECT
    }

    # run example 29 when the com0com pair is there
    if (-not (test_port_pair)) {
        Write-Output "skipped  $FETCH_CONTENT_PROJECT (com0com pair $PORT_PAIR is not found)"
        $skippedCount = 1
    } elseif (test_fetch_content_project) {
        Write-Output "passed   $FETCH_CONTENT_PROJECT"
    } else {
        [Console]::Error.WriteLine("failed   $FETCH_CONTENT_PROJECT")
        $failedSteps += $FETCH_CONTENT_PROJECT
    }
} finally {
    Remove-Item -Recurse -Force $WORK_DIRECTORY -ErrorAction SilentlyContinue
}

# notify the user
if ($failedSteps.Count -gt 0) {
    [Console]::Error.WriteLine("consumers failed: $($failedSteps -join ',')")
    exit 1
}
Write-Output "consumers passed: $(3 - $skippedCount), skipped: $skippedCount"
