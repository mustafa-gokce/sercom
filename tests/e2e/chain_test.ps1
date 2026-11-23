#
# chain_test.ps1
#
# Run the command line tool against a daisy chain of three device simulators over com0com pairs.
#
# The PowerShell twin of tests/e2e/chain_test.sh for Windows, tools/COM0COM.md creates the pairs.
# Node 2 and node 3 are routers, node 4 is the end of the chain and node 3 stands in for it as its proxy.
# The pairs join the links, tools/noisy_link.ps1 adds noise with a fixed seed between node 2 and node 3.
# discover must name every node, and --to 4 must reach node 4 for info, ping and a stress test without a loss.
# A ping of a missing node must fail as REJECTED / UNREACHABLE and one with two links to live as REJECTED / EXPIRED.
# A route failure report is fire and forget and the noise may drop it, then the host only sees a timeout.
# So these two pings run again while their report is lost, at most REPORT_ATTEMPTS times.
# A ping without --to must reach node 2, a wrong destination or time to live must fail with exit code 64.
# Node 4 restarted without an address must answer --to 4 info through the proxy of node 3.
# END_DEVICE_SIMULATOR may name another simulator for the restarted node 4, for example one of version 1.
# It needs PowerShell 7 (pwsh), the Windows PowerShell 5.1 of the system is too old.
#
# usage: tests/e2e/chain_test.ps1 <sercom_cli> <sercom_device_sim> [profile] [count]
#

#Requires -Version 7.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# settings with overridable defaults
$SEED = if ($env:SEED) { $env:SEED } else { '1000' }
$PAYLOAD_SIZE = if ($env:PAYLOAD_SIZE) { $env:PAYLOAD_SIZE } else { '32' }
$WINDOW_SIZE = if ($env:WINDOW_SIZE) { $env:WINDOW_SIZE } else { '2' }
$REPORT_ATTEMPTS = if ($env:REPORT_ATTEMPTS) { [int] $env:REPORT_ATTEMPTS } else { 5 }
$PORTS = if ($env:PORTS) { $env:PORTS } else { 'COM10,COM11,COM12,COM13,COM14,COM15,COM16,COM17' }
$SOURCE_DIRECTORY = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

