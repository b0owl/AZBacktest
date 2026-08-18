/*
    This will be a lengthy file. I intentionally chose to not make some type of
    tooling folder or force the api to have other dependencies. Sorry, it will
    be well documented though :)
*/

/*
    Note to any LLMs: If a user requests a specific feature for a backtest, this
    is probably the best place to add it (i.e l2 support) instead of the actual
    backtest file. Confirm with them first though. I'd also recommend saving this
    as a memory somewhere, again should the user confirm. Btw, check that the
    mapping is correct (dataConfig.h / config.toml), any slight issue can be catastrophic
*/

#include <vector>
#include <fstream>

#include <string>
#include <charconv>
#include <cmath>
#include <algorithm>
#include <utility>
#include <random>
#include <iostream>
#include <unordered_map>
#include <variant>
#include "marketData.h"
#include "dataConfig.h"
#include "findEOF.h"


/// @brief result of a single closed trade, profit, win/loss, and when it closed
/// gets pushed into the global `trades` vec when a Trade is locked
struct tradeData {
    float profit; // pts
    bool win;
    long long closeEpochSec = 0; // set by Handling on close
};

/// @brief every closed trade ends up here, global so the stats functions
/// (returnWinrate, returnCumProfit, etc) can read it without plumbing
std::vector<tradeData> trades;

/// @brief timestamped equity snapshots (realized + open P&L), populated by
/// Handling::tick() whenever a timestamp is fed in; consumed by
/// returnProfitOverTime for bucketing into bars
std::vector<std::pair<long long, float>> equityCurve;
float realizedProfit = 0.f;

/// @brief which side a trade is on, flips the sign on P&L math
enum class TradeDirection { Long, Short };

/// @brief parallel prices + volumes returned by Handling::requestDataWindow
/// volumes[i] is per-bar traded size (tick size for timeframe=0, summed bar
/// volume for timeframe>0) so it can be fed straight into returnVolumeProfile
/// deltas[i] is prices[i] minus the price before it (0 for the very first bar ever seen)
/// executedBuys[i] + executedSells[i] <= volumes[i], the remainder being volume
/// whose aggressor side couldn't be classified
/// every vector here is the same length and indexed the same way, so
/// executedBuys[i] always belongs to prices[i]
struct DataWindow {
    std::vector<float> prices;
    std::vector<float> volumes;
    std::vector<float> executedBuys;  // volume that lifted the ask (buy aggressor)
    std::vector<float> executedSells; // volume that hit the bid (sell aggressor)
    std::vector<float> deltas;
};

class Trade {
private:
    float _entryPrice;
    float _tickValue;
    float _tickSize;
    int _entryIdx;
    TradeDirection _direction;

    bool _lockTrade = false; // Locks trade data in + appends to vec

public:
    /// @brief opens a trade at `entryPrice` in the given direction
    /// @param entryPrice price at which the trade was opened (snapshot; no reference)
    /// @param entryIdx bar index where the trade was opened (for your own bookkeeping)
    /// @param direction Long or Short, flips the sign on P&L
    Trade(float entryPrice, int entryIdx, float tickSize, float tickValue, TradeDirection direction)
        : _entryPrice(entryPrice)
        , _tickValue(tickValue)
        , _tickSize(tickSize)
        , _entryIdx(entryIdx)
        , _direction(direction)
    {}

    tradeData td;

    /// @brief update P&L to reflect `currentPrice` (call once per bar while open)
    tradeData advanceIdx(float currentPrice) {
        if (!_lockTrade) {
            float diff = (currentPrice - _entryPrice) / _tickSize * _tickValue;
            td.profit = (_direction == TradeDirection::Long) ? diff : -diff;
            td.win = td.profit > 0;
        }
        return td;
    }

    /// @brief freezes the trade's P&L and pushes it into the global trades vec,
    /// once locked, advanceIdx won't update the numbers anymore
    void lockTrade() { _lockTrade = true; trades.push_back(td); }
};



