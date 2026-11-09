#!/bin/zsh
cd ~/malik-champions/open-champions/worktrees/play-campaign-inventory/build/native
/opt/homebrew/bin/ctest -j 3 --output-on-failure > ../../work/pci-r3/ctest.out 2>&1
echo "exit $?" >> ../../work/pci-r3/ctest.out
