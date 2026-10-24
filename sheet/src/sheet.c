/*
 * sheet — simple extensible spreadsheet for Agon MOS (moslet).
 *
 * Navigation: arrows / hjkl / Tab
 * Edit:       Enter (or start typing) — uses MOS edit line
 * Commands:   / then s|l|w|q|c|g|h
 * Esc:        quit (or /q! if dirty)
 */
#include "csv.h"
#include "formula.h"
#include "keylog.h"
#include "model.h"

#include <agon/vdp_vdu.h>
#include <ctype.h>
#include <mos_api.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define STATUS_LEN  72
#define EDIT_LEN    CELL_TEXT_LEN

/* Mode 0: 640×480, 16 colours, ~80×60 text (mode 1 is only 4 colours). */
#define SHEET_SCREEN_MODE 0

/* FabGL VirtualKey codes (fabutils.h) — used as a fallback to kbmap. */
#define VK_ESCAPE    125
#define VK_BACKSPACE 132
#define VK_HOME      133
#define VK_END       135
#define VK_TAB       142
#define VK_RETURN    143
#define VK_PAGEUP    146
#define VK_PAGEDOWN  148
#define VK_UP        150
#define VK_KP_UP     151
#define VK_DOWN      152
#define VK_KP_DOWN   153
#define VK_LEFT      154
#define VK_KP_LEFT   155
#define VK_RIGHT     156
#define VK_KP_RIGHT  157

enum {
	ACT_NONE = 0,
	ACT_MOVE,
	ACT_FULL,
	ACT_BARS,
	ACT_QUIT
};

static void edit_cell(Sheet *s, const char *seed);
static void clear_cell(Sheet *s);

/*
 * Keyboard bitmap (mos_getkbmap) — offsets/bits from Agon docs.
 * Edge-detected so we get one event per press.
 */
#define KB_LEFT_OFF   0x03
#define KB_LEFT_BIT   0
#define KB_DOWN_OFF   0x05
#define KB_DOWN_BIT   0
#define KB_UP_OFF     0x07
#define KB_UP_BIT     1
#define KB_RIGHT_OFF  0x0F
#define KB_RIGHT_BIT  0
#define KB_TAB_OFF    0x0C
#define KB_TAB_BIT    0
#define KB_ENTER_OFF  0x09
#define KB_ENTER_BIT  0
#define KB_ESC_OFF    0x0E
#define KB_ESC_BIT    0
#define KB_BKSP_OFF   0x05
#define KB_BKSP_BIT   7
#define KB_PGUP_OFF   0x07
#define KB_PGUP_BIT   7
#define KB_PGDN_OFF   0x09
#define KB_PGDN_BIT   7
#define KB_HOME_OFF   0x07
#define KB_HOME_BIT   6
#define KB_END_OFF    0x0D
#define KB_END_BIT    2
#define KB_DEL_OFF    0x0B
#define KB_DEL_BIT    0

static Sheet g_sheet;
static volatile SYSVAR *g_sv;
static char g_status[STATUS_LEN];
static int g_grid_rows;
static int g_grid_cols;
static uint8_t g_prev_mode;
static bool g_mode_set;
static uint8_t g_prev_kb[16];
static uint8_t g_last_vkeycount;
static int g_prev_r, g_prev_c;
static int g_prev_view_r, g_prev_view_c;
/* ANSI CSI parser for hosts/emulators that send Esc[A etc. for arrows. */
static int g_csi; /* 0=normal, 1=saw ESC, 2=saw ESC[ */

static void set_status(const char *msg)
{
	strncpy(g_status, msg ? msg : "", STATUS_LEN - 1);
	g_status[STATUS_LEN - 1] = '\0';
}