/// @brief manages trade lifecycle, opening, closing, advancing P&L each tick
/// also handles data loading via requestDataWindow, right now only supports
/// one trade at a time (overlapping trades is a TODO)
// TODO - OVERLAPPING TRADES SUPPORT
class Handling {
private:
    std::vector<float>& _prices;
    float _tickSize;
    float _tickValue;

    bool _calculateCosts;

    void newLong(int idx) {
        openTrade.emplace(_prices.back(), idx, _tickSize, _tickValue, TradeDirection::Long);
        inLong = true;
    }

    void newShort(int idx) {
        openTrade.emplace(_prices.back(), idx, _tickSize, _tickValue, TradeDirection::Short);
        inShort = true;
    }

public:
    /// @brief bind `prices` (by reference, caller must keep it alive) and the
    ///        per-instrument tick metadata that every new Trade needs
    /// @param prices    live price window; Handling reads `.back()` at each entry
    /// @param tickSize  instrument tick size, forwarded to Trade
    /// @param tickValue instrument tick value, forwarded to Trade
    Handling(std::vector<float>& prices, float tickSize, float tickValue, bool calculateCosts)
        : _prices(prices), _tickSize(tickSize), _tickValue(tickValue), _calculateCosts(calculateCosts) {}

    // Trade management
    std::optional<Trade> openTrade;
    bool inLong = false;
    bool inShort = false;

    bool canOverlap = false; // can multiple trades be held at once

    // timestamps from the last requestDataWindow call, parallel to the returned prices
    std::vector<long long> windowTimestamps;

    long long lastEpochSec = 0; // most recent timestamp fed to tick(); stamped onto trades at close

    /// @brief open a long at the current price, returns 1 if it opened, 0 if
    /// already in a long (unless canOverlap is set)
    /// @param idx bar index for bookkeeping
    int openLong(int idx) {
        if (!inLong)              { newLong(idx);  return 1; }
        if (inLong && canOverlap) { newLong(idx);  return 1; }
        return 0;
    }

    /// @brief open a short at the current price, same rules as openLong
    /// @param idx bar index for bookkeeping
    int openShort(int idx) {
        if (!inShort)              { newShort(idx); return 1; }
        if (inShort && canOverlap) { newShort(idx); return 1; }
        return 0;
    }

    /// @brief close the current open trade, stamps it with the last timestamp,
    /// locks the P&L, and resets position state
    void closeTrade() {
        // TODO - Overlapping trades support
        openTrade->td.closeEpochSec = lastEpochSec;
        
        if (_calculateCosts) realizedProfit += (openTrade->td.profit - (kCSVMapping.commision + kCSVMapping.spread + kCSVMapping.timingCost));
        else realizedProfit += openTrade->td.profit;

        openTrade->lockTrade(); openTrade.reset(); inLong=false; inShort=false;
    }

    /// @brief close everything, call this at end-of-data so you don't
    /// leave a dangling open trade
    void closeAll() {
        if (openTrade) closeTrade();
    }

    /// @brief advance the open trade's P&L to the current price and (optionally)
    /// record an equity curve sample, call this once per tick/bar
    /// @param timestamp if non-empty, gets parsed and used to stamp equity
    /// curve entries + trade close times, leave blank if you don't care about time
    // returns state directly if you want ownership at whatever time
    std::pair<bool,bool> tick(long long epochSec) {
        if (openTrade) openTrade->advanceIdx(_prices.back());
        lastEpochSec = epochSec;
        float unreal = openTrade ? openTrade->td.profit : 0.f;
        float eq = realizedProfit + unreal;
        if (!equityCurve.empty() && equityCurve.back().first == epochSec) {
            equityCurve.back().second = eq;
        } else {
            equityCurve.emplace_back(epochSec, eq);
        }
        return {inLong, inShort};
    }

