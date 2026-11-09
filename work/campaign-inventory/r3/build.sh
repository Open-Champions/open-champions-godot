#!/bin/zsh
# Clean build in one build/ root, sequential, -j 4: native Release, then the three GDExtensions (arm64).
set -u
cd ~/malik-champions/open-champions/worktrees/play-campaign-inventory
CPP=~/malik-champions/runtime/godot-cpp
CM=/opt/homebrew/bin/cmake
L=work/pci-r3
: > $L/build.status
df -h ~/malik-champions | tail -1 > $L/df-before.txt
date "+start %Y-%m-%dT%H:%M:%S" >> $L/build.status
$CM -S native -B build/native -DCMAKE_BUILD_TYPE=Release > $L/native-configure.log 2>&1 && $CM --build build/native -j 4 > $L/native-build.log 2>&1
echo "native exit $?" >> $L/build.status
for spec in "gdextension mac-template_debug" "gdextension/level_texture level-texture-template_debug" "gdextension/creature_anim creature-template_debug"; do
  set -- ${=spec}
  $CM -S $1 -B build/$2 -DGODOT_CPP_DIR=$CPP -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64 > $L/$2-configure.log 2>&1 && $CM --build build/$2 --config Release -j 4 > $L/$2-build.log 2>&1
  echo "$2 exit $?" >> $L/build.status
done
date "+end %Y-%m-%dT%H:%M:%S" >> $L/build.status
echo done >> $L/build.status