static void ensure_visible(Sheet *s)
{
	if (s->cur_r < s->view_r)
		s->view_r = s->cur_r;
	if (s->cur_c < s->view_c)
		s->view_c = s->cur_c;
	if (s->cur_r >= s->view_r + g_grid_rows)
		s->view_r = s->cur_r - g_grid_rows + 1;
	if (s->cur_c >= s->view_c + g_grid_cols)
		s->view_c = s->cur_c - g_grid_cols + 1;
	if (s->view_r < 0)
		s->view_r = 0;
	if (s->view_c < 0)
		s->view_c = 0;
}

static void move_cur(Sheet *s, int dr, int dc)
{
	int nr = s->cur_r + dr;
	int nc = s->cur_c + dc;
	if (nr < 0) nr = 0;
	if (nc < 0) nc = 0;
	if (nr >= SHEET_ROWS) nr = SHEET_ROWS - 1;
	if (nc >= SHEET_COLS) nc = SHEET_COLS - 1;
	s->cur_r = nr;
	s->cur_c = nc;
	ensure_visible(s);
}

static void put_n(const char *s, int width, bool hilite)
{
	char buf[32];
	int i, n, inner;

	if (width < 3)
		width = 3;
	if (width >= (int)sizeof(buf))
		width = (int)sizeof(buf) - 1;

	/*
	 * Selected cell: [value] so it's obvious even in low-colour modes.
	 * Colour 2 (green) also helps when 16-colour mode 0 is active.
	 */
	if (hilite) {
		inner = width - 2;
		n = (int)strlen(s);
		if (n > inner)
			n = inner;
		buf[0] = '[';
		for (i = 0; i < n; i++)
			buf[1 + i] = s[i];
		for (; i < inner; i++)
			buf[1 + i] = ' ';
		buf[width - 1] = ']';
		buf[width] = '\0';
		vdp_set_text_colour(2);
		fputs(buf, stdout);
		fflush(stdout);
		vdp_set_text_colour(7);
		return;
	}

	n = (int)strlen(s);
	if (n > width)
		n = width;
	for (i = 0; i < n; i++)
		buf[i] = s[i];
	for (; i < width; i++)
		buf[i] = ' ';
	buf[width] = '\0';
	fputs(buf, stdout);
	fflush(stdout);
}

static void refresh_dims(void)
{
	/* Wait until VDP reports the new mode geometry. */
	g_sv->vdp_pflags = (uint8_t)(g_sv->vdp_pflags & (uint8_t)~vdp_pflag_mode);
	vdp_get_scr_dims(true);
}

static void setup_screen(void)
{
	uint8_t *kb;
	int i;

	g_prev_mode = g_sv->scrMode;
	vdp_page_mode_off();
	vdp_cursor_enable(false);
	vdp_mode(SHEET_SCREEN_MODE);
	refresh_dims();
	/* Fallback if dims never arrived */
	if (g_sv->scrCols < 40 || g_sv->scrRows < 16) {
		vdp_mode(SHEET_SCREEN_MODE);
		refresh_dims();
	}
	g_mode_set = true;

	kb = mos_getkbmap();
	for (i = 0; i < 16; i++)
		g_prev_kb[i] = kb ? kb[i] : 0;
	g_last_vkeycount = g_sv->vkeycount;
	g_csi = 0;
}

static void restore_screen(void)
{
	if (!g_mode_set)
		return;
	vdp_cls();
	if (g_prev_mode != SHEET_SCREEN_MODE)
		vdp_mode(g_prev_mode);
	vdp_cursor_enable(true);
	g_mode_set = false;
}

static void line_at(int x, int y, const char *text, int maxcols)
{
	char buf[128];
	int n, i;

	if (maxcols > (int)sizeof(buf) - 1)
		maxcols = (int)sizeof(buf) - 1;
	if (maxcols < 1)
		return;
	n = (int)strlen(text);
	if (n > maxcols)
		n = maxcols;
	for (i = 0; i < n; i++)
		buf[i] = text[i];
	for (; i < maxcols; i++)
		buf[i] = ' ';
	buf[maxcols] = '\0';
	vdp_cursor_tab(x, y);
	fputs(buf, stdout);
}

