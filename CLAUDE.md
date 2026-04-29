# Asteria — Claude Project Guide

Astrological chart calculator and AI interpreter. Built with Qt6/C++17 on Windows (primary dev machine), also ships as a Flatpak on Flathub (Linux).

---

## Workflow

**Do not work in git worktrees on this project.** Edit directly in the main checkout at `C:\Users\ivan\git\Asteria\`. Reasons:

- The build dir (`build/`) is configured against the main checkout, so edits made in a worktree don't get rebuilt — `cmake --build` reports "no work to do" against unchanged main-checkout sources.
- On Windows, the shell session that hosts Claude Code holds a handle on its CWD. If that CWD is inside a worktree, `git worktree remove` cannot delete the directory until the session exits.

Do not spawn shells (Bash / PowerShell) inside `.claude/worktrees/...`. Always run commands against the main checkout via absolute paths or `git -C C:/Users/ivan/git/Asteria`.

---

## Build System

**Toolchain:** MSVC 2022 (BuildTools), Qt 6.11.0, CMake + Ninja  
**Build dir:** `C:\Users\ivan\git\Asteria\build\`  
**Output:** `build\Asteria.exe`

### How to compile

The MSVC environment must be initialised inside the **same** `cmd.exe` process as CMake. Do NOT use `&&` between a `.bat` and PowerShell — env vars don't survive the boundary. Use:

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cd C:\Users\ivan\git\Asteria && cmake --build build --config Release"'
```

A bare `cmake --build build --config Release` without the VS environment will fail with:
```
fatal error C1083: Cannot open include file: 'type_traits'
```

### External dependencies (local paths baked into CMakeLists.txt)

