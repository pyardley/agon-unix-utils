/*
 * Spreadsheet dimensions and display knobs.
 * Bump these to grow the grid; formula/UI code scales with them.
 */
#ifndef SHEET_CONFIG_H
#define SHEET_CONFIG_H

/* A–P; scroll horizontally when the screen is narrower. */
#define SHEET_COLS      16
/* More rows than one screen; arrow/PgUp/PgDn scroll the viewport. */
#define SHEET_ROWS      80
#define CELL_TEXT_LEN   32
#define COL_WIDTH        9
#define FILENAME_LEN    64

/* Max passes when resolving forward formula references. */
#define SHEET_EVAL_PASSES  8

#endif