static void layout(void)
{
	int cols = g_sv->scrCols;
	int rows = g_sv->scrRows;

	if (cols < 1)
		cols = 1;
	if (rows < 5)
		rows = 5;

	g_grid_cols = (cols - 4) / COL_WIDTH;
	if (g_grid_cols > SHEET_COLS)
		g_grid_cols = SHEET_COLS;
	if (g_grid_cols < 1)
		g_grid_cols = 1;

	g_grid_rows = rows - 4;
	if (g_grid_rows > SHEET_ROWS)
		g_grid_rows = SHEET_ROWS;
	if (g_grid_rows < 1)
		g_grid_rows = 1;
}

static void draw_bars(Sheet *s)
{
	char ref[8];
	char line[128];
	int cols = g_sv->scrCols;
	const Cell *cell;

	if (cols < 1)
		cols = 1;

	sheet_format_ref(s->cur_r, s->cur_c, ref, sizeof(ref));
	cell = sheet_cell_const(s, s->cur_r, s->cur_c);
	snprintf(line, sizeof(line), "%s  %s", ref,
		 cell && cell->text[0] ? cell->text : "");
	line_at(0, 0, line, cols);

	snprintf(line, sizeof(line), "%s%s  %s",
		 s->filename[0] ? s->filename : "(untitled)",
		 s->dirty ? "*" : " ",
		 g_status);
	line_at(0, 1, line, cols);
}

static void draw_headers(Sheet *s)
{
	int c;
	int cols = g_sv->scrCols;

	/* Clear header row, then place each label absolutely. */
	line_at(0, 2, "", cols > 0 ? cols : 1);
	vdp_cursor_tab(0, 2);
	fputs("   ", stdout);
	fflush(stdout);
	for (c = 0; c < g_grid_cols; c++) {
		int cc = s->view_c + c;
		char hdr[COL_WIDTH + 1];
		int x = 3 + c * COL_WIDTH;
		if (cc >= SHEET_COLS)
			break;
		snprintf(hdr, sizeof(hdr), "%*c", COL_WIDTH / 2 + 1, 'A' + cc);
		vdp_cursor_tab(x, 2);
		put_n(hdr, COL_WIDTH, cc == s->cur_c);
	}
}

static void draw_grid(Sheet *s)
{
	char line[8];
	char disp[COL_WIDTH + 1];
	int r, c;
	int rows = g_sv->scrRows;

	for (r = 0; r < g_grid_rows; r++) {
		int rr = s->view_r + r;
		int y = 3 + r;

		if (y >= rows - 1 || rr >= SHEET_ROWS)
			break;
		vdp_cursor_tab(0, y);
		snprintf(line, sizeof(line), "%2d ", rr + 1);
		fputs(line, stdout);
		fflush(stdout);
		for (c = 0; c < g_grid_cols; c++) {
			int cc = s->view_c + c;
			const Cell *cell;
			int x = 3 + c * COL_WIDTH;
			if (cc >= SHEET_COLS)
				break;
			cell = sheet_cell_const(s, rr, cc);
			sheet_display(cell, disp, sizeof(disp));
			vdp_cursor_tab(x, y);
			put_n(disp, COL_WIDTH, rr == s->cur_r && cc == s->cur_c);
		}
	}
}

static void draw_help(void)
{
	int cols = g_sv->scrCols;
	int rows = g_sv->scrRows;
	if (cols < 1)
		cols = 1;
	if (rows < 1)
		return;
	line_at(0, rows - 1,
		"Arrows/hjkl move  Enter edit  /cmd  ?help  Esc quit",
		cols);
}

static void draw_cell_at(Sheet *s, int r, int c, bool hilite)
{
	char disp[COL_WIDTH + 1];
	const Cell *cell;
	int vr = r - s->view_r;
	int vc = c - s->view_c;
	int x, y;

	if (vr < 0 || vc < 0 || vr >= g_grid_rows || vc >= g_grid_cols)
		return;
	y = 3 + vr;
	if (y >= g_sv->scrRows - 1)
		return;
	x = 3 + vc * COL_WIDTH;
	cell = sheet_cell_const(s, r, c);
	sheet_display(cell, disp, sizeof(disp));
	vdp_cursor_tab(x, y);
	put_n(disp, COL_WIDTH, hilite);
}

