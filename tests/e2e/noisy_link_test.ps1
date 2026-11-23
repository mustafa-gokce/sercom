#
# noisy_link_test.ps1
#
# Run the command line tool against the device simulator over a noisy link of two com0com pairs.
#
# The PowerShell twin of tests/e2e/noisy_link_test.sh for Windows, tools/COM0COM.md creates the pairs.
# The link comes from tools/noisy_link.ps1 with a fixed seed, so a failing run can be replayed.
# Ping, info, LED and an echo stress test must succeed, the stress test must verify every echo.
# Each command meets a fresh simulator, because version 1 has no sessions (docs/PROTOCOL.md, section 7):
# a simulator that served an earlier tool process could take a retransmission for a duplicate.
# A traced ping must show the frames of both directions in the tool and in the simulator.
# Only its simulator traces, because a traced simulator writes every frame to its log on its one thread:
# a slow disk under the log would slow the device down, and the stress test must not measure the disk.
# A late answer to the boot ping of tests/e2e/late_ping_device.py must not count for the ping command,
# and a failed boot ping must not fail it; the device runs on the inner port of the first pair.
# An info command on a serial port that does not exist must fail with exit code 1.
# It needs PowerShell 7 (pwsh), the Windows PowerShell 5.1 of the system is too old.
#
# usage: tests/e2e/noisy_link_test.ps1 <sercom_cli> <sercom_device_sim> [profile] [count]
#

#Requires -Version 7.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# settings with overridable defaults
$SEED = if ($env:SEED) { $env:SEED } else { '1000' }
$PAYLOAD_SIZE = if ($env:PAYLOAD_SIZE) { $env:PAYLOAD_SIZE } else { '32' }
$WINDOW_SIZE = if ($env:WINDOW_SIZE) { $env:WINDOW_SIZE } else { '2' }
$PORTS = if ($env:PORTS) { $env:PORTS } else { 'COM10,COM11,COM12,COM13' }
$MISSING_PORT = if ($env:MISSING_PORT) { $env:MISSING_PORT } else { 'COM99' }
$PYTHON = if ($env:PYTHON) { $env:PYTHON } else { 'python' }
$SOURCE_DIRECTORY = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

# print the usage text
function print_usage {
    Write-Output 'usage: tests/e2e/noisy_link_test.ps1 <sercom_cli> <sercom_device_sim> [profile] [count]'
    Write-Output '  profile  noise profile of tools/noisy_link.ps1 (default realistic)'
    Write-Output '  count    number of echo requests of the stress test (default 300)'
    Write-Output "  SEED sets the noise seed (default $SEED), PAYLOAD_SIZE the echo payload size in bytes"
    Write-Output "  (default $PAYLOAD_SIZE), WINDOW_SIZE the echo requests in flight (default $WINDOW_SIZE),"
    Write-Output "  PORTS the ports of tools/noisy_link.ps1 (default $PORTS) and MISSING_PORT a port"
    Write-Output "  that does not exist (default $MISSING_PORT)"
}

# check the arguments
if ($args.Count -ge 1 -and $args[0] -eq '--help') {
    print_usage
    exit 0
}
if ($args.Count -lt 2) {
    [Console]::Error.WriteLine('missing argument: <sercom_cli> and <sercom_device_sim> are required')
    print_usage
    exit 64
}

# settings of the arguments
$CLI = (Resolve-Path $args[0]).Path
$SIMULATOR = (Resolve-Path $args[1]).Path
$LINK_PROFILE = if ($args.Count -ge 3) { $args[2] } else { 'realistic' }
$COUNT = if ($args.Count -ge 4) { $args[3] } else { '300' }
$FIRST_PORT, $FIRST_NOISE_PORT, $SECOND_NOISE_PORT, $SECOND_PORT = $PORTS.Split(',')
$WORK_DIRECTORY = Join-Path ([System.IO.Path]::GetTempPath()) "sercom-e2e-$PID"
New-Item -ItemType Directory -Path $WORK_DIRECTORY | Out-Null

# log the Qt messages to standard error also without a console window
$env:QT_FORCE_STDERR_LOGGING = '1'

# start a background process with its output and its errors in log files
function start_process([string] $program, [string[]] $arguments, [string] $logFile) {
    $process = Start-Process -FilePath $program -ArgumentList $arguments -NoNewWindow -PassThru `
                             -RedirectStandardOutput $logFile -RedirectStandardError "$logFile.err"
    $null = $process.Handle
    return $process
}

# stop a background process together with the processes it started
function stop_process([System.Diagnostics.Process] $process) {
    if (-not $process.HasExited) {
        $process.Kill($true)
    }
    $null = $process.WaitForExit(5000)
}

# wait until a log file holds a line, fail after five seconds
function wait_for_line([string] $logFile, [string] $text) {
    foreach ($attempt in 1..100) {
        if ((Test-Path $logFile) -and (Select-String -Path $logFile -Pattern $text -SimpleMatch -Quiet)) {
            return
        }
        Start-Sleep -Milliseconds 50
    }
    throw "$text did not appear in $logFile"
}

# require a line of a file, print the file and fail otherwise
function require_line([string] $file, [string] $pattern) {
    if (Select-String -Path $file -Pattern $pattern -CaseSensitive -Quiet) {
        return
    }
    [Console]::Error.WriteLine("$pattern did not appear in ${file}:")
    Get-Content $file | ForEach-Object { [Console]::Error.WriteLine($_) }
    throw "missing line in $file"
}

# require a number of lines of a file, print the file and fail otherwise
function require_line_count([string] $file, [string] $pattern, [int] $count) {
    $foundCount = @(Select-String -Path $file -Pattern $pattern -CaseSensitive).Count
    if ($foundCount -eq $count) {
        return
    }
    [Console]::Error.WriteLine("$pattern appeared $foundCount times instead of $count in ${file}:")
    Get-Content $file | ForEach-Object { [Console]::Error.WriteLine($_) }
    throw "wrong line count in $file"
}

# start a fresh device simulator with the given options and log files of its own and wait until it is started
$simulatorCount = 0
function start_simulator([string[]] $options = @()) {
    $script:simulatorCount += 1
    $script:simulatorLogFile = Join-Path $WORK_DIRECTORY "sim$($script:simulatorCount).log"
    $script:simulatorProcess = start_process $SIMULATOR (@($SECOND_PORT, '--name', 'e2e-device') + $options) `
                                             $script:simulatorLogFile
    $script:processes += $script:simulatorProcess
    wait_for_line "$($script:simulatorLogFile).err" 'Started the DeviceSimulator'
}

