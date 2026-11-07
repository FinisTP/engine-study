# Package check (`Package.hpp`, `UmbraTool package-check`, roadmap P9)

`scripts\package.bat` builds the game package and then runs
`UmbraTool package-check out\package\Umbra --source .`; any error stops it.

| Code | Severity | Rule |
|---|---|---|
| `package.program` / `package.content` | error | `Umbra.exe`, the reference content (RPG file, three scenes), the compiled shaders and `THIRD_PARTY_NOTICES.md` are there |
| `package.policy` | error | every learned enemy policy the RPG content names is shipped |
| `package.forbidden` | error | no editor, UmbraTool, policy training library, scene checker, test programs, scripts (`.py`, `.bat`, `.ps1`), `CMakeCache.txt`, editor settings or exported ComfyUI workflows |
| `package.license` | error | every DLL next to the program is named in `THIRD_PARTY_NOTICES.md` (in backticks; `*` allowed, e.g. `vcruntime140*.dll`) |
| `package.secret` | error | no text file holds something shaped like a key, token or password (Meshy `msy_…`, `sk-…`, `"api_key": "…"`); the value is never printed |
| `package.path` | error | no content JSON/glTF holds an absolute path (`C:\…`, `/home/…`): it would break on another PC. Provenance (`.source.json`) may |
| `package.buildpath` | error | `Umbra.exe` does not contain `<source>/assets`: the game finds its assets at run time (next to the program, or by looking upwards in a development build) |
| `package.sourcenames` | warning | how many source file names the program contains (assertion and crash locations); they reveal the project folder but change nothing |
| `package.ai-license` | warning | a generated model whose `.source.json` lists a model that isn't known to allow selling |
| `package.extra` | warning | AI working files (`*.concept.png`, `comfy-export*` folders) that the game never loads |
| `package.large` / `package.size` | warning | files over 64 MiB, and the package size |

Library license texts are copied into `licenses/` by the install rules (`apps/umbra/CMakeLists.txt`,
from vcpkg's `share/<port>/copyright`).