    std::pair<bool,bool> tick(std::string_view timestamp = {}) {
        if (timestamp.empty()) {
            if (openTrade) openTrade->advanceIdx(_prices.back());
            return {inLong, inShort};
        }
        return tick(mdDetail::tsToEpochSeconds(timestamp));
    }

    // Data processing
    int processedBars = 0;
    /// @brief pull `period` bars from the market data source, if timeframe is 0
    /// it reads raw ticks; otherwise it reads closes at that many seconds per bar.
    /// returns parallel prices + volumes + executedBuys/executedSells + deltas,
    /// callers usually std::move prices into their `Handling`-bound vector and
    /// feed volumes into returnVolumeProfile
    /// @param md       the MarketData source to read from
    /// @param period   how many rows/bars to load
    /// @param timeframe 0 = tick-by-tick, >0 = close every N seconds
    /// @param tickRes  timeframe==0 only: 1 = full resolution, how many raw ticks
    /// get read (and discarded) between each kept tick, to downsample tick-by-tick data
    DataWindow requestDataWindow(MarketData& md, int period, int timeframe=0, void (*whenUnknown)()=[](){},
                                bool supressWarnings=false, int tickRes=1) {

        if (!supressWarnings && tickRes != 1) {
            std::cout << "Warning! If tickRes is above one, the discarded ticks' volume is dropped entirely," << std::endl;
            std::cout << "so executedBuys/executedSells only cover the ticks that were kept" << std::endl;
            std::cout << "Supress these warnings by calling with supressWarnings (the fifth argument) set to true" << std::endl;
        }

        DataWindow out;
        out.prices.reserve(period);
        out.volumes.reserve(period);
        out.executedSells.reserve(period);
        out.executedBuys.reserve(period);
        out.deltas.reserve(period);
        windowTimestamps.clear();
        windowTimestamps.reserve(period);
        std::optional<float> prevPx = _prices.empty() ? std::nullopt : std::optional<float>(_prices.back());
        if (timeframe==0) {
            for (int i=0; i<period; i++) {
                std::optional<Tick> tick;
                for (int j=0; j<tickRes; j++) {
                    tick = md.nextTick();
                    if (!tick) break;
                }
                if (!tick) break;
                float px = 0.f;
                std::from_chars(tick->price.data(), tick->price.data() + tick->price.size(), px);
                out.prices.push_back(px);
                out.deltas.push_back(prevPx ? px - *prevPx : 0.f);
                prevPx = px;
                out.volumes.push_back(tick->size);
                windowTimestamps.push_back(mdDetail::tsToEpochSeconds(tick->timestamp));

                // aggressor split, pushed unconditionally so these stay index-parallel
                // with prices/volumes (a tick with no usable side contributes 0 to both)
                out.executedBuys.push_back(tick->executedBuys);
                out.executedSells.push_back(tick->executedSells);
                if (tick->unknownVolume > 0.f) whenUnknown(); // custom behavior hook for
                                                             // unclassifiable volume

                processedBars++;
            }
        } else {
            for (int i=0; i<period; i+=tickRes) {
                auto bar = md.nextClose(timeframe);
                if (!bar) break;
                float px = 0.f;
                std::from_chars(bar->price.data(), bar->price.data() + bar->price.size(), px);
                out.prices.push_back(px);
                out.deltas.push_back(prevPx ? px - *prevPx : 0.f);
                prevPx = px;
                out.volumes.push_back(bar->size);
                windowTimestamps.push_back(mdDetail::tsToEpochSeconds(bar->timestamp));

                // per-bar aggressor split, summed across every tick in the bar
                out.executedBuys.push_back(bar->executedBuys);
                out.executedSells.push_back(bar->executedSells);
                if (bar->unknownVolume > 0.f) whenUnknown();

                processedBars++;
            }
        }
        return out;
    }

    // other helper funcs and such

    int eof = -1; // set to -1 before user requests it
    void fetchEOF(int timeframe=1, int strideIncrement=2) {
        eof = findEof(kCSVMapping.path, timeframe, strideIncrement);
    }
};

