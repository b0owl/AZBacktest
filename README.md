# AZBacktest

Backtesting engine (Go) + desktop GUI (Electron + Vite).

## AZBacktestAPI

Go library for tick-by-tick backtesting. Feed it CSV rows, get P&L back.

```go
import "AZBacktestAPI/pkg/backtest"

// Call on every tick row: [timestamp, bid, bidSize, ask, askSize, ...]
results := backtest.Tick(row)

// Open a position
backtest.Enter("trade-id", "long")  // or "short"

// Close a position
backtest.Exit("trade-id")
```

Results map returns cumulative P&L in ticks per closed trade ID.

## AZBacktestGUI

Electron desktop app. Run with:

```bash
cd AZBacktestGUI
npm install
npm run dev
```

**Keyboard controls**

| Key | Action |
|-----|--------|
| `Space` | Open console |
| `Esc` | Close console |
| `Backspace` | Delete character |
| `Ctrl+Backspace` | Delete word |
| `Enter` | Submit command |

**Console commands**

| Command | Description |
|---------|-------------|
| `GO EQUITY` | Switch to equity curve view |
| `GO STATPAGE` | Switch to stats view (rolling win rate + trade log) |
| `SET RESULTS <path>` | Point the GUI at a different `trades.csv` |

The GUI reads a `trades.csv` produced by your backtest script. Path is set once via `SET RESULTS` and persists across restarts.
