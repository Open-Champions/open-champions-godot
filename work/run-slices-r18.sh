#!/bin/bash
# Four parallel counts-only slices of the real-data scene sweep (wave 18).
# Usage: run-slices-r18.sh TAG [extra driver args...]
#   r18  --scripts   level script opted in after load
#   r18n             control, no level script
cd "$(dirname "$0")/.."
TAG=$1; shift
G=$PWD/../integration/work/runtime/Godot.app/Contents/MacOS/Godot
D=$PWD/../../private-validation/rta-disc
P=$PWD/work/private
run() {
  mkdir -p $P/cache-s$1
  timeout 7000 "$G" --headless --path godot --script res://tests/real_data_scene.gd -- \
    --game-folder="$D" --cache="$P/cache-s$1" --all --levels=0 \
    --from=$1 --to=$2 --ticks=60 --fx-recipes="$P/skill-recipes.json" \
    --level-lights="$P/level-lights.json" --summary="$P/$TAG-s$1.json" "${@:3}" \
    > $P/$TAG-s$1.log 2>&1
  echo "slice $1-$2 exit $?"
}
run 0 28 "$@" & run 28 56 "$@" & run 56 84 "$@" & run 84 111 "$@" &
wait