static void draw_header_col(Sheet *s, int c, bool hilite)
{
	char hdr[COL_WIDTH + 1];
	int vc = c - s->view_c;
	int x;

	if (vc < 0 || vc >= g_grid_cols || c < 0 || c >= SHEET_COLS)
		return;
	x = 3 + vc * COL_WIDTH;
	snprintf(hdr, sizeof(hdr), "%*c", COL_WIDTH / 2 + 1, 'A' + c);
	vdp_cursor_tab(x, 2);
	put_n(hdr, COL_WIDTH, hilite);
}

static void draw_full(Sheet *s)
{
	vdp_cls();
	layout();
	ensure_visible(s);
	draw_bars(s);
	draw_headers(s);
	draw_grid(s);
	draw_help();
	g_prev_r = s->cur_r;
	g_prev_c = s->cur_c;
	g_prev_view_r = s->view_r;
	g_prev_view_c = s->view_c;
}

/*
 * Arrow / hjkl move: keep chrome on screen (no CLS). Only rewrite the
 * formula/status bars and the old/new cell (+ column header marker).
 */
static void draw_move(Sheet *s)
{
	int old_r = g_prev_r;
	int old_c = g_prev_c;

	ensure_visible(s);
	if (s->view_r != g_prev_view_r || s->view_c != g_prev_view_c) {
		draw_full(s);
		return;
	}

	/* Top two lines stay put — just refresh their text. */
	draw_bars(s);

	if (old_r != s->cur_r || old_c != s->cur_c) {
		draw_cell_at(s, old_r, old_c, false);
		draw_cell_at(s, s->cur_r, s->cur_c, true);
		if (old_c != s->cur_c) {
			draw_header_col(s, old_c, false);
			draw_header_col(s, s->cur_c, true);
		}
	}

	g_prev_r = s->cur_r;
	g_prev_c = s->cur_c;
}

static bool kb_edge(const uint8_t *kb, int off, int bit)
{
	bool now = (kb[off] & (1u << bit)) != 0;
	bool was = (g_prev_kb[off] & (1u << bit)) != 0;
	return now && !was;
}

#if SHEET_KEYLOG
static void keylog_event(const char *src, uint8_t ascii, uint8_t vkey,
			 uint8_t down, const uint8_t *kb, int act)
{
	FILE *fp = fopen("sheetkeys.log", "a");
	if (!fp)
		return;
	fprintf(fp,
		"%s ascii=%u vkey=%u down=%u act=%d cur=%d,%d "
		"kb03=%02X kb05=%02X kb07=%02X kb0f=%02X\n",
		src, (unsigned)ascii, (unsigned)vkey, (unsigned)down, act,
		g_sheet.cur_r, g_sheet.cur_c,
		kb ? kb[0x03] : 0, kb ? kb[0x05] : 0,
		kb ? kb[0x07] : 0, kb ? kb[0x0F] : 0);
	fclose(fp);
}
#else
static void keylog_event(const char *src, uint8_t ascii, uint8_t vkey,
			 uint8_t down, const uint8_t *kb, int act)
{
	(void)src; (void)ascii; (void)vkey; (void)down; (void)kb; (void)act;
}
#endif

