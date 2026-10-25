# UI text gaps (docs/UI_TEXT_GAPS.md): help box, bust, compass, sound slots, fades,
# glyph byte. Replays tests/vectors/ui_text_gaps.tsv.
include_guard(GLOBAL)
set(CHAMPIONS_UI_TEXT_GAPS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
add_library(champions-ui-text-gaps "${CHAMPIONS_UI_TEXT_GAPS_ROOT}/src/ui_text_gaps.cpp")
target_include_directories(champions-ui-text-gaps PUBLIC "${CHAMPIONS_UI_TEXT_GAPS_ROOT}/include")
target_link_libraries(champions-ui-text-gaps PUBLIC champions-core)
set_target_properties(champions-ui-text-gaps PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(champions-ui-text-gaps PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-ui-text-gaps PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
