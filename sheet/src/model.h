#ifndef SHEET_MODEL_H
#define SHEET_MODEL_H

#include <stdbool.h>
#include "config.h"

enum cell_kind {
	CELL_EMPTY = 0,
	CELL_TEXT,
	CELL_NUMBER,
	CELL_FORMULA,
	CELL_ERROR
};

typedef struct {
	char text[CELL_TEXT_LEN];
	double value;
	unsigned char kind;
	unsigned char err; /* non-zero when kind == CELL_ERROR or eval failed */
} Cell;

typedef struct {
	Cell cells[SHEET_ROWS][SHEET_COLS];
	char filename[FILENAME_LEN];
	bool dirty;
	int cur_r;
	int cur_c;
	int view_r;
	int view_c;
} Sheet;

void sheet_init(Sheet *s);
void sheet_clear(Sheet *s);
Cell *sheet_cell(Sheet *s, int r, int c);
const Cell *sheet_cell_const(const Sheet *s, int r, int c);
void sheet_set_text(Sheet *s, int r, int c, const char *text);
void sheet_recalc(Sheet *s);

bool sheet_parse_ref(const char *p, int *r, int *c, const char **end);
void sheet_format_ref(int r, int c, char *out, int outlen);
const char *sheet_display(const Cell *cell, char *buf, int buflen);

#endif