static int handle_vkey(Sheet *s, uint8_t vk)
{
	switch (vk) {
	case VK_LEFT:
	case VK_KP_LEFT:
		move_cur(s, 0, -1);
		return ACT_MOVE;
	case VK_RIGHT:
	case VK_KP_RIGHT:
		move_cur(s, 0, 1);
		return ACT_MOVE;
	case VK_UP:
	case VK_KP_UP:
		move_cur(s, -1, 0);
		return ACT_MOVE;
	case VK_DOWN:
	case VK_KP_DOWN:
		move_cur(s, 1, 0);
		return ACT_MOVE;
	case VK_TAB:
		move_cur(s, 0, 1);
		return ACT_MOVE;
	case VK_HOME:
		s->cur_c = 0;
		ensure_visible(s);
		return ACT_MOVE;
	case VK_END:
		s->cur_c = SHEET_COLS - 1;
		ensure_visible(s);
		return ACT_MOVE;
	case VK_PAGEUP:
		move_cur(s, -g_grid_rows, 0);
		return ACT_MOVE;
	case VK_PAGEDOWN:
		move_cur(s, g_grid_rows, 0);
		return ACT_MOVE;
	case VK_RETURN:
		edit_cell(s, NULL);
		return ACT_FULL;
	case VK_BACKSPACE:
		clear_cell(s);
		return ACT_FULL;
	case VK_ESCAPE:
		if (s->dirty) {
			set_status("dirty — /q! to quit, /s to save");
			return ACT_BARS;
		}
		return ACT_QUIT;
	default:
		return ACT_NONE;
	}
}

static void cmd_help(void)
{
	set_status("/s file  /l file  /w  /q  /c  /g A1  ?=help");
}

static void run_command(Sheet *s, const char *cmd)
{
	const char *errmsg = NULL;
	char arg[FILENAME_LEN];
	char ch;

	while (*cmd && isspace((unsigned char)*cmd))
		cmd++;
	if (!*cmd)
		return;
	ch = (char)tolower((unsigned char)*cmd);
	cmd++;
	while (*cmd && isspace((unsigned char)*cmd))
		cmd++;
	strncpy(arg, cmd, FILENAME_LEN - 1);
	arg[FILENAME_LEN - 1] = '\0';

	switch (ch) {
	case 'h':
	case '?':
		cmd_help();
		break;
	case 'q':
		if (s->dirty && arg[0] != '!') {
			set_status("dirty — /q! to quit anyway");
			break;
		}
		s->dirty = false;
		set_status("QUIT");
		break;
	case 'c':
		sheet_clear(s);
		sheet_recalc(s);
		set_status("cleared");
		break;
	case 'w':
	case 's': {
		const char *path = arg[0] ? arg : s->filename;
		if (!path[0]) {
			set_status("usage: /s filename");
			break;
		}
		if (sheet_save_csv(s, path, &errmsg)) {
			strncpy(s->filename, path, FILENAME_LEN - 1);
			s->filename[FILENAME_LEN - 1] = '\0';
			s->dirty = false;
			set_status("saved");
		} else {
			set_status(errmsg ? errmsg : "save failed");
		}
		break;
	}
	case 'l':
	case 'e':
		if (!arg[0]) {
			set_status("usage: /l filename");
			break;
		}
		if (sheet_load_csv(s, arg, &errmsg))
			set_status("loaded");
		else
			set_status(errmsg ? errmsg : "load failed");
		break;
	case 'g': {
		int r, c;
		const char *end;
		if (!sheet_parse_ref(arg, &r, &c, &end) || *end) {
			set_status("usage: /g A1");
			break;
		}
		s->cur_r = r;
		s->cur_c = c;
		ensure_visible(s);
		set_status("ok");
		break;
	}
	default:
		set_status("unknown cmd — ? for help");
		break;
	}
}

static void prompt_command(Sheet *s)
{
	char buf[FILENAME_LEN + 4];
	uint8_t rc;

	vdp_cursor_enable(true);
	vdp_cursor_tab(0, 1);
	fputs("/ ", stdout);
	buf[0] = '\0';
	rc = mos_editline(buf, sizeof(buf), 1);
	vdp_cursor_enable(false);
	if (rc == 27) {
		set_status("cancelled");
		return;
	}
	run_command(s, buf);
}