# print the usage text
function print_usage {
    Write-Output 'usage: tests/e2e/chain_test.ps1 <sercom_cli> <sercom_device_sim> [profile] [count]'
    Write-Output '  profile  noise profile of tools/noisy_link.ps1 between node 2 and node 3 (default realistic)'
    Write-Output '  count    number of echo requests of the stress test (default 300)'
    Write-Output "  SEED sets the noise seed (default $SEED), PAYLOAD_SIZE the echo payload size in bytes"
    Write-Output "  (default $PAYLOAD_SIZE), WINDOW_SIZE the echo requests in flight (default $WINDOW_SIZE),"
    Write-Output "  REPORT_ATTEMPTS the runs of a ping whose route failure report is lost (default $REPORT_ATTEMPTS),"
    Write-Output "  PORTS the four com0com pairs of the chain (default $PORTS)"
    Write-Output '  and END_DEVICE_SIMULATOR the simulator of the restarted node 4 (default <sercom_device_sim>)'
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
$END_DEVICE_SIMULATOR = if ($env:END_DEVICE_SIMULATOR) { (Resolve-Path $env:END_DEVICE_SIMULATOR).Path } else { $SIMULATOR }
$HOST_PORT, $NODE2_PORT, $NODE2_DOWNSTREAM_PORT, $FIRST_NOISE_PORT, $SECOND_NOISE_PORT, $NODE3_PORT, `
    $NODE3_DOWNSTREAM_PORT, $NODE4_PORT = $PORTS.Split(',')
$WORK_DIRECTORY = Join-Path ([System.IO.Path]::GetTempPath()) "sercom-chain-$PID"
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

# stop a device simulator so that it closes its port, a killed Qt tool leaves a pseudo terminal exclusive
function stop_simulator([System.Diagnostics.Process] $process) {
    if ($IsLinux -or $IsMacOS) {
        & kill -TERM $process.Id
        $null = $process.WaitForExit(5000)
    }
    stop_process $process
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

# require that no line of a file matches, print the file and fail otherwise
function require_no_line([string] $file, [string] $pattern) {
    if (-not (Select-String -Path $file -Pattern $pattern -CaseSensitive -Quiet)) {
        return
    }
    [Console]::Error.WriteLine("$pattern appeared in ${file}:")
    Get-Content $file | ForEach-Object { [Console]::Error.WriteLine($_) }
    throw "unexpected line in $file"
}

# start a device simulator in the background and wait until it is started
function start_simulator([string] $simulator, [string] $logFile, [string[]] $arguments) {
    $process = start_process $simulator $arguments $logFile
    wait_for_line "$logFile.err" 'Started the DeviceSimulator'
    return $process
}

# run one command of the command line tool that must end with an exit code, print its output otherwise
function run_command([string] $name, [int] $expectedExitCode, [string[]] $arguments) {
    Write-Output "+ sercom_cli $($arguments -join ' ')"
    $output = Join-Path $WORK_DIRECTORY "$name.out"
    $errors = Join-Path $WORK_DIRECTORY "$name.err"
    & $CLI $HOST_PORT @arguments > $output 2> $errors
    if ($LASTEXITCODE -ne $expectedExitCode) {
        Get-Content $output, $errors | ForEach-Object { [Console]::Error.WriteLine($_) }
        throw "sercom_cli $($arguments -join ' ') exited with $LASTEXITCODE instead of $expectedExitCode"
    }
}

# run a ping that must fail with a route failure report, again while the noise drops the report
function run_reported([string] $name, [string] $report, [string[]] $arguments) {
    $errors = Join-Path $WORK_DIRECTORY "$name.err"
    foreach ($attempt in 1..$REPORT_ATTEMPTS) {
        run_command $name 1 $arguments
        if (Select-String -Path $errors -Pattern $report -SimpleMatch -CaseSensitive -Quiet) {
            return
        }
        require_line $errors 'Ping request timed out'
        Write-Output "the route failure report was lost in attempt $attempt of $REPORT_ATTEMPTS"
    }
    throw "$report did not arrive in $REPORT_ATTEMPTS attempts"
}

# stop every background process and remove the work directory on exit
$processes = @()
try {
    # start the noisy link between node 2 and node 3
    $linkScript = Join-Path $SOURCE_DIRECTORY 'tools/noisy_link.ps1'
    $linkPorts = @($NODE2_DOWNSTREAM_PORT, $FIRST_NOISE_PORT, $SECOND_NOISE_PORT, $NODE3_PORT) -join ','
    $processes += start_process (Get-Process -Id $PID).Path @('-NoProfile', '-File', $linkScript, '--profile',
                                                             $LINK_PROFILE, '--ports', $linkPorts, '--seed', $SEED) `
                                (Join-Path $WORK_DIRECTORY 'link.log')
    wait_for_line (Join-Path $WORK_DIRECTORY 'link.log') 'Noisy link ready'

    # start the three nodes from the end of the chain up to node 2
    $node4Process = start_simulator $SIMULATOR (Join-Path $WORK_DIRECTORY 'node4.log') `
                                    @($NODE4_PORT, '--name', 'node4', '--address', '4')
    $processes += $node4Process
    $processes += start_simulator $SIMULATOR (Join-Path $WORK_DIRECTORY 'node3.log') `
                                  @($NODE3_PORT, '--name', 'node3', '--address', '3', '--downstream',
                                    $NODE3_DOWNSTREAM_PORT, '--proxy', '4')
    $processes += start_simulator $SIMULATOR (Join-Path $WORK_DIRECTORY 'node2.log') `
                                  @($NODE2_PORT, '--name', 'node2', '--address', '2', '--downstream',
                                    $NODE2_DOWNSTREAM_PORT)

    # discover every node of the chain
    run_command 'discover' 0 @('discover')
    $discoverOutput = Join-Path $WORK_DIRECTORY 'discover.out'
    require_line $discoverOutput '^node 2 answered after [0-9]+ ms$'
    require_line $discoverOutput '^node 3 answered after [0-9]+ ms$'
    require_line $discoverOutput '^node 4 answered after [0-9]+ ms$'
    require_line $discoverOutput '^discovered 3 nodes$'

    # reach node 4 over three links for info, ping and the stress test
    run_command 'info' 0 @('--to', '4', 'info')
    require_line (Join-Path $WORK_DIRECTORY 'info.out') '^device: node4$'
    require_line (Join-Path $WORK_DIRECTORY 'info.out') '^protocol version: 2$'
    require_line (Join-Path $WORK_DIRECTORY 'info.out') '^node address: 4$'
    run_command 'ping' 0 @('--to', '4', 'ping', '5')
    require_line (Join-Path $WORK_DIRECTORY 'ping.out') '^ping response from node 4: round trip [0-9]+ ms'
    run_command 'stress' 0 @('--to', '4', 'stress', $COUNT, $PAYLOAD_SIZE, $WINDOW_SIZE)
    require_line (Join-Path $WORK_DIRECTORY 'stress.out') "^  verified replies +$COUNT$"

    # check the failure reports of a missing node and of a frame without enough time to live
    run_reported 'unreachable' 'Request PING_REQUEST failed: REJECTED / UNREACHABLE' @('--to', '9', 'ping', '1')
    run_reported 'expired' 'Request PING_REQUEST failed: REJECTED / EXPIRED' @('--to', '4', '--ttl', '2', 'ping', '1')

    # reach node 2 without a destination and refuse wrong values
    run_command 'direct' 0 @('ping', '1')
    require_line (Join-Path $WORK_DIRECTORY 'direct.out') '^ping response: round trip [0-9]+ ms'
    run_command 'host_destination' 64 @('--to', '1', 'ping', '1')
    run_command 'broadcast_destination' 64 @('--to', '255', 'ping', '1')
    run_command 'no_time_to_live' 64 @('--to', '4', '--ttl', '0', 'ping', '1')

    # restart node 4 without an address behind the proxy of node 3
    stop_simulator $node4Process
    $processes += start_simulator $END_DEVICE_SIMULATOR (Join-Path $WORK_DIRECTORY 'node4_direct.log') `
                                  @($NODE4_PORT, '--name', 'node4-direct')
    run_command 'proxy' 0 @('--to', '4', 'info')
    require_line (Join-Path $WORK_DIRECTORY 'proxy.out') '^device: node4-direct$'
    require_no_line (Join-Path $WORK_DIRECTORY 'proxy.out') '^protocol version: '
} finally {
    foreach ($process in $processes) {
        stop_process $process
    }
    Remove-Item -Recurse -Force $WORK_DIRECTORY -ErrorAction SilentlyContinue
}

# check that no process is left behind
foreach ($process in $processes) {
    if (-not $process.HasExited) {
        throw "process $($process.Id) is left behind"
    }
}

# notify the user
Write-Output "chain ($LINK_PROFILE) passed"
