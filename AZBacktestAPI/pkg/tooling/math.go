package tooling

import "math"

func Mean(data []float64) float64 {
	var sum float64
	for _, value := range data {
		sum += value
	}
	return sum / float64(len(data))
}

func Stdev(data []float64, avg float64) float64 {
	var variance float64
	for _, value := range data {
		variance += math.Pow(value-avg, 2)
	}
	return math.Sqrt(variance / float64(len(data)))
}
