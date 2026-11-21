#
# run_examples.ps1
#
# Run every example program of a build directory and fail when one of them exits with a code other than 0.
#
# The PowerShell twin of tools/run_examples.sh for Windows, it builds nothing either.
# Example 01 runs on the first com0com pair, must print a received text and stops on Ctrl+C.
# Example 24 runs on the same pair against the device simulator on the far end.
# Both are skipped with a message when the pair is missing, tools/COM0COM.md creates it.
# Every example runs before the exit code is set.
# It needs PowerShell 7 (pwsh), the Windows PowerShell 5.1 of the system is too old.
#
# usage: tools/run_examples.ps1 [build directory]
#

#Requires -Version 7.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# settings with overridable defaults
$SOURCE_DIRECTORY = Split-Path -Parent $PSScriptRoot
$TIMEOUT_SECONDS = if ($env:TIMEOUT_SECONDS) { [int]$env:TIMEOUT_SECONDS } else { 60 }
$RAW_SERIAL_SECONDS = if ($env:RAW_SERIAL_SECONDS) { [int]$env:RAW_SERIAL_SECONDS } else { 2 }
$PORT_PAIR = if ($env:PORT_PAIR) { $env:PORT_PAIR } else { 'COM10,COM11' }
$EXECUTABLE_SUFFIX = if ($IsWindows) { '.exe' } else { '' }

# print the usage text
function print_usage {
    Write-Output 'usage: tools/run_examples.ps1 [build directory]'
    Write-Output '  build directory  CMake build directory with the example programs (default build)'
    Write-Output "  TIMEOUT_SECONDS stops an example after that time (default $TIMEOUT_SECONDS)"
    Write-Output "  RAW_SERIAL_SECONDS sets the run time of example 01 before Ctrl+C (default $RAW_SERIAL_SECONDS)"
    Write-Output "  PORT_PAIR names the com0com pair of examples 01 and 24 (default $PORT_PAIR)"
}

# check the arguments
if ($args.Count -ge 1 -and $args[0] -eq '--help') {
    print_usage
    exit 0
}
if ($args.Count -gt 1) {
    [Console]::Error.WriteLine("unexpected argument: $($args[1])")
    print_usage
    exit 64
}

# select the build directory and the ports
$BUILD_DIRECTORY = (Resolve-Path $(if ($args.Count -ge 1) { $args[0] } else { Join-Path $SOURCE_DIRECTORY 'build' })).Path
$FIRST_PORT, $SECOND_PORT = $PORT_PAIR.Split(',')
$WORK_DIRECTORY = Join-Path ([System.IO.Path]::GetTempPath()) "sercom-examples-$PID"
New-Item -ItemType Directory -Path $WORK_DIRECTORY | Out-Null

# log the Qt messages to standard error also without a console window
$env:QT_FORCE_STDERR_LOGGING = '1'

# send Ctrl+C to the console of a program, the helper attaches to that console alone
$SEND_CONTROL_C = @'
Add-Type -Namespace Sercom -Name ConsoleControl -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern bool FreeConsole();
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern bool AttachConsole(uint processId);
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern bool SetConsoleCtrlHandler(System.IntPtr handlerRoutine, bool add);
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern bool GenerateConsoleCtrlEvent(uint controlEvent, uint processGroupId);
"@
[Sercom.ConsoleControl]::FreeConsole() | Out-Null
if (-not [Sercom.ConsoleControl]::AttachConsole(PROCESS_ID)) { exit 1 }
[Sercom.ConsoleControl]::SetConsoleCtrlHandler([IntPtr]::Zero, $true) | Out-Null
if (-not [Sercom.ConsoleControl]::GenerateConsoleCtrlEvent(0, 0)) { exit 1 }
'@

