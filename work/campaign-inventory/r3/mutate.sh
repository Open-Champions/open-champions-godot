#!/bin/zsh
# Each mutation alone, rebuilt, its test run, then restored and touched.
cd ~/malik-champions/open-champions/worktrees/play-campaign-inventory
CM=/opt/homebrew/bin/cmake
L=work/pci-r3
run() { # name file perl-substitution target test
  local name=$1 file=$2 expr=$3 target=$4 test=$5
  cp $file $L/mut.bak
  perl -0pi -e "$expr" $file
  if cmp -s $file $L/mut.bak; then echo "$name: MUTATION DID NOT APPLY"; cp $L/mut.bak $file; touch $file; return; fi
  $CM --build build/native --target $target -j 4 > $L/mut-$name.log 2>&1 || { echo "$name: build failed"; }
  if build/native/champions-$test-tests > $L/mut-$name.out 2>&1; then echo "$name: SURVIVED (test passed)"; else echo "$name: killed: $(grep -m1 -E "FAIL|line [0-9]+" $L/mut-$name.out | cut -c1-140)"; fi
  cp $L/mut.bak $file; touch $file
}
# PCR2-4: the share back at its round-1 place (after the records, before vars_set).
run share-old-order native/src/ps2_save.cpp "s/(  \}\n)(  \/\/ 5\. script_clearDeferred)/\$1  if (share && memory.s32(NUM_PLAYERS) > 0)\n    campaign_inventory::share_quest_items(variables, 0);\n\$2/; s/(  memory.string\(LOADED_NAME, pending\);\n(?:  \/\/[^\n]*\n)*)  if \(share && memory.s32\(NUM_PLAYERS\) > 0\)\n    campaign_inventory::share_quest_items\(variables, 0\);\n/\$1/" champions-ps2-save-tests ps2-save
# PCR2-1: a client save refusal treated as a card error again (latch kept).
run refusal-as-card-error native/src/frontend.cpp "s/error_ = reason.empty\(\) \? \"save refused\" : reason;\n  save_latch_ = LATCH_UNKNOWN;/error_ = reason.empty() ? \"save refused\" : reason;/" champions-campaign-inventory-tests campaign-inventory
run refusal-as-card-error-fe native/src/frontend.cpp "s/error_ = reason.empty\(\) \? \"save refused\" : reason;\n  save_latch_ = LATCH_UNKNOWN;/error_ = reason.empty() ? \"save refused\" : reason;/" champions-frontend-tests frontend
run load-open-clear native/src/frontend.cpp "s/        save_latch_ = 0; \/\/ memcard_requestLoad clears the latch when it opens \(0x217760\)\n        go\(Screen::LoadGame\);\n      \} else if \(id == \"options\"\)/        go(Screen::LoadGame);\n      } else if (id == \"options\")/" champions-campaign-inventory-tests campaign-inventory
run load-slot-set native/src/frontend.cpp "s/if \(confirm && id == \"session\"\) \{\n      save_latch_ = 1;\n      request\(\"load_session\"\);/if (confirm && id == \"session\") {\n      request(\"load_session\");/" champions-frontend-tests frontend
run load-done-clear native/src/frontend.cpp "s/  save_latch_ = 0;\n  paused_ = false;\n  request\(\"pause\", \{\}, 0\);\n  go\(Screen::Playing\);\n\}/  paused_ = false;\n  request(\"pause\", {}, 0);\n  go(Screen::Playing);\n}/" champions-frontend-tests frontend
run load-refusal-unknown native/src/frontend.cpp "s/    error_ = error.empty\(\) \? \"load refused\" : error;\n    save_latch_ = LATCH_UNKNOWN;/    error_ = error.empty() ? \"load refused\" : error;/" champions-frontend-tests frontend
run unknown-latch-refusal native/include/champions/campaign_entry.hpp "s/if \(carry.card_latch != 0 && carry.card_latch != 1\)/if (false)/" champions-campaign-inventory-tests campaign-inventory
echo mutate-done
