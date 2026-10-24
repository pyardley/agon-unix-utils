#include "csv.h"

#include <stdio.h>
#include <string.h>

/*
 * CSV layout: rows of comma-separated cell text (formulas keep leading '=').
 * Cells containing comma or quote are wrapped in double quotes; quotes doubled.
 *
 * AgDev libc fputs() always appends a newline (non-ISO). Never use fputs for
 * field data — it turned every cell into its own CSV record.
 */

static void write_raw(FILE *fp, const char *text, size_t n)
{
	if (n)
		fwrite(text, 1, n, fp);
}

static void write_char(FILE *fp, char ch)
{
	fwrite(&ch, 1, 1, fp);
}

static void write_field(FILE *fp, const char *text)
{
	bool need_q = false;
	const char *p;
	size_t n;

	if (!text)
		text = "";
	for (p = text; *p; p++) {
		if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
			need_q = true;
			break;
		}
	}
	if (!need_q) {
		n = strlen(text);
		write_raw(fp, text, n);
		return;
	}
	write_char(fp, '"');
	for (p = text; *p; p++) {
		if (*p == '"')
			write_char(fp, '"');
		write_char(fp, *p);
	}
	write_char(fp, '"');
}

bool sheet_save_csv(const Sheet *s, const char *path, const char **errmsg)
{
	FILE *fp;
	int r, c;
	int max_r = -1, max_c = -1;

	/* Binary mode avoids host/text translation surprises. */
	fp = fopen(path, "wb");
	if (!fp) {
		if (errmsg)
			*errmsg = "cannot write";
		return false;
	}

	for (r = 0; r < SHEET_ROWS; r++) {
		for (c = 0; c < SHEET_COLS; c++) {
			if (s->cells[r][c].kind != CELL_EMPTY) {
				if (r > max_r) max_r = r;
				if (c > max_c) max_c = c;
			}
		}
	}
	if (max_r < 0) {
		write_char(fp, '\n');
		fclose(fp);
		return true;
	}

	for (r = 0; r <= max_r; r++) {
		for (c = 0; c <= max_c; c++) {
			if (c)
				write_char(fp, ',');
			write_field(fp, s->cells[r][c].text);
		}
		write_char(fp, '\n');
	}
	fclose(fp);
	return true;
}

static int parse_csv_line(char *line, char *fields[], int max_fields)
{
	int n = 0;
	char *p = line;

	while (*p && n < max_fields) {
		if (*p == '"') {
			char *dst;
			p++;
			fields[n++] = p;
			dst = p;
			while (*p) {
				if (*p == '"') {
					if (p[1] == '"') {
						*dst++ = '"';
						p += 2;
						continue;
					}
					p++;
					break;
				}
				*dst++ = *p++;
			}
			*dst = '\0';
			if (*p == ',')
				p++;
		} else {
			fields[n++] = p;
			while (*p && *p != ',')
				p++;
			if (*p == ',') {
				*p = '\0';
				p++;
			}
		}
	}
	return n;
}

bool sheet_load_csv(Sheet *s, const char *path, const char **errmsg)
{
	FILE *fp;
	char line[SHEET_COLS * CELL_TEXT_LEN + 64];
	int r = 0;
	char name[FILENAME_LEN];

	fp = fopen(path, "rb");
	if (!fp) {
		if (errmsg)
			*errmsg = "cannot open";
		return false;
	}

	strncpy(name, path, FILENAME_LEN - 1);
	name[FILENAME_LEN - 1] = '\0';
	sheet_init(s);
	strncpy(s->filename, name, FILENAME_LEN - 1);

	while (fgets(line, sizeof(line), fp) && r < SHEET_ROWS) {
		char *fields[SHEET_COLS];
		int n, c;
		size_t len = strlen(line);

		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		/* Skip empty trailing lines without consuming a sheet row. */
		if (len == 0)
			continue;

		memset(fields, 0, sizeof(fields));
		n = parse_csv_line(line, fields, SHEET_COLS);
		for (c = 0; c < n && c < SHEET_COLS; c++) {
			if (fields[c] && fields[c][0])
				sheet_set_text(s, r, c, fields[c]);
		}
		r++;
	}
	fclose(fp);
	sheet_recalc(s);
	s->dirty = false;
	return true;
}
