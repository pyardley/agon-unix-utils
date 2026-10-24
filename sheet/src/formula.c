#include "formula.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Tiny recursive-descent evaluator.
 *
 * Grammar:
 *   expr   := term (('+'|'-') term)*
 *   term   := factor (('*'|'/') factor)*
 *   factor := number | cell | cell:cell | NAME '(' args ')' | '(' expr ')' | '-' factor
 *   args   := expr (',' expr)*
 *
 * Bare A1:B2 is treated as SUM of that rectangle (SHEET.BBC-style).
 * Named functions live in fn_table[] — append entries to extend.
 */

#define MAX_ARGS 8

typedef struct {
	Sheet *sheet;
	int self_r;
	int self_c;
	const char *p;
	const char *errmsg;
	bool ok;
} Parser;

typedef double (*SheetFn)(Sheet *s, double *args, int nargs,
			  int r0, int c0, int r1, int c1, bool has_range,
			  const char **errmsg);

typedef struct {
	const char *name;
	SheetFn fn;
	int min_args; /* -1 = range form only (unused args) */
	int max_args;
	bool wants_range;
} FnEntry;

static void skip_ws(Parser *pr)
{
	while (*pr->p && isspace((unsigned char)*pr->p))
		pr->p++;
}

static void fail(Parser *pr, const char *msg)
{
	pr->ok = false;
	if (!pr->errmsg)
		pr->errmsg = msg;
}

static double cell_value(Sheet *s, int r, int c, int self_r, int self_c,
			 const char **errmsg)
{
	const Cell *cell;

	if (r == self_r && c == self_c) {
		*errmsg = "circular";
		return 0;
	}
	cell = sheet_cell_const(s, r, c);
	if (!cell || cell->kind == CELL_EMPTY)
		return 0;
	if (cell->kind == CELL_TEXT) {
		*errmsg = "text ref";
		return 0;
	}
	if (cell->kind == CELL_ERROR) {
		*errmsg = "ref error";
		return 0;
	}
	return cell->value;
}

