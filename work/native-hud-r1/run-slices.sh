#!/bin/bash
# Four parallel counts-only slices of the real-data scene sweep.
# Usage: work/native-hud-r1/run-slices.sh TAG [extra driver args...]
: "${RTA_DISC:?Set RTA_DISC to the read-only game folder}"
cd "$(dirname "$0")/../.."
TAG=$1; shift
G=$PWD/../integration/work/runtime/Godot.app/Contents/MacOS/Godot
D=$RTA_DISC
PI=$PWD/../playable-integration/work/private
P=$PWD/work/private
mkdir -p "$P"
run() {
  mkdir -p "$P/cache-$TAG-s$1"
  timeout 7000 "$G" --headless --path godot --script res://tests/real_data_scene.gd -- \
    --game-folder="$D" --cache="$P/cache-$TAG-s$1" --all --levels=0 \
    --from=$1 --to=$2 --ticks=60 --fx-recipes="$PI/skill-recipes.json" \
    --level-lights="$PI/level-lights.json" --summary="$P/$TAG-s$1.json" "${@:3}" \
    > "$P/$TAG-s$1.log" 2>&1
  echo "slice $1-$2 exit $?"
}
run 0 28 "$@" & run 28 56 "$@" & run 56 84 "$@" & run 84 111 "$@" &
wait
