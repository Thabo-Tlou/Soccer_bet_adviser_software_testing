# Soccer Bet Adviser  (v2.2.0)

> **Is this the new build?** The footer of the customer view and the admin sidebar footer show **v2.2.0**.
> If you see an older version number you are running an old executable: rebuild (delete your old build folder first) and run the new one.

Desktop app (C++20 / Qt 6) that estimates match outcome probabilities (Poisson goals model + ONNX Random Forest),
compares them with bookmaker prices and explains the result. Two front-ends share one backend:
a **customer view** (scores, predictions, table, picks) and an **admin view** (dashboard, matches, analysis,
performance, search, settings).

## How the pieces connect

```
 football-data.org / The Odds API
            |  (src/api: HttpClient, FootballDataClient, OddsApiClient, LiveDataService)
            v
      LiveDataService  --fixturesChanged / demoTick-->  AppController  (src/ui/AppController)
            |                                               |   owns PredictionService (src/core: models, reasoning)
            |  crest URLs                                   |   owns PredictionStore (SQLite history)
            v                                               v
      CrestProvider  <---------------------  Admin pages  +  Customer pages (UserDataHub)
      (src/ui/art)                                  PitchWidget  RobotWidget  (src/ui/art)
```

* Both front-ends read the same `AppController`, so a saved pick, a settings change or a refreshed fixture shows up on both sides immediately.
* `src/core` has no Qt dependency; `src/api` is Qt Core + Network only; `src/ui` is Qt Widgets.
* No API key? Both sides fall back to the bundled recorded results (`AppController::recordedResults`).

## Betting markets and staking
Every common market is priced for each match (1X2, double chance, draw no bet, over/under, both teams to score, team goals, Asian handicap,
exact goals, correct score, clean sheets, odd/even) from one scoreline grid, with value verdicts, stake advice and a bet slip.
See **`docs/BETTING_MARKETS.md`** (what is covered, how prices and verdicts work, the held-out validation, and the limits).
Code: `src/core/Markets.*` (engine, tested in `tests/test_core.cpp`), `src/ui/MarketsPanel.*`, `src/ui/user/UserSlipPage.*`.

## Design system
Colours, spacing, type and elevation live in `src/ui/design/Tokens.h` (palette: Violet Water, Transformer, Thyme Green, Green Leaf,
Turtle Skin, Nightmare). See **`docs/DESIGN_SYSTEM.md`** for tokens, components, layout and accessibility checks (`ctest -R design`).
The customer view opens on a **Home** page (what the app does, live stats, featured match, league snapshot, how it works).

## Visual layer (`src/ui/art`)

| File | What it does |
|---|---|
| `Crest.*` | Club crests: your logo files, else crest URLs from football-data.org (cached), else a generated club-colour crest |
| `PitchWidget.*` | Stadium graphic: live score + minute tracker, full-time shot volume, or the model's probabilities as pitch territories |
| `RobotWidget.*` | Software-rendered 3D robot analyst (QPainter only, no OpenGL): idles, scans while models run, nods with the verdict |
| `Icons.*` | One consistent line-icon set drawn in code (any colour, any size) |
| `AnimClock.h` | One shared ~30 fps clock; stops when "Reduce motion" is on |

## Fresh data and time zone

* **Latest results:** at start-up the app downloads the current season's results from football-data.co.uk (free, no key),
  caches them in `<AppData>/results/`, and merges any matches newer than the bundled history *before* the models load.
  If you are offline it simply uses the last saved copy. Admin > Settings > **Match data** has *Update results now* (restarts the app).
* **League table:** built from the history above, plus finished matches from the live API when a key is configured.
* **Time zone:** all times are shown in the zone chosen in Settings (default **Africa/Maseru, UTC+2**). The bundled history stores
  kickoff times in UK local time; they are converted to real UTC first, so BST/GMT is handled correctly.
* Developer switches: `SBA_NO_UPDATE=1` skips the download; `SBA_RESULTS_URL=<base url>` points it at another server (testing).

## Live preview without an API key

Customer view: **Scores > Preview live demo** (or Admin > Settings > *Show a simulated live match*).
This runs a looping, clearly labelled **DEMO** match. It is never stored, never predicted and never mixed into history.
For screenshots/CI you can set the environment variable `SBA_DEMO=1` (not saved).

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```
Requires Qt 6 (Widgets, Network, Concurrent; **Svg** recommended - without it SVG logos are skipped), CMake >= 3.21. ONNX Runtime and nlohmann-json are fetched by CMake.

Developer aids: `SoccerBetAdviser --screenshots <dir>` (admin) and `--user-screenshots <dir>` (customer) render the main screens to PNG.

## Club logos
See `logos/README.txt`.

Estimates only, not financial advice. 18+. Please gamble responsibly.
