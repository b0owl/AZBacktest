<img width="2048" height="1332" alt="image" src="https://github.com/user-attachments/assets/5096c757-2daf-478e-bc54-aa1de24dad1e" />

# AZBacktest

**Before building, edit `csvConfig.h` to map your CSV columns and set the data path. The file ships with placeholder values that will not compile until filled in.**

Release convention:  
-> Every small feature *and* bugfix will result in an increment to the last number in the version name (i.e 1.0.0 to 1.0.1).  
-> Every big feature will result in an increment to the middle number in the version name.  
-> Once enough features pile on, the first number in the version name will be incremented while the last two are reset to zero.  

Commit convention/notes:  
-> Commits are rarely fully tested before release, and frequently have multiple follow up commits fixing issues / speed.  
-> These unstable commits were previously committed to the main branch. As of 8/6/26 that has changed, an unstable branch has been created.  
-> All commits that are not submitted by b0owl are to be added to the unstable branch, and eventually merged with main.  
-> It would be unwise to try and build the header file from the unstable branch, unless it includes an in-progress feature your project requires.  
-> If you are looking for an earlier version of  the project (created before the aforementioned date) scan the commit messages for the most stable version.  

TODOS:  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[ ] Overlapping trade support  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[ ] Read-From-CSV option for UI (less restarting the visualizer)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[ ] More built-in price derived data  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[ ] Codebase refactoring  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[x] Skin support (allow easy exposure to the internal imgui theme)  

C++17 backtesting framework. Header-only API with an ImGui/ImPlot visualization window.

## Build

```bash
# run a strategy
bash build.sh -runfile exampleStrategies/BuyAndHold.cpp

# generate single-header amalgamation (azbacktest.h)
bash build.sh

# clean leftover build artifacts
bash build.sh -clean
```

Requires g++ with C++17 (C++20 if Parquet support is linked in, see below), GLFW and OpenGL (vendored under `vendor/`).

## Configuration

Edit `csvConfig.h` to point at your CSV (or Parquet, see below) and map column indices.
Note: If you use the header file (from releases) you'd have to search for the mapping itself, and change it directly.

### Parquet support

`MarketData` also reads Parquet files directly - point `kCSVMapping.path` at a `.parquet`
file instead of a `.csv` one, keeping the same column indices (`timestampCol`, `priceCol`,
etc). The Parquet file needs the same column order/names as your CSV, with each column
given a proper type (timestamps, doubles, ints) instead of raw text - usually several times
smaller and faster to read than the equivalent CSV. `pyarrow` (`pip install pyarrow`) can
produce one straight from a CSV, letting it infer types per column and preserving order.

