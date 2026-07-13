#pragma once

// Wraparound helpers for a fixed-column, rectangular (rows * columns == itemCount) icon grid,
// shared by HomeActivity's main grid and ToolsFolderActivity's grid so the wraparound math
// isn't duplicated between them.
namespace GridNavigator {

inline void moveLeft(int& index, int columns, int /*itemCount*/) {
  const int row = index / columns;
  const int col = (index % columns - 1 + columns) % columns;
  index = row * columns + col;
}

inline void moveRight(int& index, int columns, int /*itemCount*/) {
  const int row = index / columns;
  const int col = (index % columns + 1) % columns;
  index = row * columns + col;
}

inline void moveUp(int& index, int columns, int itemCount) {
  const int rows = itemCount / columns;
  const int col = index % columns;
  const int row = (index / columns - 1 + rows) % rows;
  index = row * columns + col;
}

inline void moveDown(int& index, int columns, int itemCount) {
  const int rows = itemCount / columns;
  const int col = index % columns;
  const int row = (index / columns + 1) % rows;
  index = row * columns + col;
}

}  // namespace GridNavigator
