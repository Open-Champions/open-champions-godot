#!/bin/zsh
cd ~/malik-champions/open-champions/worktrees/play-campaign-inventory
G=~/malik-champions/runtime/Godot.app/Contents/MacOS/Godot
D=../../private-validation/rta-disc
L=work/pci-r3
echo "disc files before: $(find $D -type f | wc -l | tr -d " ")" > $L/journey.out
$G --headless --path godot --import > $L/import.log 2>&1; echo "import exit $?" >> $L/journey.out
python3 tools/run_loop_playthrough.py --godot $G --game-folder $D --out $L/playthrough >> $L/journey.out 2>&1; echo "exit $?" >> $L/journey.out
echo "disc files after: $(find $D -type f | wc -l | tr -d " ")" >> $L/journey.out
echo journey-done >> $L/journey.out