This needs [Apache Arrow](https://arrow.apache.org/):

- **macOS**: `brew install apache-arrow`
- **Windows** (MinGW/MSYS): install via [vcpkg](https://vcpkg.io) with a MinGW triplet
  (vcpkg's default MSVC-built libs aren't ABI-compatible with g++):
  `vcpkg install arrow[parquet]:x64-mingw-dynamic`. `build.sh` looks for it via `VCPKG_ROOT`,
  or at `C:\vcpkg` / `C:\tools\vcpkg` if that's unset.

`build.sh` auto-detects Arrow and links it in when present; without it, builds still work
but are CSV-only (pointing `kCSVMapping.path` at a `.parquet` file without Arrow compiled in
throws a clear error at startup telling you to install it). Since Arrow's headers require
C++20, `build.sh` bumps the standard to C++20 automatically whenever it links Arrow in -
only for builds that use it, CSV-only builds stay on C++17.

```cpp
inline constexpr CSVMapping kCSVMapping{
    PLACEHOLDER_VALUE,              // timestampCol (int)
    PLACEHOLDER_VALUE,              // priceCol (int)
    PLACEHOLDER_VALUE,              // sizeCol (int)
    PLACEHOLDER_VALUE,              // path (const char*)
    { PLACEHOLDER_VALUE },          // dateFormat (DateFormat: yearOff, yearLen, monthOff, monthLen, dayOff, dayLen)
    PLACEHOLDER_VALUE,              // skipHeader (bool)
    PLACEHOLDER_VALUE,              // symbolCol (int, -1 to disable)
    PLACEHOLDER_VALUE,              // symbol (const char*)
    PLACEHOLDER_VALUE,              // symbolRoll (bool)
    PLACEHOLDER_VALUE,              // aggressor (int)
    PLACEHOLDER_VALUE,              // buySideAggressorAlias (const char*)
    PLACEHOLDER_VALUE,              // sellSideAggressorAlias (const char*)
    PLACEHOLDER_VALUE,              // unknownSideAggressorAlias (const char*)
};
```

## API Reference

### Market Data

#### `MarketData(const std::string& path)`
Memory-maps a CSV file for zero-copy tick reading.

#### `std::optional<Tick> MarketData::nextTick()`
Returns the next raw tick (timestamp + price as string_views, size, and aggressor side), or `nullopt` at EOF. The `side` field is a `const char*` set to the matching aggressor alias from `csvConfig.h`.

#### `std::optional<Tick> MarketData::nextClose(int seconds)`
Returns the close tick of the next bar spanning at least `seconds`. Skips forward until the timestamp exceeds start + seconds.

- `seconds` — bar width in seconds (e.g. 60 for 1-min bars)

---

### Trade Management

#### `Handling(std::vector<float>& prices, float tickSize, float tickValue)`
Manages trade lifecycle. Binds a live price vector (by reference) and per-instrument tick metadata.

- `prices` — live price window, Handling reads `.back()` for entries
- `tickSize` — instrument tick size
- `tickValue` — instrument tick value

#### `int Handling::openLong(int idx)`
Opens a long at the current price. Returns 1 if opened, 0 if already in a long.

- `idx` — bar index for bookkeeping

#### `int Handling::openShort(int idx)`
Opens a short at the current price. Same rules as `openLong`.

- `idx` — bar index for bookkeeping

#### `void Handling::closeTrade()`
Closes the current open trade, locks P&L, resets position state.

#### `void Handling::closeAll()`
Closes any open trade. Call at end-of-data.

#### `bool Handling::tick(std::string_view timestamp = {})`
Advances the open trade's P&L to current price and records an equity curve sample.

- `timestamp` — if non-empty, gets parsed for equity curve timestamps

#### `DataWindow Handling::requestDataWindow(MarketData& md, int period, void (*whenUnknown)(), bool supressWarnings = false, int timeframe = 0, int tickRes = 1)`
Pulls `period` bars from the market data source. Returns a `DataWindow` with parallel `prices`, `volumes`, `execBids`, `execAsks`, and `deltas` vectors.

- `md` — MarketData source
- `period` — how many rows/bars to load
- `whenUnknown` — callback invoked when the aggressor side can't be classified
- `supressWarnings` — set true to silence tickRes warnings
- `timeframe` — 0 = tick-by-tick, >0 = close every N seconds
- `tickRes` — tick mode only: keep every Nth tick (1 = full resolution)

`execBids` and `execAsks` classify each tick's volume by aggressor side using the aliases configured in `csvConfig.h`. If the CSV side column doesn't match either alias, `whenUnknown` is called instead. `deltas[i]` is the price change from the previous bar.

---

### Indicators

#### `std::vector<float> returnSimpleMovingAverage(const std::vector<float>& prices, int period)`
Simple moving average over the tail of `prices`. Returns `period` output points.

- `prices` — full price vector
- `period` — lookback length

#### `std::vector<float> returnExponentialMovingAverage(const std::vector<float>& prices, int period)`
Exponential moving average, same tail-slice convention as SMA.

- `prices` — full price vector
- `period` — lookback / smoothing length

#### `std::vector<std::vector<float>> returnVolumeProfile(int anchor, std::vector<float>& prices, std::vector<float> volumeData)`
Builds a volume profile from `anchor` forward.

- `anchor` — starting index (everything before is ignored)
- `prices` — price vector
- `volumeData` — per-bar volume, same length as prices
- Returns `[[price, volume], ...]`

#### `std::vector<float> returnValueArea(std::vector<std::vector<float>> volumeProfile, float pct = 0.70f)`
Returns `{VAL, VAH}`, the price range containing `pct` of total volume.

- `volumeProfile` — output of `returnVolumeProfile`
- `pct` — fraction of volume to capture (default 0.70)

#### `std::vector<float> returnVWAP(int anchor, const std::vector<float>& prices, const std::vector<float>& volumeData)`
Anchored volume-weighted average price, running/cumulative from `anchor` forward. One output value per bar (parallel to `prices[anchor..]`), so `.back()` is the current VWAP.

- `anchor` — starting index (everything before is ignored); reset it to the first bar of a session for a daily VWAP
- `prices` — price vector
- `volumeData` — per-bar volume, same length as prices

---

### Statistics

#### `float returnWinrate()`
Fraction of winning trades (0.0-1.0).

#### `float returnCumProfit()`
Total profit across all closed trades in points.

#### `std::vector<float> returnProfitOverTime(int bucketMins = 1)`
Equity curve bucketed into fixed-width time bars.

- `bucketMins` — width of each bar in minutes

#### `std::vector<float> returnAverageProfitOverTime()`
Running average profit per trade, one value per closed trade.

#### `float returnAvgPnl()`
Average P&L across all trades in points.

#### `float returnAverageWinSize()`
Average profit of winning trades in points.

#### `float returnAverageLossSize()`
Average loss of losing trades in points (negative).

#### `float returnTradesPerDay()`
Trades per calendar day based on first/last trade close timestamps.

#### `float returnExpectancy(Predicate pred)`
Average P&L of trades matching `pred`.

- `pred` — callable taking `const tradeData&`, returns true to include

---

### Monte Carlo

#### `std::vector<std::vector<float>> returnMonteCarlo(int sims, int avgBlockLen = 5, unsigned seed = 42)`
Stationary bootstrap MC. Resamples blocks of consecutive trades with geometrically distributed lengths.

- `sims` — number of simulations
- `avgBlockLen` — expected block length (controls serial dependence)
- `seed` — RNG seed for reproducibility
- Returns `paths[sim][step]`

#### `std::vector<std::vector<float>> returnPercentilePaths(const std::vector<std::vector<float>>& mcPaths, std::vector<int> percentiles)`
Extracts percentile lines from MC paths.

- `mcPaths` — output of `returnMonteCarlo`
- `percentiles` — list of percentiles 0-100 (e.g. `{5, 50, 95}`)
- Returns one column per percentile

---

### Series Pool

#### `void addSeries(std::string name, std::vector<T> values, int type = 0, RGBA color = {})`
Adds a 1D series to the visualization pool. Any arithmetic type gets converted to float.

- `name` — display name
- `values` — data points
- `type` — 0 = line, 1 = bar
- `color` — optional RGBA color (`{r, g, b, a}`, all -1 = auto)

#### `void addSeries(std::string name, std::vector<std::vector<T>> values, std::vector<std::string> colNames = {}, int type = 0, RGBA color = {})`
Adds a 2D series (multiple columns) to the pool.

- `name` — display name
- `values` — vector of columns, each column is a vector
- `colNames` — optional per-column names
- `type` — 0 = line, 1 = bar
- `color` — optional RGBA color

#### `void initSeriesPool(std::vector<std::vector<float>> data, std::vector<std::string> names, std::vector<int> types)`
Batch init, clears the pool and adds each inner vector as a 1-column series.

---

### Stat Pool

#### `void addStat(std::string name, float value)`
Logs a named stat. Shows up in the Statistic Explorer widget. Overwrites if the name already exists.

---

### Window

#### `int showConsole(const char* title)`
Opens the ImGui window and enters the render loop. Blocks until the window is closed. Add series/stats before calling this.

Menu bar provides **New > Panel** (chart window) and **New > Widget** (series explorer, statistic explorer). Panels and widgets can be closed via their title bar X button.