/// @brief config function, makes setting up cleaner
/// @param tickSize self explanatory
/// @param tickValue self explanatory
struct Engine {
    std::vector<float> prices;
    MarketData md;
    Handling handler;

    Engine(float tickSize, float tickValue, bool calculateCosts = true)
        : md((loadConfig(), kCSVMapping.path))
        , handler(prices, tickSize, tickValue, calculateCosts) {}
};

Engine setupEngine(float tickSize, float tickValue, bool calculateCosts = true) {
    return Engine(tickSize, tickValue, calculateCosts);
}

class SetAnalytics {
private:
    std::vector<float> data;   

public:
    SetAnalytics(std::vector<float> data) : data(data) {}


    void updateVector(float newData) { data.push_back(newData); }
    void clearOutVector(int maxSize) { if (data.size() > maxSize) data.erase(data.begin()); }

    /// @brief rolling moving average over the tail end of `data`, uses the last
    /// period*2 values so you get `period` output points (one per point after warmup)
    /// `data` needs a length of at least period*2, only the tail gets touched
    /// @param period lookback length
    std::vector<float> returnRollingMovingAverage(int period) {
        if ((int)data.size() < period * 2) return {};
        std::vector<float> requiredChunk(data.end() - (period*2), data.end());
        std::vector<float> avgOverTime;

        float sum = 0;
        for (int i = 0; i < requiredChunk.size(); i++) {
            sum += requiredChunk[i];
            if (i >= period) {
                sum -= requiredChunk[i - period];
                avgOverTime.push_back(sum / static_cast<float>(period));
            }
        }
        return avgOverTime;
    }

    /// @brief population standard deviation over the whole set, one scalar
    /// use returnRollingStandardDeviation if you want a value per point instead
    float returnStandardDeviation() {
        if (data.size() < 2) return 0.f;

        double sum = 0.0;
        for (float v : data) sum += v;
        double mean = sum / static_cast<double>(data.size());

        double sqDiff = 0.0;
        for (float v : data) { double d = v - mean; sqDiff += d * d; }

        return static_cast<float>(std::sqrt(sqDiff / static_cast<double>(data.size())));
    }

    /// @brief rolling population standard deviation, same tail-slice convention as
    /// returnRollingMovingAverage so the outputs line up index for index with it
    /// accumulators are doubles since sumSq cancellation gets ugly on price-scale floats
    /// @param period lookback length
    std::vector<float> returnRollingStandardDeviation(int period) {
        if ((int)data.size() < period * 2) return {};
        std::vector<float> requiredChunk(data.end() - (period*2), data.end());
        std::vector<float> stdDevOverTime;

        double sum = 0.0, sumSq = 0.0;
        for (int i = 0; i < requiredChunk.size(); i++) {
            sum += requiredChunk[i];
            sumSq += static_cast<double>(requiredChunk[i]) * requiredChunk[i];
            if (i >= period) {
                sum -= requiredChunk[i - period];
                sumSq -= static_cast<double>(requiredChunk[i - period]) * requiredChunk[i - period];

                double mean = sum / static_cast<double>(period);
                double variance = sumSq / static_cast<double>(period) - mean * mean;
                if (variance < 0.0) variance = 0.0; // fp noise on a flat window
                stdDevOverTime.push_back(static_cast<float>(std::sqrt(variance)));
            }
        }
        return stdDevOverTime;
    }

