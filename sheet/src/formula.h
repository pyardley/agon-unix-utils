#ifndef SHEET_FORMULA_H
#define SHEET_FORMULA_H

#include "model.h"

/*
 * Evaluate a formula string (with or without leading '=').
 * Returns true on success and writes *out. On failure sets *errmsg
 * (may be NULL if caller does not care).
 *
 * Built-in functions are registered in formula.c via FnEntry —
 * add new ones there without touching the parser.
 */
bool formula_eval(Sheet *s, int self_r, int self_c, const char *expr,
		  double *out, const char **errmsg);

#endif
