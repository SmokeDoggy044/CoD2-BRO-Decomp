# Licenses

The repository is private for now and will be made public later. This folder holds
the license texts of everything in it.

## The port's own code

Not chosen yet. Everything outside `third_party/` is original work of this project
and gets its license (file `LICENSE` at the repository root, plus the text here)
before the repository goes public.

Note for the choice: the GPL-2.0 headers below are compiled into the loader, so a
build that includes them must be distributed under GPL-2.0-or-later terms. A
GPL-compatible license for the port (GPL-2.0-or-later itself, or a permissive one
such as MIT) keeps that possible; replacing those headers with our own would remove
the constraint.

## Third-party code

`third_party/xbox-includes/` (github.com/mborgerson/xbox-includes, version in its
`VERSION` file; its own `COPYING` is the GPL-2.0 text):

| Files | License | Text |
|---|---|---|
| `d3d8.h`, `d3d8types.h`, `dsound.h`, `dsoundtypes.h`, `xacteng.h`, `xapi.h`, `xgraphic.h`, `xonline.h` (from Cxbx) | GPL-2.0-or-later | `GPL-2.0-or-later.txt` |
| `xboxkrnl.h` | CC0-1.0 | `CC0-1.0.txt` |
| `d3d8types_wine.h` (from Wine) | LGPL-2.1-or-later | `LGPL-2.1-or-later.txt` |
| `xboxdef.h` | no notice in the file; the package's `COPYING` (GPL-2.0) | `GPL-2.0-or-later.txt` |

## Not in this repository

Call of Duty 2: Big Red One and its files (XBE, assets) belong to their owners and
are never committed: the port loads them from the user's own copy of the game.
`tools/data/default_xbe_symbols.txt` lists function addresses and analysis names
from a Ghidra export, with no game code or data.