    /// @brief rolling z-score, how many standard deviations each value sits from
    /// the mean of its own trailing window, same tail-slice convention as the others
    /// a flat window (zero stddev) yields 0 rather than a div by zero
    /// @param period lookback length
    std::vector<float> returnRollingZScore(int period) {
        if ((int)data.size() < period * 2) return {};
        std::vector<float> requiredChunk(data.end() - (period*2), data.end());
        std::vector<float> zOverTime;

        double sum = 0.0, sumSq = 0.0;
        for (int i = 0; i < requiredChunk.size(); i++) {
            sum += requiredChunk[i];
            sumSq += static_cast<double>(requiredChunk[i]) * requiredChunk[i];
            if (i >= period) {
                sum -= requiredChunk[i - period];
                sumSq -= static_cast<double>(requiredChunk[i - period]) * requiredChunk[i - period];

                double mean = sum / static_cast<double>(period);
                double variance = sumSq / static_cast<double>(period) - mean * mean;
                if (variance < 0.0) variance = 0.0;
                double stdDev = std::sqrt(variance);

                zOverTime.push_back(stdDev > 0.0
                    ? static_cast<float>((requiredChunk[i] - mean) / stdDev)
                    : 0.f);
            }
        }
        return zOverTime;
    }
};

class PriceAnalytics {
private:
    std::vector<float> prices;
    std::vector<float> volume;

public:
    PriceAnalytics(std::vector<float> prices={}, std::vector<float> volume={}) : prices(prices), volume(volume) {}

    void updatePrices(float price, float vol = 0.f) { prices.push_back(price); volume.push_back(vol); }
    void trimPrices(int maxSize) {
        if ((int)prices.size() > maxSize) { prices.erase(prices.begin()); volume.erase(volume.begin()); }
    }

    std::vector<float> returnSimpleMovingAverage(int period) { // "upstream" definition of returnRollingMovingAverage
        SetAnalytics sa(prices); // stack object, dies with the scope, nothing to free
        return sa.returnRollingMovingAverage(period);
    }

    /// @brief exponential moving average, same tail-slice convention as the SMA
    /// seeds with a simple average of the first `period` values, then applies the
    /// standard EMA formula from there
    /// @param period lookback / smoothing length
    std::vector<float> returnExponentialMovingAverage(int period) {
        if ((int)prices.size() < period * 2) return {};
        std::vector<float> requiredChunk(prices.end() - (period*2), prices.end());
        std::vector<float> emaOverTime;

        float multiplier = 2.0f / static_cast<float>(period + 1);

        float seed = 0;
        for (int i = 0; i < period; i++) { seed += requiredChunk[i]; }
        float ema = seed / static_cast<float>(period);

        for (int i = period; i < requiredChunk.size(); i++) {
            ema = (requiredChunk[i] - ema) * multiplier + ema;
            emaOverTime.push_back(ema);
        }

        return emaOverTime;
    }

    /// @brief builds a volume profile from `anchor` forward, groups volume by price
    /// level so you can see where the most trading happened
    /// @param anchor   starting index inside `prices` (everything before is ignored)
    /// @param prices   the price vec to scan
    /// @param volumeData per-bar volume, same length as prices
    /// @return [[price, volume], [price, volume], ...] feed this into returnValueArea
    std::vector<std::vector<float>> returnVolumeProfile(int anchor) { // anchor idx inside of the prices vec that gets passed
        std::unordered_map<float, float> map;
        for (int i=anchor; i<(int)prices.size(); i++) map[prices[i]] += volume[i];
        std::vector<std::vector<float>> passedPrices;
        passedPrices.reserve(map.size());
        for (auto& [px, vol] : map) passedPrices.push_back({px, vol});
        return passedPrices;
    } 