| Dependency | Path |
|---|---|
| Swiss Ephemeris source | `C:\Users\ivan\git\swisseph\` |
| Ephemeris data files (`.se1`) | `C:\Users\ivan\git\ephe\ephemeris\` |
| Qt 6.11.0 | `C:\Qt\6.11.0\msvc2022_64\` |

### Build flags / defines

- `WIN32_EXECUTABLE TRUE` — GUI subsystem; app detaches from terminal immediately. `qDebug()` goes to `OutputDebugString`, **not** stdout.
- `FLATHUB_BUILD` — CMake option (OFF by default). When ON: uses `libswe.a` static build, omits `PrintSupport`/`Pdf`, sets Flatpak paths.
- `GENTOO_BUILD` — similar conditional for Gentoo packaging.
- `SWISSEPH_DATA_DIR` — compile-time path injected as a string define.

---

## Debug Logging

Because `WIN32_EXECUTABLE` hides the console, all `qDebug()`/`qWarning()` output goes to a log file:

**Log location:** `Documents\Asteria\debug.log`  
(i.e. `%USERPROFILE%\Documents\Asteria\debug.log`)

Set up in `main.cpp` via `qInstallMessageHandler`. Format: `hh:mm:ss.zzz [D/W/C/F] message`

---

## Source File Map

| File | Responsibility |
|---|---|
| `main.cpp` | Entry point. Installs log handler, loads font, sets org/app name, creates `MainWindow`. |
| `mainwindow.h/.cpp` | Central UI: input dock, chart display, interpretation dock, all menus and slots. ~2000+ lines. |
| `chartcalculator.h/.cpp` | All astrological maths via Swiss Ephemeris. Natal, solar/lunar/planetary returns, transits, progressions, eclipses. |
| `chartrenderer.h/.cpp` | `QGraphicsView`-based wheel chart renderer. `PlanetItem`, `AspectItem` custom graphics items. |
| `chartdatamanager.h/.cpp` | Serialise/deserialise chart data to/from JSON for save/load. |
| `mistralapi.h/.cpp` | HTTP client for OpenAI-compatible AI API. Handles both chart and transit interpretation requests. |
| `modelselectordialog.h/.cpp` | Dialog for managing AI model configurations (add/edit/delete/set-active). |
| `model.h` | Plain `struct Model` — name, provider, endpoint, apiKey, modelName, temperature, maxTokens. |
| `Globals.h/.cpp` | `AsteriaGlobals` namespace + `AspectSettings` singleton + `getOrbMax()`/`setOrbMax()` free functions. |
| `aspectarianwidget.h/.cpp` | Table widget showing all aspects. |
| `elementmodalitywidget.h/.cpp` | Element/modality balance display. |
| `planetlistwidget.h/.cpp` | Table of planets with sign/house/degree. |
| `aspectsettingsdialog.h/.cpp` | Dialog for aspect line visual settings. |
| `osmmapdialog.h/.cpp` | OpenStreetMap dialog for picking birth coordinates. Uses QtLocation/QML (`map.qml`). |
| `transitsearchdialog.h/.cpp` | Filter/search dialog for transit table results. |
| `symbolsdialog.h/.cpp` | Reference dialog showing astrological symbols legend. |
| `donationdialog.h/.cpp` | Donation info dialog. |

---

## Global State (`Globals.h` / `Globals.cpp`)

```cpp
namespace AsteriaGlobals {
    bool additionalBodiesEnabled;       // checkbox state for extra bodies
    QString lastGeneratedChartType;     // e.g. "Natal Birth", "Solar Return", "Zodiac Signs"
    QString appDir;                     // Documents/Asteria  (writable data dir)
    bool activeModelLoaded;             // true when a valid AI model is configured
}
```

`lastGeneratedChartType` drives the AI prompt selection in `MistralAPI::createPrompt()`.  
`activeModelLoaded` is the gate that prevents AI requests when no model is configured. It is set by `MistralAPI::loadActiveModel()` (called from constructor and `MainWindow::configureAIModels()`).

---

## AI / MistralAPI System

### How it works

`MistralAPI` is an OpenAI-compatible HTTP client. It works with **any** provider that speaks the `/v1/chat/completions` format: Mistral, OpenAI, Groq, Ollama, OpenRouter, Together AI, DeepSeek, etc.

**Not compatible** (different API formats): Anthropic Claude, Google Gemini, Cohere.

### QSettings storage (Windows Registry)

Key: `HKCU\Software\Alamahant\Asteria\Models`  
(on Linux Flatpak: `~/.config/Alamahant/Asteria.conf`)

```
Models/ActiveModel         = "My Mistral"          ← name of active config
Models/My Mistral/endpoint = "https://..."
Models/My Mistral/apiKey   = "..."
Models/My Mistral/modelName= "mistral-medium-latest"
Models/My Mistral/temperature = 0.7
Models/My Mistral/maxTokens   = 8192
```

### Key API details

- **`"stream": false`** is explicitly set in both `createPrompt()` and `createTransitPrompt()`. This is critical — without it, OpenRouter and some other providers return SSE streaming chunks (`data: {...}\n\n`) which `QJsonDocument::fromJson()` cannot parse, causing the UI to stay stuck at "Requesting interpretation from AI...".
- `m_language` defaults to `"English"` (constructor initialiser) and is guarded in `setLanguage()` to reject empty/whitespace strings.
- Two request types: chart (`interpretChart`) and transit (`interpretTransits`). Transit replies are distinguished by a `isTransitRequest` property set on the `QNetworkReply` object.

### AI prompt variants (by `lastGeneratedChartType`)

| Chart Type | Prompt style |
|---|---|
| `"Zodiac Signs"` | Per-sign magazine horoscope, 12–15 sentences each |
| `"Secondary Progression"` | Inner development/psychological growth narrative |
| `"Davison Relationship"` | Relationship entity analysis |
| Everything else | Generic natal reading (personality/strengths/challenges/life path) |

---

## Chart Types

All types set `AsteriaGlobals::lastGeneratedChartType` before rendering:

- Natal Birth
- Solar Return, Lunar Return
- Saturn/Jupiter/Venus/Mars/Mercury/Uranus/Neptune/Pluto Return
- Secondary Progression
- Synastry, Composite, Davison Relationship
- Zodiac Signs (world chart, no birth data)
- Transits (separate flow — uses `interpretTransits` not `interpretChart`)
- Eclipses

### Secondary Progression save file layout

The `.astr` save file for a bi-wheel contains these top-level keys:

| Key | Contents |
|---|---|
| `chartData` | Progressed chart (planets, angles, houses, aspects). `chartData.aspects` = **progressed × progressed** — the "Prog → Prog" aspectarian tab. |
| `natalChartData` | Natal chart. `natalChartData.aspects` = natal × natal (not displayed directly). |
| `progressionYear` | Integer year used to offset the natal date. |
| *(no key)* | **Progressed × natal interaspects** ("Prog → Natal" tab) are **not saved** — recomputed on load via `calculateInteraspects(progressed, natal)`. In each `AspectData`, `planet1` = progressed planet, `planet2` = natal planet. Sent to AI as `progressedToNatalAspects`. |

`m_currentNatalChartData` is empty for all non-bi-wheel charts. The load function checks for `natalChartData` first; the subsequent `relationshipInfo` else-branch must **not** clear `m_currentNatalChartData`/`m_progressionYear` — those are already set by the time that branch runs.

---

## Key Gotchas

1. **Build must run in a VS Developer shell** — see Build section above. The `type_traits` error means the MSVC environment is missing.

2. **`stream: false` must stay in the request payload** — removing it will break OpenRouter and other proxies that default to SSE streaming.

3. **`activeModelLoaded` flag** — `loadActiveModel()` must be called (and its return value used to set the flag) whenever the model config changes. It is called in the constructor and after `configureAIModels()` closes.

4. **`WIN32_EXECUTABLE` hides all console output** — always use `Documents\Asteria\debug.log` to read `qDebug()` output. The log is append-only across sessions.

5. **`QtLocation` warning at startup** — `qrc:/map.qml:2:1: module "QtLocation" is not installed` is expected on some Qt builds. The map dialog (`osmmapdialog`) may not function but the rest of the app is unaffected.

6. **`C4834` warning** on `qInstallMessageHandler` — harmless nodiscard warning from MSVC; the return value is intentionally discarded.

7. **Swiss Ephemeris data path** — ephemeris `.se1` files are compiled into the binary path via `SWISSEPH_DATA_DIR`. On local Windows builds this resolves to `C:/Program Files (x86)/Asteria/share/Asteria/ephemeris` (install prefix default). The files are also copied into `build/ephemeris/` at CMake configure time.

---

## App Data Locations (Windows)

| Data | Location |
|---|---|
| Settings (registry) | `HKCU\Software\Alamahant\Asteria` |
| Debug log | `%USERPROFILE%\Documents\Asteria\debug.log` |
| App data dir (`appDir`) | `%USERPROFILE%\Documents\Asteria\` |
| Saved charts | User-chosen via file dialog (JSON format) |

---

## Version

Current version: `2.1.5` (set in `main.cpp` via `QCoreApplication::setApplicationVersion`).
