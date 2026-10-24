# agon-unix-utils

Minimal Unix-style **cut** and **uniq** moslets for the [Agon Light](https://www.thebyteattic.com/p/agon.html) / Console8 (MOS).

Built with [AgDev](https://github.com/pcawte/AgDev). Binaries are moslets (load address `0x0B0000`) — copy `mos/*.bin` to the SD card `mos/` folder.

Prebuilt binaries for convenience: `mos/cut.bin`, `mos/uniq.bin`.

## cut

Extract fields from each line of a text file.

```text
cut -d, -f2 data.csv
cut -d, -f1,3 data.csv
cut -d : -f1 passwd.txt
```

| Option | Meaning |
|--------|---------|
| `-d delim` / `-dX` | Field delimiter (default TAB). Prefer `-d,` so a bare comma is not a separate token. |
| `-f list` / `-f1,3` | Comma-separated **1-based** field numbers |
| `-h` | Help |

No `-c` (characters) and no ranges (`1-3`) in this minimal port.

## uniq

Collapse **adjacent** duplicate lines (same as classic `uniq`). Sort first for global uniqueness (`sort` from Lennart’s utilities).

```text
uniq names.txt
uniq -c names.txt
uniq -d names.txt
uniq -u names.txt
```

| Option | Meaning |
|--------|---------|
| `-c` | Prefix count of occurrences |
| `-d` | Only duplicated lines |
| `-u` | Only lines that appear once |
| `-h` | Help |

## Build

```bash
export PATH="$HOME/development/tools/agdev/bin:$PATH"
make
```

## Install

```bash
make install
# or: cp mos/*.bin ~/.agon-sdcard/mos/
```

## Test

Sample inputs are in `tests/`. On this machine they were verified with Fab Agon Emulator’s CLI (`agon-cli-emulator`) via `autoexec.txt` redirects — all of:

- `cut -d, -f2`
- `cut -d, -f1,3`
- `uniq` / `uniq -c` / `uniq -d` / `uniq -u`

passed against `tests/cut_in.txt` and `tests/uniq_in.txt`.

## License

MIT