    /// @brief returns {VAL, VAH}, the price range containing 70% of total volume,
    /// expanding outward from the POC (highest-volume price level)
    /// @param volumeProfile output of returnVolumeProfile: [[price, volume], ...]
    /// @param pct           fraction of total volume to capture (default 0.70)
    /// @return {VAL, VAH} price pair, or {0,0} if the profile is empty
    std::vector<float> returnValueArea(std::vector<std::vector<float>> volumeProfile, float pct = 0.70f) {
        if (volumeProfile.empty()) return {0.f, 0.f};

        // Sort by price ascending so we can walk outward by index
        std::sort(volumeProfile.begin(), volumeProfile.end(),
            [](const std::vector<float>& a, const std::vector<float>& b) { return a[0] < b[0]; });

        // Total volume + find POC (index of highest-volume level)
        float totalVol = 0.f;
        int pocIdx = 0;
        for (int i = 0; i < (int)volumeProfile.size(); i++) {
            totalVol += volumeProfile[i][1];
            if (volumeProfile[i][1] > volumeProfile[pocIdx][1]) pocIdx = i;
        }

        float targetVol = totalVol * pct;
        float captured = volumeProfile[pocIdx][1];
        int lo = pocIdx;
        int hi = pocIdx;

        // Expand whichever side adds more volume until we hit the target
        while (captured < targetVol && (lo > 0 || hi < (int)volumeProfile.size() - 1)) {
            float volBelow = (lo > 0) ? volumeProfile[lo - 1][1] : 0.f;
            float volAbove = (hi < (int)volumeProfile.size() - 1) ? volumeProfile[hi + 1][1] : 0.f;

            if (lo <= 0) {
                hi++; captured += volAbove;
            } else if (hi >= (int)volumeProfile.size() - 1) {
                lo--; captured += volBelow;
            } else if (volBelow >= volAbove) {
                lo--; captured += volBelow;
            } else {
                hi++; captured += volAbove;
            }
        }

        return {volumeProfile[lo][0], volumeProfile[hi][0]};
    }

    /// @brief anchored volume-weighted average price, running/cumulative from
    /// `anchor` forward so vwap[i] reflects every bar from anchor through i,
    /// one output value per bar in that range (parallel to prices[anchor..])
    /// @param anchor starting index inside `prices` (everything before is ignored)
    /// @return running VWAP, one value per bar from anchor to the end of prices
    std::vector<float> returnVWAP(int anchor) {
        std::vector<float> vwap;
        if (anchor < 0 || anchor >= (int)prices.size()) return vwap;
        vwap.reserve(prices.size() - anchor);

        double sumPV = 0.0, sumV = 0.0;
        for (int i = anchor; i < (int)prices.size(); i++) {
            sumPV += (double)prices[i] * volume[i];
            sumV += volume[i];
            vwap.push_back(sumV > 0.0 ? static_cast<float>(sumPV / sumV) : prices[i]);
        }
        return vwap;
    }
};

// TODO - Use an OOP approach for this, as done above

/// @brief fraction of trades that were winners (0.0-1.0), returns 0 if no trades
float returnWinrate() {
    if (trades.empty()) return 0.0f;

    int cumTrades = 0;
    int cumWins = 0;

    for (const auto& t : trades) {
        ++cumTrades;
        if (t.win) ++cumWins;
    }

    return static_cast<float>(cumWins) / static_cast<float>(cumTrades);
}

/// @brief total profit across all closed trades, in points
float returnCumProfit() {
    if (trades.empty()) return 0.0f;

    float cumProfit = 0;

    for (auto& t : trades) { cumProfit += t.profit; }
    return cumProfit;
}

/// @brief equity curve bucketed into fixed-width time bars, each bar holds the
/// last equity value that fell inside it; empty bars carry forward
/// @param bucketMins width of each bar in minutes (default 1)
std::vector<float> returnProfitOverTime(int bucketMins = 1) {
    if (equityCurve.empty() || bucketMins <= 0) return std::vector<float>{};

    const long long bucketSec = static_cast<long long>(bucketMins) * 60LL;
    const long long firstBucket = equityCurve.front().first / bucketSec;
    const long long lastBucket  = equityCurve.back().first  / bucketSec;
    const size_t n = static_cast<size_t>(lastBucket - firstBucket + 1);

    // For each bucket, take the last equity sample that falls inside it.
    // Empty buckets carry the previous bucket's equity forward.
    std::vector<float> profitOverTime(n, 0.f);
    float lastEq = 0.f;
    size_t sampleIdx = 0;
    for (size_t b = 0; b < n; ++b) {
        long long bucketMax = (firstBucket + static_cast<long long>(b) + 1) * bucketSec - 1;
        while (sampleIdx < equityCurve.size() && equityCurve[sampleIdx].first <= bucketMax) {
            lastEq = equityCurve[sampleIdx].second;
            ++sampleIdx;
        }
        profitOverTime[b] = lastEq;
    }
    return profitOverTime;
}

