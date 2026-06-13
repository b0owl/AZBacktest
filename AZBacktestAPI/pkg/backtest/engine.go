package backtest

import (
	"fmt"
	"math/rand"
	"strconv"
	"time"
)

type trade struct {
	direction string
	entryMid  string
	entryTs   string
}

var activeTrades = map[string]trade{}
var tradeResults = map[string][]float64{}

var rowIndex int
var tickSize float64 = 0.25
var rowTimes []string

func fetch_time(entryTs string, row []string) float64 {
	entry, _ := time.Parse(time.RFC3339Nano, entryTs)
	current, _ := time.Parse(time.RFC3339Nano, row[0])
	return current.Sub(entry).Seconds()
}

func Tick(row []string) map[string][]float64 {
	rowTimes = append(rowTimes, row[0])
	rowIndex++
	bid, _ := strconv.ParseFloat(row[1], 64)
	ask, _ := strconv.ParseFloat(row[3], 64)
	currentPrice := (bid + ask) / 2.0

	for id, t := range activeTrades {
		entryPrice, _ := strconv.ParseFloat(t.entryMid, 64)
		var tickProfit float64
		if t.direction == "long" {
			tickProfit = (currentPrice - entryPrice) / tickSize
		} else {
			tickProfit = (entryPrice - currentPrice) / tickSize
		}
		tradeResults[id] = []float64{tickProfit, 1.0, fetch_time(t.entryTs, row)}
	}
	return tradeResults
}

func returnTradeResults(id string) []float64 {
	return tradeResults[id]
}

func Log(direction string, row []string) string {
	bid, _ := strconv.ParseFloat(row[1], 64)
	ask, _ := strconv.ParseFloat(row[3], 64)
	mid := fmt.Sprintf("%f", (bid+ask)/2.0)
	id := fmt.Sprintf("%s_%s_%03d", row[0], direction, rand.Intn(900)+100)
	activeTrades[id] = trade{direction: direction, entryMid: mid, entryTs: row[0]}
	tradeResults[id] = []float64{0.0, 1.0, 0.0}
	return id
}

func Clear(id string) {
	delete(activeTrades, id)
	delete(tradeResults, id)
}

// -- Logical Operands -- //

// TBBO Format:

// 0: ts_recv,1: ts_event,2: rtype,
// 3: publisher_id,4: instrument_id,5: action, []
// 6: side,7: depth,8: price,
// 9: size,10: flags,11: ts_in_delta,
// 12: sequence,
//
// 13: bid_px_00,14: ask_px_00,
// 15: bid_sz_00,16: ask_sz_00,
// 17: bid_ct_00, 18: ask_ct_00,
//
// 19: symbol

func rowIndexToTime(idx int) time.Time {
	if idx < 1 || idx > len(rowTimes) {
		return time.Time{}
	}
	t, _ := time.Parse(time.RFC3339Nano, rowTimes[idx-1])
	return t
}

func get_ask_bid_historical() {
	// TODO 
}
