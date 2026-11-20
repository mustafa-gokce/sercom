#!/bin/bash

#
# noisy_link.sh
#
# Connect two virtual serial ports through a full-duplex noisy link.
#
# Applications open DIRECTORY/ttyV1 and DIRECTORY/ttyV2, tools/SOCAT.md describes the wiring.
# Each direction runs its own noise.py, with a seed the directions take SEED and SEED + 1.
# The link is ready when the script prints "Noisy link ready", Ctrl-C tears it down.
#
# usage: tools/noisy_link.sh [--profile PROFILE] [--dir DIRECTORY] [--seed SEED]
#

set -euo pipefail

# settings with overridable defaults
SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIRECTORY
readonly DEFAULT_PROFILE="${PROFILE:-moderate}"
readonly DEFAULT_LINK_DIRECTORY="${LINK_DIRECTORY:-/tmp}"
readonly DEFAULT_SEED="${SEED:-}"

# print the usage text
print_usage() {
    echo "usage: tools/noisy_link.sh [--profile PROFILE] [--dir DIRECTORY] [--seed SEED]"
    echo "  --profile PROFILE  clean, realistic, moderate or aggressive (default ${DEFAULT_PROFILE})"
    echo "  --dir DIRECTORY    directory of the port links (default ${DEFAULT_LINK_DIRECTORY})"
    echo "  --seed SEED        seed of the first direction, the second takes SEED + 1 (default ${DEFAULT_SEED:-none})"
}

# parse the arguments
PROFILE="${DEFAULT_PROFILE}"
LINK_DIRECTORY="${DEFAULT_LINK_DIRECTORY}"
SEED="${DEFAULT_SEED}"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --profile) PROFILE="$2"; shift 2 ;;
        --dir) LINK_DIRECTORY="$2"; shift 2 ;;
        --seed) SEED="$2"; shift 2 ;;
        --help) print_usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly PROFILE
readonly LINK_DIRECTORY
readonly SEED

# select the noise arguments of the profile, the profiles of tools/NOISE.md
case "${PROFILE}" in
    clean)
        NOISE_ARGUMENTS=() ;;
    realistic)
        NOISE_ARGUMENTS=(--drop 0.001 --flip 0.001 --insert 0.0005 --duplicate 0.0001
                         --maximum-bit-flips 1 --delay 0.0001 --burst 0.0001 --burst-length 5) ;;
    moderate)
        NOISE_ARGUMENTS=(--drop 0.01 --flip 0.01 --insert 0.005 --duplicate 0.002
                         --maximum-bit-flips 2 --delay 0.001 --burst 0.001 --burst-length 20) ;;
    aggressive)
        NOISE_ARGUMENTS=(--drop 0.05 --flip 0.05 --insert 0.02 --duplicate 0.01
                         --maximum-bit-flips 4 --delay 0.005 --burst 0.01 --burst-length 50) ;;
    *) echo "unknown profile: ${PROFILE}" >&2; print_usage >&2; exit 64 ;;
esac
readonly NOISE_ARGUMENTS

# select the seed arguments of both directions
FIRST_SEED_ARGUMENTS=()
SECOND_SEED_ARGUMENTS=()
if [[ -n "${SEED}" ]]; then
    FIRST_SEED_ARGUMENTS=(--seed "${SEED}")
    SECOND_SEED_ARGUMENTS=(--seed "$((SEED + 1))")
fi
readonly FIRST_SEED_ARGUMENTS
readonly SECOND_SEED_ARGUMENTS

# remove the links of an earlier run
readonly FIRST_PORT="${LINK_DIRECTORY}/ttyV1"
readonly SECOND_PORT="${LINK_DIRECTORY}/ttyV2"
readonly FIRST_NOISE_PORT="${LINK_DIRECTORY}/noiseA"
readonly SECOND_NOISE_PORT="${LINK_DIRECTORY}/noiseB"
rm -f "${FIRST_PORT}" "${SECOND_PORT}" "${FIRST_NOISE_PORT}" "${SECOND_NOISE_PORT}"

# stop every background process on exit
process_ids=()
clean_up() {
    kill "${process_ids[@]}" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap clean_up EXIT INT TERM

# bridge each application port to an internal port
socat PTY,raw,echo=0,link="${FIRST_PORT}" PTY,raw,echo=0,link="${FIRST_NOISE_PORT}" &
process_ids+=($!)
socat PTY,raw,echo=0,link="${SECOND_PORT}" PTY,raw,echo=0,link="${SECOND_NOISE_PORT}" &
process_ids+=($!)

# wait until socat has created the links
for _ in $(seq 1 50); do
    [[ -e "${FIRST_PORT}" && -e "${SECOND_PORT}" && -e "${FIRST_NOISE_PORT}" && -e "${SECOND_NOISE_PORT}" ]] && break
    sleep 0.05
done

# run one noise instance per direction
python3 -u "${SCRIPT_DIRECTORY}/noise.py" "${NOISE_ARGUMENTS[@]}" "${FIRST_SEED_ARGUMENTS[@]}" \
    < "${FIRST_NOISE_PORT}" > "${SECOND_NOISE_PORT}" &
process_ids+=($!)
python3 -u "${SCRIPT_DIRECTORY}/noise.py" "${NOISE_ARGUMENTS[@]}" "${SECOND_SEED_ARGUMENTS[@]}" \
    < "${SECOND_NOISE_PORT}" > "${FIRST_NOISE_PORT}" &
process_ids+=($!)

# notify the user
echo "Noisy link ready (${PROFILE}): ${FIRST_PORT} <-> ${SECOND_PORT} (Ctrl-C to tear down)"
wait