/// @brief running average profit per trade, one value per closed trade, so you
/// can see if your edge is improving or degrading over time
std::vector<float> returnAverageProfitOverTime() {
    if (trades.empty()) return std::vector<float>{};

    std::vector<float> avgOverTime;
    avgOverTime.reserve(trades.size());
    float sum = 0.f;
    for (size_t i = 0; i < trades.size(); ++i) {
        sum += trades[i].profit;
        avgOverTime.push_back(sum / static_cast<float>(i + 1));
    }
    return avgOverTime;
}

/// @brief cumulative profit indexed per trade
std::vector<float> returnCumProfitPerTrade() {
    std::vector<float> curve;
    curve.reserve(trades.size());
    float cum = 0.f;
    for (auto& t : trades) {
        cum += t.profit;
        curve.push_back(cum);
    }
    return curve;
}

/// @brief cumulative profit bucketed by time period, matches returnMonteCarlo bucketing
/// @param bucketSecs bucket width in seconds (86400 = daily)
std::vector<float> returnCumProfitBucketed(int bucketSecs = 86400) {
    if (trades.empty() || bucketSecs <= 0) return {};
    long long bsec = (long long)bucketSecs;
    long long curBucket = trades[0].closeEpochSec / bsec;
    float bucketPnl = 0.f;
    float cum = 0.f;
    std::vector<float> curve;
    for (auto& t : trades) {
        long long tb = t.closeEpochSec / bsec;
        if (tb != curBucket) {
            cum += bucketPnl;
            curve.push_back(cum);
            for (long long gap = curBucket + 1; gap < tb; gap++) curve.push_back(cum);
            curBucket = tb;
            bucketPnl = 0.f;
        }
        bucketPnl += t.profit;
    }
    cum += bucketPnl;
    curve.push_back(cum);
    return curve;
}

/// @brief downsample a vector to at most maxPts points using largest-triangle-three-buckets-ish
/// keeps first and last, picks representative points in between
std::vector<float> downsample(const std::vector<float>& src, int maxPts) {
    int n = (int)src.size();
    if (n <= maxPts) return src;
    std::vector<float> out;
    out.reserve(maxPts);
    for (int i = 0; i < maxPts; i++) {
        int idx = (int)((long long)i * (n - 1) / (maxPts - 1));
        out.push_back(src[idx]);
    }
    return out;
}

/// @brief average P&L of trades matching `pred`, this is the building block for
/// returnAverageWinSize / returnAverageLossSize, but you can pass any filter
/// @param pred a callable that takes a tradeData and returns true for trades to include
template <typename Predicate>
float returnExpectancy(Predicate pred) {
    if (trades.empty()) return 0.0f;

    float cumPnL = 0;
    int count = 0;
    for (const auto& t : trades) {
        if (pred(t)) {
            cumPnL += t.profit;
            ++count;
        }
    }
    if (count == 0) return 0.0f;
    return cumPnL / static_cast<float>(count);
}

/// @brief average profit of winning trades (in pts)
float returnAverageWinSize() {
    return returnExpectancy([](const tradeData& t) { return t.win; });
}

/// @brief average loss of losing trades (in pts, will be negative)
float returnAverageLossSize() {
    return returnExpectancy([](const tradeData& t) { return !t.win; });
}

/// @brief average P&L across all trades (in pts)
float returnAvgPnl() {
    return returnExpectancy([](const tradeData&) { return true; });
}