# run one command of the command line tool against a fresh device simulator, any failure stops the test
function run([string[]] $arguments) {
    start_simulator
    Write-Output "+ sercom_cli $($arguments -join ' ')"
    & $CLI $FIRST_PORT @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "sercom_cli $($arguments -join ' ') exited with $LASTEXITCODE"
    }
    stop_process $script:simulatorProcess
}

# stop every background process and remove the work directory on exit
$processes = @()
try {
    # start the noisy link and wait until it is ready
    $linkScript = Join-Path $SOURCE_DIRECTORY 'tools/noisy_link.ps1'
    $processes += start_process (Get-Process -Id $PID).Path @('-NoProfile', '-File', $linkScript, '--profile',
                                                             $LINK_PROFILE, '--ports', $PORTS, '--seed', $SEED) `
                                (Join-Path $WORK_DIRECTORY 'link.log')
    wait_for_line (Join-Path $WORK_DIRECTORY 'link.log') 'Noisy link ready'
    $linkProcess = $processes[-1]

    # run the commands
    run @('ping', '3')
    run @('info')
    run @('led', 'on')
    run @('led', 'off')
    run @('stress', $COUNT, $PAYLOAD_SIZE, $WINDOW_SIZE)

    # run a traced ping against a fresh simulator and check the frames, the boot wait and the single ping response
    start_simulator @('--trace')
    Write-Output '+ sercom_cli --trace ping 1'
    $traceOutput = Join-Path $WORK_DIRECTORY 'trace.out'
    $traceErrors = Join-Path $WORK_DIRECTORY 'trace.err'
    & $CLI $FIRST_PORT --trace ping 1 > $traceOutput 2> $traceErrors
    if ($LASTEXITCODE -ne 0) {
        throw "sercom_cli --trace ping 1 exited with $LASTEXITCODE"
    }
    stop_process $simulatorProcess
    require_line $traceOutput '^\[ *[0-9]+\.[0-9]{3}\] >> #[0-9]+ +PING_REQUEST .*CRC ok$'
    require_line $traceOutput '^\[ *[0-9]+\.[0-9]{3}\] << #[0-9]+ +PING_RESPONSE .*CRC ok$'
    require_line_count $traceOutput '^ping response: ' 1
    require_line $traceErrors 'Device answered after [0-9]+ ms'

    # check the same frames in the opposite directions in the log of its simulator
    require_line $simulatorLogFile '^\[ *[0-9]+\.[0-9]{3}\] >> #[0-9]+ +PING_REQUEST .*CRC ok$'
    require_line $simulatorLogFile '^\[ *[0-9]+\.[0-9]{3}\] << #[0-9]+ +PING_RESPONSE .*CRC ok$'

    # free the inner port of the first pair for the late ping device
    stop_process $linkProcess

    # check in both modes of the late ping device that only the ping of the command counts
    foreach ($mode in @('late', 'failed')) {
        $deviceLogFile = Join-Path $WORK_DIRECTORY "$mode.log"
        $deviceScript = Join-Path $SOURCE_DIRECTORY 'tests/e2e/late_ping_device.py'
        $processes += start_process $PYTHON @('-u', $deviceScript, $FIRST_NOISE_PORT, '--mode', $mode) $deviceLogFile
        wait_for_line $deviceLogFile 'Late ping device ready'
        Write-Output "+ sercom_cli late_ping_device --mode $mode ping 1"
        $modeOutput = Join-Path $WORK_DIRECTORY "$mode.out"
        & $CLI $FIRST_PORT ping 1 > $modeOutput
        if ($LASTEXITCODE -ne 0) {
            throw "sercom_cli ping 1 against the $mode device exited with $LASTEXITCODE"
        }
        require_line $modeOutput '^ping response: .*device uptime 2222 ms$'
        require_line_count $modeOutput '^ping response: ' 1
        stop_process $processes[-1]
    }

    # check that a missing serial port fails
    Write-Output '+ sercom_cli missing_port info'
    & $CLI $MISSING_PORT info
    if ($LASTEXITCODE -ne 1) {
        throw "a missing serial port gave exit code $LASTEXITCODE instead of 1"
    }
} finally {
    foreach ($process in $processes) {
        stop_process $process
    }
    Remove-Item -Recurse -Force $WORK_DIRECTORY -ErrorAction SilentlyContinue
}

# notify the user
Write-Output "e2e ($LINK_PROFILE) passed"
