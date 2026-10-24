#include "model.h"
#include "formula.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sheet_init(Sheet *s)
{
	memset(s, 0, sizeof(*s));
}

void sheet_clear(Sheet *s)
{
	char name[FILENAME_LEN];

	strncpy(name, s->filename, FILENAME_LEN - 1);
	name[FILENAME_LEN - 1] = '\0';
	sheet_init(s);
	strncpy(s->filename, name, FILENAME_LEN - 1);
	s->dirty = true;
}

Cell *sheet_cell(Sheet *s, int r, int c)
{
	if (r < 0 || r >= SHEET_ROWS || c < 0 || c >= SHEET_COLS)
		return NULL;
	return &s->cells[r][c];
}

const Cell *sheet_cell_const(const Sheet *s, int r, int c)
{
	if (r < 0 || r >= SHEET_ROWS || c < 0 || c >= SHEET_COLS)
		return NULL;
	return &s->cells[r][c];
}

bool sheet_parse_ref(const char *p, int *r, int *c, const char **end)
{
	int col = 0;
	int row = 0;
	const char *start = p;

	if (!p || !isalpha((unsigned char)*p))
		return false;

	while (isalpha((unsigned char)*p)) {
		char ch = (char)toupper((unsigned char)*p);
		col = col * 26 + (ch - 'A' + 1);
		p++;
		if (col > SHEET_COLS)
			return false;
	}
	if (col < 1 || !isdigit((unsigned char)*p))
		return false;

	while (isdigit((unsigned char)*p)) {
		row = row * 10 + (*p - '0');
		p++;
		if (row > SHEET_ROWS)
			return false;
	}
	if (row < 1)
		return false;
	if (p == start)
		return false;

	*c = col - 1;
	*r = row - 1;
	if (end)
		*end = p;
	return true;
}

void sheet_format_ref(int r, int c, char *out, int outlen)
{
	if (outlen < 6 || r < 0 || c < 0 || r >= SHEET_ROWS || c >= SHEET_COLS) {
		if (outlen > 0)
			out[0] = '\0';
		return;
	}
	snprintf(out, (size_t)outlen, "%c%d", 'A' + c, r + 1);
}

static bool looks_number(const char *t)
{
	char *end;
	if (!t || !*t)
		return false;
	strtod(t, &end);
	return end != t && *end == '\0';
}

void sheet_set_text(Sheet *s, int r, int c, const char *text)
{
	Cell *cell = sheet_cell(s, r, c);
	if (!cell)
		return;

	memset(cell, 0, sizeof(*cell));
	if (!text || !*text) {
		cell->kind = CELL_EMPTY;
		s->dirty = true;
		return;
	}

	strncpy(cell->text, text, CELL_TEXT_LEN - 1);
	cell->text[CELL_TEXT_LEN - 1] = '\0';

	if (cell->text[0] == '=') {
		cell->kind = CELL_FORMULA;
	} else if (looks_number(cell->text)) {
		cell->kind = CELL_NUMBER;
		cell->value = strtod(cell->text, NULL);
	} else {
		cell->kind = CELL_TEXT;
	}
	s->dirty = true;
}

/*
 * AgDev nanoprintf has no %f/%g — those print the format text literally.
 * Format with integer math instead (avoid long-long float converts).
 */
static void format_number(double v, char *buf, int buflen)
{
	char tmp[32];
	int neg = 0;
	unsigned long ip;
	unsigned long frac;
	int n;
	double rest;

	if (buflen < 2) {
		if (buflen > 0)
			buf[0] = '\0';
		return;
	}
	if (v < 0) {
		neg = 1;
		v = -v;
	}
	if (v > 99999999.0)
		v = 99999999.0;

	ip = (unsigned long)v;
	rest = (v - (double)ip) * 10000.0;
	if (rest < 0)
		rest = 0;
	frac = (unsigned long)(rest + 0.5);
	if (frac >= 10000UL) {
		ip++;
		frac = 0;
	}

	if (frac == 0)
		n = (int)snprintf(tmp, sizeof(tmp), "%s%lu", neg ? "-" : "", ip);
	else {
		while (frac > 0 && frac % 10UL == 0)
			frac /= 10UL;
		n = (int)snprintf(tmp, sizeof(tmp), "%s%lu.%lu",
				 neg ? "-" : "", ip, frac);
	}
	if (n < 0)
		n = 0;
	if (n >= buflen)
		n = buflen - 1;
	memcpy(buf, tmp, (size_t)n);
	buf[n] = '\0';
}

const char *sheet_display(const Cell *cell, char *buf, int buflen)
{
	if (!cell || buflen < 2) {
		if (buf && buflen > 0)
			buf[0] = '\0';
		return buf ? buf : "";
	}

	switch (cell->kind) {
	case CELL_EMPTY:
		buf[0] = '\0';
		break;
	case CELL_TEXT:
		strncpy(buf, cell->text, (size_t)buflen - 1);
		buf[buflen - 1] = '\0';
		break;
	case CELL_ERROR:
		strncpy(buf, "#ERR", (size_t)buflen - 1);
		buf[buflen - 1] = '\0';
		break;
	case CELL_NUMBER:
	case CELL_FORMULA:
		format_number(cell->value, buf, buflen);
		break;
	default:
		buf[0] = '\0';
		break;
	}
	return buf;
}

void sheet_recalc(Sheet *s)
{
	int pass, r, c;

	for (pass = 0; pass < SHEET_EVAL_PASSES; pass++) {
		bool changed = false;

		for (r = 0; r < SHEET_ROWS; r++) {
			for (c = 0; c < SHEET_COLS; c++) {
				Cell *cell = &s->cells[r][c];
				double v;
				const char *errmsg = NULL;

				if (cell->kind != CELL_FORMULA && cell->kind != CELL_ERROR)
					continue;
				if (cell->text[0] != '=')
					continue;

				if (formula_eval(s, r, c, cell->text, &v, &errmsg)) {
					if (cell->kind != CELL_FORMULA || cell->err ||
					    cell->value != v) {
						cell->kind = CELL_FORMULA;
						cell->value = v;
						cell->err = 0;
						changed = true;
					}
				} else if (cell->kind != CELL_ERROR) {
					cell->kind = CELL_ERROR;
					cell->value = 0;
					cell->err = 1;
					changed = true;
				}
			}
		}
		if (!changed)
			break;
	}
}