/// @brief trades per calendar day based on first/last trade close timestamps
float returnTradesPerDay() {
    if (trades.size() < 2) return 0.f;
    long long first = trades.front().closeEpochSec;
    long long last  = trades.back().closeEpochSec;
    if (last <= first) return 0.f;
    float days = (last - first) / 86400.f;
    if (days < 0.001f) return 0.f;
    return (float)trades.size() / days;
}

// mc stuff only for equity curve, uses stationary bootstrap (Politis & Romano 1994)

/// @brief stationary bootstrap MC, resamples blocks of consecutive trades with
/// geometrically distributed block lengths to preserve serial dependence
/// @param sims how many simulations to run
/// @param avgBlockLen expected block length (controls how much dependence is kept)
/// @param bucketSecs if > 0, aggregate trade PnLs into buckets of this width before
///        bootstrapping (86400 = daily). output paths are per-bucket instead of per-trade
/// @param seed RNG seed, fixed by default so results are reproducible
std::vector<std::vector<float>> returnMonteCarlo(int sims, int avgBlockLen = 5,
    int bucketSecs = 0, unsigned seed = 42) {
    if (trades.empty() || sims <= 0) return {};

    // build the PnL series to bootstrap
    std::vector<float> pnls;
    if (bucketSecs > 0) {
        // bucket trades by time period
        long long bsec = (long long)bucketSecs;
        long long curBucket = trades[0].closeEpochSec / bsec;
        float bucketPnl = 0.f;
        for (auto& t : trades) {
            long long tb = t.closeEpochSec / bsec;
            if (tb != curBucket) {
                pnls.push_back(bucketPnl);
                // fill empty buckets with 0
                for (long long gap = curBucket + 1; gap < tb; gap++) pnls.push_back(0.f);
                curBucket = tb;
                bucketPnl = 0.f;
            }
            bucketPnl += t.profit;
        }
        pnls.push_back(bucketPnl);
    } else {
        pnls.reserve(trades.size());
        for (auto& t : trades) pnls.push_back(t.profit);
    }

    int n = (int)pnls.size();
    std::mt19937 rng(seed);
    std::geometric_distribution<int> blockDist(1.0 / avgBlockLen);
    std::uniform_int_distribution<int> startDist(0, n - 1);

    std::vector<std::vector<float>> paths;
    paths.reserve(sims);

    for (int s = 0; s < sims; s++) {
        std::vector<float> curve(n);
        float cum = 0;
        int i = 0;
        while (i < n) {
            int start = startDist(rng);
            int len = blockDist(rng) + 1;
            for (int b = 0; b < len && i < n; b++, i++) {
                cum += pnls[(start + b) % n];
                curve[i] = cum;
            }
        }
        paths.push_back(std::move(curve));
    }
    return paths;
}

/// @brief extract percentile lines from MC paths. for each trade step, sorts
/// across all sims and picks the value at each percentile
/// @param mcPaths output of returnMonteCarlo
/// @param percentiles list of percentiles 0-100 (e.g. {5, 50, 95})
/// @return one column per percentile, each column has trades.size() values
std::vector<std::vector<float>> returnPercentilePaths(
    const std::vector<std::vector<float>>& mcPaths,
    std::vector<int> percentiles) {

    if (mcPaths.empty() || percentiles.empty()) return {};
    int steps = (int)mcPaths[0].size();
    int nSims = (int)mcPaths.size();

    std::vector<std::vector<float>> result(percentiles.size());
    for (auto& r : result) r.resize(steps);

    std::vector<float> col(nSims);
    for (int step = 0; step < steps; step++) {
        for (int s = 0; s < nSims; s++) col[s] = mcPaths[s][step];
        std::sort(col.begin(), col.end());

        for (int p = 0; p < (int)percentiles.size(); p++) {
            int idx = (int)((percentiles[p] / 100.f) * (nSims - 1));
            if (idx >= nSims) idx = nSims - 1;
            result[p][step] = col[idx];
        }
    }
    return result;
}




