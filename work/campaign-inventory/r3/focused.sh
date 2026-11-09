#!/bin/zsh
cd ~/malik-champions/open-champions/worktrees/play-campaign-inventory/build/native
/opt/homebrew/bin/ctest -R "^(campaign-inventory|ps2-save|frontend-exact|quests|script-commands|level-flow-exact|mission2-actors|pickups)$" --output-on-failure > ../../work/pci-r3/focused.out 2>&1
echo "exit $?" >> ../../work/pci-r3/focused.out
