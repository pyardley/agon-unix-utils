# agon-unix-utils

Unix-style command-line utilities for the [Agon Light](https://www.thebyteattic.com/p/agon.html) / Agon Light 2 / Console8 (MOS moslets).

This repository contains **AgDev**-built moslets authored here, plus notes on other utilities commonly installed on the same SD card from the Agon community.

Prebuilt binaries from this repo: `mos/*.bin` (load address `0x0B0000`). Copy them into the SD card `mos/` folder.

```bash
make
make install   # copies to ~/.agon-sdcard/mos/ when using Fab Agon Emulator
```

---

## Utilities in this repository

| Command | Description | Example |
|--------|-------------|---------|
| **cut** | Extract fields from each line (`-d`, `-f`; ranges in field lists are comma-separated numbers) | `cut -d, -f1,3 data.csv` |
| **uniq** | Collapse adjacent duplicate lines (`-c` / `-d` / `-u`) | `uniq -c names.txt` |
| **touch** | Create an empty file, or open/close an existing one | `touch new.txt` |
| **tee** | Copy a file to stdout and to another file (`-a` append) | `tee out.txt in.txt` |
| **tr** | Translate or delete characters (`-d`; supports `a-z` style ranges) | `tr a-z A-Z file.txt` |
| **diff** | Line-oriented compare (`-q` quiet) | `diff a.txt b.txt` |
| **du** | Disk usage of files/dirs in bytes (`-s` summarise) | `du -s games` |
| **df** | Report bytes *used* on the volume (free space N/A via MOS API) | `df` |
| **sheet** | Simple spreadsheet (formulas, CSV load/save) | `sheet budget.csv` |

Build requires [AgDev](https://github.com/pcawte/AgDev) (`cedev-config` / `ez80-clang` on `PATH`).

### sheet notes

- Grid defaults: 16 columns (A–P) × 80 rows (`sheet/src/config.h`); viewport scrolls.
- On start, switches to **MODE 0** (640×480, 16 colours / ~80×60 text) and restores the previous mode on quit.
- Formulas start with `=`: `=A1+B2`, `=A1:A5` (sum), `=SUM(A1:B2)`, `=AVG` / `MIN` / `MAX` / `COUNT` / `ABS` / `SQRT`.
- Keys: arrows or `hjkl` move, Enter edit, `/` commands (`/s` save, `/l` load, `/w` write, `/q` quit, `/c` clear, `/g A1` goto), `?` help.
- Extend formulas by adding a `FnEntry` in `sheet/src/formula.c`.

---

## Other utilities installed on this machine’s SD card

These are **not** built from this repo’s source; they come from community projects. Links go to the upstream repositories.

### From [vascocosta/agon-utils](https://github.com/vascocosta/agon-utils)

| Command | Description |
|--------|-------------|
| **head** | Print the first *n* lines of a file (`-n`) |
| **tail** | Print the last *n* lines of a file (`-n`) |
| **vgrep** | Minimal grep (`-i`, single file) |
| *(also upstream)* | `echo`, `strings`, `wc` — available from the same repo if you install them |

Prebuilt copies of `head` / `tail` used here: `third_party/vascocosta/`.

### From [lennart-benschop/agon-utilities](https://github.com/lennart-benschop/agon-utilities)

| Command | Description |
|--------|-------------|
| **grep** | Search text files (`-i`, wildcards, multi-file) |
| **more** | Page through a text file |
| **wc** | Count lines / words / characters |
| **cal** | Calendar |
| **comp** | Byte-compare two files |
| **concat** | Concatenate files |
| **find** | Find files by name pattern (whole volume) |
| **memfill** | Fill a RAM range |
| **font** / **fontctl** | Bitmap font load / select |
| **nano** | Nano-style editor (moslet) |
| **loadfont** / **recode** / **sort** / **ne** / **mc** | In `/bin/` — fonts, encoding, sort, editors, Midnight Commander–style UI |

### From [tomm/popup-mos](https://github.com/tomm/popup-mos) (and linked projects)

Batteries-included SD template; notable extras often installed from it:

| Command | Description | Upstream |
|--------|-------------|----------|
| **unzip** / **gunzip** | Extract ZIP / gzip | [sijnstra/agon-projects](https://github.com/sijnstra/agon-projects) |
| **vi** | BusyBox vi | [tomm/toms-agon-experiments](https://github.com/tomm/toms-agon-experiments) |
| **ez80asm** / **zap** | On-device assemblers | [envenomator/agon-ez80asm](https://github.com/envenomator/agon-ez80asm), [avalonbits/zap](https://github.com/avalonbits/zap) |
| **flash** | Flash MOS/VDP firmware | [AgonPlatform/agon-flash](https://github.com/AgonPlatform/agon-flash) |
| **hexload** / **hexdump** / **memdump** / **strings** | Serial load & inspection | envenomator / sijinstra |
| **crc32** | Checksums | [envenomator/agon-crc32](https://github.com/envenomator/agon-crc32) |
| **calc24** / **bootlogo** / **playmod** / **matrix** | Calculator, splash, MOD player, demo | sijinstra / HeathenUK |
| **bbcbasic** / **bbcbasic24** / **basic** | BBC BASIC interpreters | [breakintoprogram](https://github.com/breakintoprogram/agon-bbc-basic), [envenomator/agon-bbc-basic](https://github.com/envenomator/agon-bbc-basic) |
| **acc** / **aed** | On-device C compiler / advanced editor | [avalonbits/acc](https://github.com/avalonbits/acc), [avalonbits/aed](https://github.com/avalonbits/aed) |

Games pack (separate folder): also from [popup-mos](https://github.com/tomm/popup-mos) `games/`.

### Built into MOS (no moslet needed)

Examples: `CAT` / `DIR`, `TYPE`, `COPY`, `DELETE` / `RM`, `MkDir`, `CD`, `LOAD`, `RUN`, `HELP`, `RENAME`, …

---

## Build / install / test

```bash
export PATH="$HOME/development/tools/agdev/bin:$PATH"
make            # build all moslets into mos/ and bin/
make install    # copy to ~/.agon-sdcard/mos/ (+ Vasco head/tail if present)
make test       # emulator suite (Fab Agon Flatpak required)
```

The test runner (`tests/run-emulator-tests.sh`) boots `agon-cli-emulator` once per case on an isolated card at `~/.agon-sdcard/_test_agon_unix_utils`, with fixtures in `tests/fixtures/` and golden files in `tests/expected/`. Details: `tests/README.md`.

---

## Notes for Agon

- MOS does **not** support Unix pipes (`|`). Tools take **file arguments** (and optional `>` redirection).
- Prefer attached options where commas matter, e.g. `cut -d, -f1,3`, `tr a-z A-Z`.
- `diff` exits with status 1 when files differ (can stop `autoexec.txt` / `*Exec` scripts).
- `df` reports **used** bytes only; free space is not exposed through the AgDev MOS API used here.

## License

MIT (for code in this repository). Third-party binaries remain under their upstream licenses.
