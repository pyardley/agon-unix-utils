# sheet — CSV editor & simple spreadsheet for Agon

`sheet` is a moslet for editing CSV files on Agon Light / Agon MOS and doing
light spreadsheet work: totals, averages, and other small formulas without
leaving the console.

It is meant for budgets, inventories, score tables, and similar flat data —
not as a full Excel/LibreOffice replacement.

```
sheet                 # empty workbook
sheet demo.csv        # open a CSV
sheet -t              # non-interactive self-test
```

On start it switches to **MODE 0** (640×480, 16 colours) and restores the
previous mode on quit.

---

## Screen layout

| Area | Contents |
|------|----------|
| Top line | Current cell (`A1`), cell text/formula, filename (`*` if dirty), status |
| Next line | Column letters (A–P); current column marked `[A]` |
| Grid | Row numbers with `>` on the current row; cells; selection as `[value]` in green |
| Bottom line | Key hints |

Only part of the sheet fits on screen. Moving the cursor past the edge scrolls
the viewport.

---

## Moving around

| Keys | Action |
|------|--------|
| Arrow keys | Move one cell |
| `h` `j` `k` `l` | Left / down / up / right (vi-style; handy if arrows misbehave) |
| Tab | Move right |
| Home / End | First / last column |
| PgUp / PgDn | Page up / down by the visible height |
| `/g A1` | Jump to a cell |

---

## Editing cells

| Keys | Action |
|------|--------|
| Enter | Edit the current cell (MOS line editor) |
| Type a character | Start editing with that character as the seed |
| Backspace | Clear the current cell |

- Plain text is stored as text.
- A value that looks like a number is stored as a number.
- Text starting with `=` is a formula and is recalculated when the sheet changes.

Cell text is limited to **32 characters**.

---

## Commands

Press `/` then a command (shown on the top line):

| Command | Meaning |
|---------|---------|
| `/s` or `/s file.csv` | Save (uses current name if omitted) |
| `/w` | Same as `/s` |
| `/l file.csv` | Load a CSV (replaces the sheet) |
| `/q` | Quit (refuses if dirty) |
| `/q!` | Quit without saving |
| `/c` | Clear the whole sheet |
| `/g B12` | Go to cell |
| `/h` or `?` | Short command reminder |

`Esc` also quits when the sheet is clean; if dirty, use `/q!` or save first.

---

## CSV files

`sheet` reads and writes ordinary CSV:

- One row per line, fields separated by commas.
- Fields with commas, quotes, or newlines are double-quoted; quotes inside a
  field are doubled (`""`).
- Formulas are saved with their leading `=` so they round-trip.

Example:

```csv
Item,Qty,Price,Total
Widgets,3,1.5,=B2*C2
Gadgets,2,4,=B3*C3
,,Subtotal,=SUM(D2:D3)
```

Empty trailing rows are not required; save writes only up to the last used
cell. Load skips blank lines.

**Tip:** Keep a copy of important CSVs before heavy edits. There is no undo.

---

## Formulas

Formulas begin with `=`. Supported forms:

| Example | Meaning |
|---------|---------|
| `=A1+B2` | Arithmetic (`+ - * /`), parentheses |
| `=A1:A5` | Sum of a rectangle (bare range) |
| `=SUM(A1:B2)` | Sum |
| `=AVG(B1:B10)` | Average |
| `=MIN(...)` / `=MAX(...)` | Min / max |
| `=COUNT(A1:A10)` | Count of numeric cells |
| `=ABS(-3)` / `=SQRT(A1)` | Absolute value / square root |

Failed formulas show `#ERR` in the grid. The top line still shows the formula
text for the current cell.

Recalc runs automatically after edits. Forward references are resolved over a
small number of passes; circular refs fail.

---

## Grid size

| Limit | Value |
|-------|--------|
| Columns | A–P (16) |
| Rows | 1–80 |
| Cell text | 32 characters |
| Filename | 64 characters |

Widen or lengthen the model in `sheet/src/config.h` (watch Agon BSS size if
you grow it a lot).

---

## Limitations

- **No insert/delete** of rows or columns. Shift data by hand; formula text is
  **not** rewritten when cells move.
- **No undo / redo.**
- **No multiple sheets**, charts, formatting, or merged cells.
- **No sorting, filtering, or find/replace.**
- **Fixed column width** on screen; long text is clipped in the grid (full
  text is on the top line when the cell is selected).
- **Numbers** use a small custom formatter (AgDev has no `%f`/`%g` in
  nanoprintf); very large or tiny values are clamped/simplified.
- **One file at a time**; `/l` replaces the in-memory sheet.
- Built for Agon MOS (AgDev). Behaviour depends on VDP text mode and the
  keyboard path (arrows, CSI, or `hjkl`).

---

## Possible future enhancements

- Insert / delete rows and columns, with formula reference adjustment.
- Undo of the last edit (or a short undo stack).
- Copy / paste and fill-down.
- Find (and maybe replace) in the sheet.
- Larger grid or paged “virtual” storage if RAM allows.
- TSV / semicolon CSV and configurable delimiter.
- Frozen header row while scrolling.
- More functions (`IF`, `ROUND`, `LEN`, …) via `fn_table[]` in
  `sheet/src/formula.c`.
- Export a values-only CSV (formulas replaced by results).
- Optional row/column labels from the first header line.

---

## Building

From the repo root (AgDev on `PATH`):

```sh
make -C sheet
# installs beside other moslets when you use the top-level Makefile
make
```

Binaries: `sheet/bin/sheet.bin`, also copied to `bin/` and `mos/` by the
project Makefile.