static void edit_cell(Sheet *s, const char *seed)
{
	char buf[EDIT_LEN];
	char ref[8];
	char prompt[16];
	const Cell *cell;
	uint8_t rc;

	sheet_format_ref(s->cur_r, s->cur_c, ref, sizeof(ref));
	cell = sheet_cell_const(s, s->cur_r, s->cur_c);

	if (seed && seed[0]) {
		strncpy(buf, seed, EDIT_LEN - 1);
		buf[EDIT_LEN - 1] = '\0';
	} else if (cell && cell->text[0]) {
		strncpy(buf, cell->text, EDIT_LEN - 1);
		buf[EDIT_LEN - 1] = '\0';
	} else {
		buf[0] = '\0';
	}

	snprintf(prompt, sizeof(prompt), "%s> ", ref);
	vdp_cursor_enable(true);
	vdp_cursor_tab(0, 0);
	fputs(prompt, stdout);
	/* clearbuffer=0 keeps seed/existing text for editing */
	rc = mos_editline(buf, sizeof(buf), 0);
	vdp_cursor_enable(false);

	if (rc == 27) {
		set_status("cancelled");
		return;
	}
	sheet_set_text(s, s->cur_r, s->cur_c, buf);
	sheet_recalc(s);
	set_status("ok");
}

static void clear_cell(Sheet *s)
{
	sheet_set_text(s, s->cur_r, s->cur_c, "");
	sheet_recalc(s);
	set_status("cleared cell");
}

static int handle_nav_keys(Sheet *s, const uint8_t *kb)
{
	if (kb_edge(kb, KB_LEFT_OFF, KB_LEFT_BIT)) {
		move_cur(s, 0, -1);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_RIGHT_OFF, KB_RIGHT_BIT)) {
		move_cur(s, 0, 1);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_UP_OFF, KB_UP_BIT)) {
		move_cur(s, -1, 0);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_DOWN_OFF, KB_DOWN_BIT)) {
		move_cur(s, 1, 0);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_TAB_OFF, KB_TAB_BIT)) {
		move_cur(s, 0, 1);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_HOME_OFF, KB_HOME_BIT)) {
		s->cur_c = 0;
		ensure_visible(s);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_END_OFF, KB_END_BIT)) {
		s->cur_c = SHEET_COLS - 1;
		ensure_visible(s);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_PGUP_OFF, KB_PGUP_BIT)) {
		move_cur(s, -g_grid_rows, 0);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_PGDN_OFF, KB_PGDN_BIT)) {
		move_cur(s, g_grid_rows, 0);
		return ACT_MOVE;
	}
	if (kb_edge(kb, KB_ENTER_OFF, KB_ENTER_BIT)) {
		edit_cell(s, NULL);
		return ACT_FULL;
	}
	if (kb_edge(kb, KB_BKSP_OFF, KB_BKSP_BIT) ||
	    kb_edge(kb, KB_DEL_OFF, KB_DEL_BIT)) {
		clear_cell(s);
		return ACT_FULL;
	}
	if (kb_edge(kb, KB_ESC_OFF, KB_ESC_BIT)) {
		if (s->dirty) {
			set_status("dirty — /q! to quit, /s to save");
			return ACT_BARS;
		}
		return ACT_QUIT;
	}
	return ACT_NONE;
}

static int handle_csi_final(Sheet *s, uint8_t ch)
{
	switch (ch) {
	case 'A': move_cur(s, -1, 0); return ACT_MOVE; /* up */
	case 'B': move_cur(s, 1, 0); return ACT_MOVE;  /* down */
	case 'C': move_cur(s, 0, 1); return ACT_MOVE;  /* right */
	case 'D': move_cur(s, 0, -1); return ACT_MOVE; /* left */
	case 'H':
		s->cur_c = 0;
		ensure_visible(s);
		return ACT_MOVE;
	case 'F':
		s->cur_c = SHEET_COLS - 1;
		ensure_visible(s);
		return ACT_MOVE;
	default:
		return ACT_NONE;
	}
}

