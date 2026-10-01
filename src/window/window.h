#pragma once
#include <string>

void showConsole(const char* title, void (skin)());

void setTiling(bool on);
void setTileGrid(int cols, int rows);
void tileWindow(const std::string& key, int col, int row, int colSpan = 1, int rowSpan = 1);
