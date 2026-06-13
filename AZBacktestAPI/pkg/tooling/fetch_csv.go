package tooling

import (
	"encoding/csv"
	"os"
)

var csvReader *csv.Reader
var csvFile *os.File

func InitReader(path string) {
	csvFile, _ = os.Open(path)
	csvReader = csv.NewReader(csvFile)
}

func NextRow() ([]string, error) {
	return csvReader.Read()
}