static double range_reduce(Sheet *s, int r0, int c0, int r1, int c1,
			   int self_r, int self_c, int mode,
			   const char **errmsg)
{
	int r, c, n = 0;
	double sum = 0, mn = 0, mx = 0;

	if (r0 > r1) { int t = r0; r0 = r1; r1 = t; }
	if (c0 > c1) { int t = c0; c0 = c1; c1 = t; }

	for (r = r0; r <= r1; r++) {
		for (c = c0; c <= c1; c++) {
			const Cell *cell = sheet_cell_const(s, r, c);
			double v;

			if (r == self_r && c == self_c) {
				*errmsg = "circular";
				return 0;
			}
			if (!cell || cell->kind == CELL_EMPTY || cell->kind == CELL_TEXT)
				continue;
			if (cell->kind == CELL_ERROR) {
				*errmsg = "ref error";
				return 0;
			}
			v = cell->value;
			if (n == 0) {
				mn = mx = v;
			} else {
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			sum += v;
			n++;
		}
	}

	if (n == 0)
		return 0;
	switch (mode) {
	case 1: return sum / (double)n; /* AVG */
	case 2: return mn;
	case 3: return mx;
	case 4: return (double)n;       /* COUNT */
	default: return sum;            /* SUM */
	}
}

static double fn_sum(Sheet *s, double *args, int nargs,
		     int r0, int c0, int r1, int c1, bool has_range,
		     const char **errmsg)
{
	int i;
	double t = 0;
	(void)s; (void)r0; (void)c0; (void)r1; (void)c1; (void)errmsg;
	if (has_range)
		return range_reduce(s, r0, c0, r1, c1, -1, -1, 0, errmsg);
	for (i = 0; i < nargs; i++)
		t += args[i];
	return t;
}

static double fn_avg(Sheet *s, double *args, int nargs,
		     int r0, int c0, int r1, int c1, bool has_range,
		     const char **errmsg)
{
	int i;
	double t = 0;
	(void)s;
	if (has_range)
		return range_reduce(s, r0, c0, r1, c1, -1, -1, 1, errmsg);
	if (nargs == 0) {
		*errmsg = "AVG args";
		return 0;
	}
	for (i = 0; i < nargs; i++)
		t += args[i];
	return t / (double)nargs;
}

static double fn_min(Sheet *s, double *args, int nargs,
		     int r0, int c0, int r1, int c1, bool has_range,
		     const char **errmsg)
{
	int i;
	double m;
	(void)s;
	if (has_range)
		return range_reduce(s, r0, c0, r1, c1, -1, -1, 2, errmsg);
	if (nargs == 0) {
		*errmsg = "MIN args";
		return 0;
	}
	m = args[0];
	for (i = 1; i < nargs; i++)
		if (args[i] < m) m = args[i];
	return m;
}

static double fn_max(Sheet *s, double *args, int nargs,
		     int r0, int c0, int r1, int c1, bool has_range,
		     const char **errmsg)
{
	int i;
	double m;
	(void)s;
	if (has_range)
		return range_reduce(s, r0, c0, r1, c1, -1, -1, 3, errmsg);
	if (nargs == 0) {
		*errmsg = "MAX args";
		return 0;
	}
	m = args[0];
	for (i = 1; i < nargs; i++)
		if (args[i] > m) m = args[i];
	return m;
}

static double fn_count(Sheet *s, double *args, int nargs,
		       int r0, int c0, int r1, int c1, bool has_range,
		       const char **errmsg)
{
	(void)args;
	if (has_range)
		return range_reduce(s, r0, c0, r1, c1, -1, -1, 4, errmsg);
	return (double)nargs;
}

static double fn_abs(Sheet *s, double *args, int nargs,
		     int r0, int c0, int r1, int c1, bool has_range,
		     const char **errmsg)
{
	(void)s; (void)r0; (void)c0; (void)r1; (void)c1; (void)has_range;
	if (nargs != 1) {
		*errmsg = "ABS args";
		return 0;
	}
	return fabs(args[0]);
}

static double fn_sqrt(Sheet *s, double *args, int nargs,
		      int r0, int c0, int r1, int c1, bool has_range,
		      const char **errmsg)
{
	(void)s; (void)r0; (void)c0; (void)r1; (void)c1; (void)has_range;
	if (nargs != 1) {
		*errmsg = "SQRT args";
		return 0;
	}
	if (args[0] < 0) {
		*errmsg = "SQRT neg";
		return 0;
	}
	return sqrt(args[0]);
}

/* Extend the sheet by appending to this table. */
static const FnEntry fn_table[] = {
	{ "SUM",   fn_sum,   0, MAX_ARGS, true },
	{ "AVG",   fn_avg,   0, MAX_ARGS, true },
	{ "MIN",   fn_min,   0, MAX_ARGS, true },
	{ "MAX",   fn_max,   0, MAX_ARGS, true },
	{ "COUNT", fn_count, 0, MAX_ARGS, true },
	{ "ABS",   fn_abs,   1, 1, false },
	{ "SQRT",  fn_sqrt,  1, 1, false },
	{ NULL, NULL, 0, 0, false }
};

static const FnEntry *find_fn(const char *name)
{
	int i;
	for (i = 0; fn_table[i].name; i++) {
		if (strcmp(fn_table[i].name, name) == 0)
			return &fn_table[i];
	}
	return NULL;
}

static double parse_expr(Parser *pr);

static bool parse_range_or_cell(Parser *pr, int *r0, int *c0, int *r1, int *c1,
				bool *is_range)
{
	const char *end;

	*is_range = false;
	if (!sheet_parse_ref(pr->p, r0, c0, &end))
		return false;
	pr->p = end;
	skip_ws(pr);
	if (*pr->p == ':') {
		pr->p++;
		skip_ws(pr);
		if (!sheet_parse_ref(pr->p, r1, c1, &end)) {
			fail(pr, "bad range");
			return false;
		}
		pr->p = end;
		*is_range = true;
	} else {
		*r1 = *r0;
		*c1 = *c0;
	}
	return true;
}

static double parse_factor(Parser *pr)
{
	double v;
	char name[16];
	int ni = 0;
	const char *end;

	skip_ws(pr);
	if (!pr->ok)
		return 0;

	if (*pr->p == '(') {
		pr->p++;
		v = parse_expr(pr);
		skip_ws(pr);
		if (*pr->p != ')') {
			fail(pr, "')' expected");
			return 0;
		}
		pr->p++;
		return v;
	}

	if (*pr->p == '-') {
		pr->p++;
		return -parse_factor(pr);
	}
	if (*pr->p == '+') {
		pr->p++;
		return parse_factor(pr);
	}

	if (isdigit((unsigned char)*pr->p) ||
	    (*pr->p == '.' && isdigit((unsigned char)pr->p[1]))) {
		v = strtod(pr->p, (char **)&end);
		pr->p = end;
		return v;
	}

	/* Function name or cell reference */
	if (isalpha((unsigned char)*pr->p)) {
		const char *save = pr->p;
		int r0, c0, r1, c1;
		bool is_range;

		while (isalpha((unsigned char)*pr->p) && ni < (int)sizeof(name) - 1) {
			name[ni++] = (char)toupper((unsigned char)*pr->p);
			pr->p++;
		}
		name[ni] = '\0';
		skip_ws(pr);

		if (*pr->p == '(') {
			const FnEntry *fn = find_fn(name);
			double args[MAX_ARGS];
			int nargs = 0;
			bool has_range = false;
			int rr0 = 0, cc0 = 0, rr1 = 0, cc1 = 0;
			const char *errmsg = NULL;

			if (!fn) {
				fail(pr, "unknown fn");
				return 0;
			}
			pr->p++;
			skip_ws(pr);
			if (*pr->p != ')') {
				/* Prefer range form: SUM(A1:B2) */
				const char *before = pr->p;
				if (fn->wants_range &&
				    parse_range_or_cell(pr, &rr0, &cc0, &rr1, &cc1, &is_range) &&
				    is_range) {
					skip_ws(pr);
					if (*pr->p == ')') {
						has_range = true;
					} else {
						pr->p = before;
						pr->ok = true;
						pr->errmsg = NULL;
						goto parse_arg_list;
					}
				} else {
					pr->p = before;
parse_arg_list:
					for (;;) {
						if (nargs >= MAX_ARGS) {
							fail(pr, "too many args");
							return 0;
						}
						args[nargs++] = parse_expr(pr);
						if (!pr->ok)
							return 0;
						skip_ws(pr);
						if (*pr->p == ',') {
							pr->p++;
							continue;
						}
						break;
					}
				}
			}
			skip_ws(pr);
			if (*pr->p != ')') {
				fail(pr, "')' expected");
				return 0;
			}
			pr->p++;
			if (!has_range && (nargs < fn->min_args || nargs > fn->max_args)) {
				fail(pr, "arity");
				return 0;
			}
			if (has_range) {
				/* Re-evaluate range with self for circular detection */
				int mode = 0;
				if (strcmp(fn->name, "AVG") == 0) mode = 1;
				else if (strcmp(fn->name, "MIN") == 0) mode = 2;
				else if (strcmp(fn->name, "MAX") == 0) mode = 3;
				else if (strcmp(fn->name, "COUNT") == 0) mode = 4;
				v = range_reduce(pr->sheet, rr0, cc0, rr1, cc1,
						 pr->self_r, pr->self_c, mode, &errmsg);
			} else {
				v = fn->fn(pr->sheet, args, nargs, 0, 0, 0, 0, false, &errmsg);
			}
			if (errmsg) {
				fail(pr, errmsg);
				return 0;
			}
			return v;
		}

		/* Not a function call — rewind and parse as cell / range. */
		pr->p = save;
		if (!parse_range_or_cell(pr, &r0, &c0, &r1, &c1, &is_range)) {
			fail(pr, "bad ref");
			return 0;
		}
		if (is_range) {
			const char *errmsg = NULL;
			v = range_reduce(pr->sheet, r0, c0, r1, c1,
					 pr->self_r, pr->self_c, 0, &errmsg);
			if (errmsg) {
				fail(pr, errmsg);
				return 0;
			}
			return v;
		}
		{
			const char *errmsg = NULL;
			v = cell_value(pr->sheet, r0, c0, pr->self_r, pr->self_c, &errmsg);
			if (errmsg) {
				fail(pr, errmsg);
				return 0;
			}
			return v;
		}
	}

	fail(pr, "syntax");
	return 0;
}

static double parse_term(Parser *pr)
{
	double v = parse_factor(pr);
	for (;;) {
		skip_ws(pr);
		if (*pr->p == '*') {
			pr->p++;
			v *= parse_factor(pr);
		} else if (*pr->p == '/') {
			double d;
			pr->p++;
			d = parse_factor(pr);
			if (!pr->ok)
				return 0;
			if (d == 0) {
				fail(pr, "div0");
				return 0;
			}
			v /= d;
		} else {
			break;
		}
		if (!pr->ok)
			return 0;
	}
	return v;
}

static double parse_expr(Parser *pr)
{
	double v = parse_term(pr);
	for (;;) {
		skip_ws(pr);
		if (*pr->p == '+') {
			pr->p++;
			v += parse_term(pr);
		} else if (*pr->p == '-') {
			pr->p++;
			v -= parse_term(pr);
		} else {
			break;
		}
		if (!pr->ok)
			return 0;
	}
	return v;
}

bool formula_eval(Sheet *s, int self_r, int self_c, const char *expr,
		  double *out, const char **errmsg)
{
	Parser pr;
	double v;

	if (!expr)
		expr = "";
	if (*expr == '=')
		expr++;

	pr.sheet = s;
	pr.self_r = self_r;
	pr.self_c = self_c;
	pr.p = expr;
	pr.errmsg = NULL;
	pr.ok = true;

	v = parse_expr(&pr);
	skip_ws(&pr);
	if (pr.ok && *pr.p != '\0') {
		pr.ok = false;
		pr.errmsg = "trailing";
	}
	if (!pr.ok) {
		if (errmsg)
			*errmsg = pr.errmsg ? pr.errmsg : "error";
		return false;
	}
	*out = v;
	if (errmsg)
		*errmsg = NULL;
	return true;
}
