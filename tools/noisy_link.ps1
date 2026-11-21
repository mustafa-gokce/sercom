#
# noisy_link.ps1
#
# Connect two com0com port pairs through a full-duplex noisy link on Windows.
#
# The PowerShell twin of tools/noisy_link.sh, tools/COM0COM.md creates the pairs.
# Applications open the first port of the first pair and the second port of the second pair.
# One noise.py --duplex holds the two inner ports, because Windows opens a COM port only once.
# With a seed the directions take SEED and SEED + 1, like the two noise.py of tools/noisy_link.sh.
# The link is ready when the script prints "Noisy link ready", Ctrl+C tears it down.
# It needs PowerShell 7 (pwsh), the Windows PowerShell 5.1 of the system is too old.
#
# usage: tools/noisy_link.ps1 [--profile PROFILE] [--ports PORTS] [--seed SEED]
#

#Requires -Version 7.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# settings with overridable defaults
$SCRIPT_DIRECTORY = $PSScriptRoot
$DEFAULT_PROFILE = if ($env:PROFILE) { $env:PROFILE } else { 'moderate' }
$DEFAULT_PORTS = if ($env:PORTS) { $env:PORTS } else { 'COM10,COM11,COM12,COM13' }
$DEFAULT_SEED = if ($env:SEED) { $env:SEED } else { '' }
$PYTHON = if ($env:PYTHON) { $env:PYTHON } else { 'python' }

# print the usage text
function print_usage {
    Write-Output 'usage: tools/noisy_link.ps1 [--profile PROFILE] [--ports PORTS] [--seed SEED]'
    Write-Output "  --profile PROFILE  clean, realistic, moderate or aggressive (default $DEFAULT_PROFILE)"
    Write-Output "  --ports PORTS      application port, two inner ports and application port (default $DEFAULT_PORTS)"
    Write-Output "  --seed SEED        seed of the first direction, the second takes SEED + 1 (default $(if ($DEFAULT_SEED) { $DEFAULT_SEED } else { 'none' }))"
}

# parse the arguments
$linkProfile = $DEFAULT_PROFILE
$ports = $DEFAULT_PORTS
$seed = $DEFAULT_SEED
$index = 0
while ($index -lt $args.Count) {
    switch ($args[$index]) {
        '--profile' { $linkProfile = $args[$index + 1]; $index += 2 }
        '--ports' { $ports = $args[$index + 1]; $index += 2 }
        '--seed' { $seed = $args[$index + 1]; $index += 2 }
        '--help' { print_usage; exit 0 }
        default { [Console]::Error.WriteLine("unknown option: $($args[$index])"); print_usage; exit 64 }
    }
}

# select the noise arguments of the profile, the profiles of tools/NOISE.md
$noiseArguments = switch ($linkProfile) {
    'clean' { @() }
    'realistic' { @('--drop', '0.001', '--flip', '0.001', '--insert', '0.0005', '--duplicate', '0.0001',
                    '--maximum-bit-flips', '1', '--delay', '0.0001', '--burst', '0.0001', '--burst-length', '5') }
    'moderate' { @('--drop', '0.01', '--flip', '0.01', '--insert', '0.005', '--duplicate', '0.002',
                   '--maximum-bit-flips', '2', '--delay', '0.001', '--burst', '0.001', '--burst-length', '20') }
    'aggressive' { @('--drop', '0.05', '--flip', '0.05', '--insert', '0.02', '--duplicate', '0.01',
                     '--maximum-bit-flips', '4', '--delay', '0.005', '--burst', '0.01', '--burst-length', '50') }
    default { [Console]::Error.WriteLine("unknown profile: $linkProfile"); print_usage; exit 64 }
}

# select the four ports of the two pairs
$portNames = @($ports.Split(','))
if ($portNames.Count -ne 4) {
    [Console]::Error.WriteLine("invalid ports: $ports, expected four COM ports separated by commas")
    print_usage
    exit 64
}
$FIRST_PORT, $FIRST_NOISE_PORT, $SECOND_NOISE_PORT, $SECOND_PORT = $portNames

# select the seed argument, the return direction of noise.py --duplex takes the next seed
$seedArguments = if ($seed) { @('--seed', $seed) } else { @() }

# forward both directions between the inner ports of the pairs
$noiseArguments = @('-u', (Join-Path $SCRIPT_DIRECTORY 'noise.py'), $FIRST_NOISE_PORT, $SECOND_NOISE_PORT, '--duplex') `
                  + $noiseArguments + $seedArguments
$noiseProcess = Start-Process -FilePath $PYTHON -ArgumentList $noiseArguments -NoNewWindow -PassThru

# stop noise.py on exit
try {
    # let noise.py open both inner ports
    Start-Sleep -Seconds 1
    if ($noiseProcess.HasExited) {
        [Console]::Error.WriteLine("noise.py could not open $FIRST_NOISE_PORT and $SECOND_NOISE_PORT")
        exit 1
    }

    # notify the user
    Write-Output "Noisy link ready ($linkProfile): $FIRST_PORT <-> $SECOND_PORT (Ctrl+C to tear down)"
    $noiseProcess.WaitForExit()
} finally {
    Stop-Process -Id $noiseProcess.Id -Force -ErrorAction SilentlyContinue
}