static int handle_ascii(Sheet *s, uint8_t ch)
{
	char seed[2];

	/* Escape sequences from terminal/emulator host keyboards. */
	if (g_csi == 0 && ch == 27) {
		g_csi = 1;
		return ACT_NONE;
	}
	if (g_csi == 1) {
		if (ch == '[') {
			g_csi = 2;
			return ACT_NONE;
		}
		g_csi = 0;
		if (ch == 27) {
			/* double Esc → quit path */
			if (s->dirty) {
				set_status("dirty — /q! to quit, /s to save");
				return ACT_BARS;
			}
			return ACT_QUIT;
		}
		/* ESC then something else: ignore prefix, handle char */
	}
	if (g_csi == 2) {
		int act;
		g_csi = 0;
		act = handle_csi_final(s, ch);
		if (act != ACT_NONE)
			return act;
		/* Unknown CSI final — ignore */
		return ACT_NONE;
	}

	switch (ch) {
	case 'h': case 'H': move_cur(s, 0, -1); return ACT_MOVE;
	case 'j': case 'J': move_cur(s, 1, 0); return ACT_MOVE;
	case 'k': case 'K': move_cur(s, -1, 0); return ACT_MOVE;
	case 'l': case 'L': move_cur(s, 0, 1); return ACT_MOVE;
	case '/':
		prompt_command(s);
		return ACT_FULL;
	case '?':
		cmd_help();
		return ACT_BARS;
	case 13:
		edit_cell(s, NULL);
		return ACT_FULL;
	case 8:
	case 127:
		clear_cell(s);
		return ACT_FULL;
	default:
		if (ch >= 32 && ch < 127) {
			seed[0] = (char)ch;
			seed[1] = '\0';
			edit_cell(s, seed);
			return ACT_FULL;
		}
		break;
	}
	return ACT_NONE;
}

static void usage(void)
{
	printf("Usage: sheet [file.csv]\n");
	printf("       sheet -t          run non-interactive self-test\n");
	printf("  Simple spreadsheet. Formulas start with =\n");
	printf("  e.g. =A1+B1  =SUM(A1:A5)  =AVG(B1:B3)\n");
}

/* Non-interactive checks for formula + CSI/hjkl navigation. */
static int run_selftest(void)
{
	Sheet *s = &g_sheet;
	FILE *fp;
	int fails = 0;
	const char *errmsg = NULL;
	double v;

	sheet_init(s);
	g_grid_rows = 20;
	g_grid_cols = 8;

	sheet_set_text(s, 0, 0, "10");
	sheet_set_text(s, 0, 1, "20");
	sheet_set_text(s, 0, 2, "=A1+B1");
	sheet_set_text(s, 1, 0, "=SUM(A1:B1)");
	sheet_recalc(s);

	if (s->cells[0][2].kind != CELL_FORMULA || s->cells[0][2].value != 30) {
		printf("FAIL formula A1+B1 => %d\n",
		       (int)s->cells[0][2].value);
		fails++;
	} else {
		printf("PASS formula A1+B1\n");
	}
	if (s->cells[1][0].kind != CELL_FORMULA || s->cells[1][0].value != 30) {
		printf("FAIL SUM(A1:B1) => %d\n",
		       (int)s->cells[1][0].value);
		fails++;
	} else {
		printf("PASS SUM(A1:B1)\n");
	}

	{
		char dbuf[16];
		sheet_display(&s->cells[0][2], dbuf, sizeof(dbuf));
		if (strcmp(dbuf, "30") != 0) {
			printf("FAIL display got '%s'\n", dbuf);
			fails++;
		} else {
			printf("PASS number display\n");
		}
	}

	s->cur_r = 0;
	s->cur_c = 0;
	handle_ascii(s, 'j');
	handle_ascii(s, 'l');
	if (s->cur_r != 1 || s->cur_c != 1) {
		printf("FAIL hjkl nav at %d,%d\n", s->cur_r, s->cur_c);
		fails++;
	} else {
		printf("PASS hjkl nav\n");
	}

	handle_ascii(s, 27);
	handle_ascii(s, '[');
	handle_ascii(s, 'B'); /* down */
	handle_ascii(s, 27);
	handle_ascii(s, '[');
	handle_ascii(s, 'C'); /* right */
	if (s->cur_r != 2 || s->cur_c != 2) {
		printf("FAIL CSI arrows at %d,%d\n", s->cur_r, s->cur_c);
		fails++;
	} else {
		printf("PASS CSI arrows\n");
	}

	if (!formula_eval(s, 0, 0, "=ABS(-5)", &v, &errmsg) || v != 5) {
		printf("FAIL ABS\n");
		fails++;
	} else {
		printf("PASS ABS\n");
	}

	fp = fopen("sheet_test.txt", "w");
	if (fp) {
		fprintf(fp, "%s\n", fails ? "FAIL" : "PASS");
		fclose(fp);
	}
	printf("%s\n", fails ? "FAIL" : "PASS");
	return fails ? 1 : 0;
}

