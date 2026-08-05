# AZBacktest

TODOS:
    Overlapping trade support
    Read-From-CSV option for UI (less restarting the visualizer)
    More built-in price derived data
    Codebase refactoring

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

Requires g++ with C++17, GLFW and OpenGL (vendored under `vendor/`).

## Configuration

Edit `csvConfig.h` to point at your CSV and map column indices.
Note: If you use the header file (from releases) you'd have to search for the mapping itself, and change it directly.

```cpp
inline constexpr CSVMapping kCSVMapping{
    0,                              // timestampCol
    8,                              // priceCol
    9,                              // sizeCol (volume)
    "Data/your-file.csv",           // path (relative to project root)
    { 0, 4, 5, 2, 8, 2 },          // dateFormat: year off/len, month off/len, day off/len
    true,                           // skipHeader
};
```

## API Reference

### Market Data

#### `MarketData(const std::string& path)`
Memory-maps a CSV file for zero-copy tick reading.

#### `std::optional<Tick> MarketData::nextTick()`
Returns the next raw tick (timestamp + price as string_views), or `nullopt` at EOF.

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

#### `std::vector<float> Handling::requestDataWindow(MarketData& md, int period, int timeframe = 0)`
Pulls `period` prices from the market data source.

- `md` — MarketData source
- `period` — how many rows/bars to load
- `timeframe` — 0 = tick-by-tick, >0 = close every N seconds

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