# start a program with its output in a log file and its errors next to it, in a hidden console on request
function start_program([string] $program, [string[]] $arguments, [string] $logFile, [bool] $ownConsole = $false) {
    $startArguments = @{
        FilePath = $program
        PassThru = $true
        RedirectStandardOutput = $logFile
        RedirectStandardError = "$logFile.err"
    }
    if ($ownConsole) {
        $startArguments.WindowStyle = 'Hidden'
    } else {
        $startArguments.NoNewWindow = $true
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

# run an example that needs no serial port
function run_example([string] $program, [string] $logFile) {
    return wait_for_program (start_program $program @() $logFile)
}

# run the raw serial example on the port pair, send it a text and stop it with Ctrl+C
function run_raw_serial_example([string] $program, [string] $logFile) {
    $process = start_program $program @($FIRST_PORT) $logFile $true
    if (-not (wait_for_line $logFile 'Listening on')) {
        Stop-Process -Id $process.Id -Force
        return 1
    }
    $port = New-Object System.IO.Ports.SerialPort $SECOND_PORT, 115200
    $port.Open()
    $port.Write('hello')
    Start-Sleep -Seconds $RAW_SERIAL_SECONDS
    $port.Close()

    # send Ctrl+C to the console of the example
    $helperScript = $SEND_CONTROL_C.Replace('PROCESS_ID', [string]$process.Id)
    $encodedHelperScript = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($helperScript))
    $helper = Start-Process -FilePath (Get-Process -Id $PID).Path `
                            -ArgumentList @('-NoProfile', '-EncodedCommand', $encodedHelperScript) `
                            -WindowStyle Hidden -PassThru -Wait
    if ($helper.ExitCode -ne 0) {
        [Console]::Error.WriteLine("Ctrl+C did not reach the console of $program")
    }
    $exitCode = wait_for_program $process
    if ($exitCode -ne 0) {
        return $exitCode
    }

    # raw serial example must print the received bytes
    if (-not (Select-String -Path $logFile -Pattern "Received: 0x68 'h'" -SimpleMatch -Quiet)) {
        return 1
    }
    return 0
}

# run the serial ping example on the port pair against the device simulator
function run_serial_ping_example([string] $program, [string] $logFile) {
    $simulatorLogFile = Join-Path $WORK_DIRECTORY 'serial_ping_simulator.log'
    $simulator = start_program (Join-Path $BUILD_DIRECTORY "sercom_device_sim$EXECUTABLE_SUFFIX") @($SECOND_PORT) $simulatorLogFile
    try {
        if (-not (wait_for_line "$simulatorLogFile.err" 'Started the DeviceSimulator')) {
            return 1
        }
        return wait_for_program (start_program $program @($FIRST_PORT) $logFile)
    } finally {
        Stop-Process -Id $simulator.Id -Force -ErrorAction SilentlyContinue
    }
}

# check whether both ports of the com0com pair open for the examples with a serial port
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
$pairFound = test_port_pair

# run every example of the examples directory in the order of its number
$failedExamples = @()
$skippedExamples = @()
$passedCount = 0
try {
    foreach ($sourceFile in Get-ChildItem (Join-Path $SOURCE_DIRECTORY 'examples') -Filter '*.cpp' | Sort-Object Name) {
        if ($sourceFile.Name -notmatch '^[0-9][0-9]_') {
            continue
        }
        $exampleName = $sourceFile.BaseName
        $programName = "sercom_example_$exampleName"
        $program = Join-Path $BUILD_DIRECTORY "$programName$EXECUTABLE_SUFFIX"
        $logFile = Join-Path $WORK_DIRECTORY "$programName.log"

        # program must be built
        if (-not (Test-Path $program)) {
            [Console]::Error.WriteLine("failed   $programName (not found in $BUILD_DIRECTORY)")
            $failedExamples += $programName
            continue
        }

        # select the runner of the example, the serial port examples need the com0com pair
        $runner = switch ($exampleName) {
            '01_raw_serial' { 'run_raw_serial_example' }
            '24_serial_ping' { 'run_serial_ping_example' }
            default { 'run_example' }
        }
        if ($runner -ne 'run_example' -and -not $pairFound) {
            Write-Output "skipped  $programName (com0com pair $PORT_PAIR is not found)"
            $skippedExamples += $programName
            continue
        }

        # run the example and keep its exit code
        $exitCode = & $runner $program $logFile
        if ($exitCode -ne 0) {
            Get-Content $logFile, "$logFile.err" -ErrorAction SilentlyContinue | ForEach-Object { [Console]::Error.WriteLine($_) }
            [Console]::Error.WriteLine("failed   $programName (exit code $exitCode)")
            $failedExamples += $programName
            continue
        }
        Write-Output "passed   $programName"
        $passedCount += 1
    }
} finally {
    Remove-Item -Recurse -Force $WORK_DIRECTORY -ErrorAction SilentlyContinue
}

# notify the user
if ($failedExamples.Count -gt 0) {
    [Console]::Error.WriteLine("examples failed: $($failedExamples -join ',')")
    exit 1
}
Write-Output "examples passed: $passedCount, skipped: $($skippedExamples.Count)"