int main(int argc, char **argv)
{
	Sheet *s = &g_sheet;
	const char *errmsg = NULL;
	int i;

	sheet_init(s);
	g_sv = vdp_vdu_init();
	set_status("? for help");

	if (argc >= 2) {
		if (strcmp(argv[1], "-h") == 0) {
			usage();
			return 0;
		}
		if (strcmp(argv[1], "-t") == 0)
			return run_selftest();
		if (!sheet_load_csv(s, argv[1], &errmsg)) {
			printf("sheet: %s: %s\n", argv[1],
			       errmsg ? errmsg : "load failed");
			return 1;
		}
		set_status("loaded");
	}

	setup_screen();
	layout();
	draw_full(s);

#if SHEET_KEYLOG
	{
		FILE *fp = fopen("sheetkeys.log", "w");
		if (fp) {
			fprintf(fp, "sheet keylog start mode=%u cols=%u rows=%u\n",
				(unsigned)g_sv->scrMode,
				(unsigned)g_sv->scrCols,
				(unsigned)g_sv->scrRows);
			fclose(fp);
		}
	}
#endif

	while (strcmp(g_status, "QUIT") != 0) {
		uint8_t *kb = mos_getkbmap();
		int act = ACT_NONE;
		uint8_t count;
		static uint8_t zero_kb[16];
		bool saw_vkey = false;

		if (!kb)
			kb = zero_kb;

		/*
		 * One physical key must produce one action. Prefer FabGL
		 * vkey/ASCII (incl. CSI arrows); only use kbmap if neither fired.
		 * Otherwise arrows were applied 2–3× and the view shot to the bottom.
		 */
		count = g_sv->vkeycount;
		if (count != g_last_vkeycount) {
			uint8_t ascii = g_sv->keyascii;
			uint8_t vkey = g_sv->vkeycode;
			uint8_t down = g_sv->vkeydown;

			g_last_vkeycount = count;
			saw_vkey = true;
			if (down) {
				if (vkey)
					act = handle_vkey(s, vkey);
				if (act == ACT_NONE && ascii)
					act = handle_ascii(s, ascii);
				keylog_event("vkey", ascii, vkey, down, kb, act);
			} else {
				keylog_event("vup", ascii, vkey, down, kb, ACT_NONE);
			}
		}

		if (!saw_vkey && act == ACT_NONE) {
			act = handle_nav_keys(s, kb);
			if (act != ACT_NONE)
				keylog_event("kbmap", 0, 0, 1, kb, act);
		}

		for (i = 0; i < 16; i++)
			g_prev_kb[i] = kb[i];

		if (act == ACT_QUIT) {
			set_status("QUIT");
			break;
		}
		if (act == ACT_FULL)
			draw_full(s);
		else if (act == ACT_MOVE || act == ACT_BARS)
			draw_move(s);
	}

	restore_screen();
	return 0;
}
