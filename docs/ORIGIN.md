# Source origin and licensing

The initial native archive library, inspector and synthetic tests were extracted
from `native/` in the
[Return to Arms research repository](https://github.com/Open-Champions/champions-return-to-arms-re)
using Git subtree history. The extracted history retains original authors and
commit messages; this repository then moves that root into `native/`.

The inherited subtree tip was `2864aa28d6596dc72bb43ca2f7c5c5585d6f890c`.
Its first implementation commit was `15604e5b1480ea335bf169a3c2e1670e45b16731`.
Subtree commit IDs differ from the original repository's full-tree commits.
The inherited inspector guide links the original platform-validation run at
source revision `cf00c28`; this does not substitute for this repository's own CI.

Copyright and the MIT license from the original authored-code repository are
preserved in [LICENSE](../LICENSE). No proprietary game source, assets or compiled
game code were transferred. Open-BFME is an architectural precedent, not a source
code dependency. Godot and godot-cpp are proposed future dependencies and have
not been vendored into this initial foundation.
