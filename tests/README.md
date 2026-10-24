# Test suite

Emulator-based tests for every moslet built in this repository.

## Requirements

- Fab Agon Emulator Flatpak: `io.github.tomm.fab_agon_emulator`
- `timeout` (coreutils)
- Built binaries in `../mos/*.bin` (`make` from repo root)

## Run

```bash
make test
# or:
./tests/run-emulator-tests.sh
```

Each case boots the CLI emulator once with an isolated SD card (fixtures + moslets only), runs one MOS command via `autoexec.txt`, then compares output to `tests/expected/`.

The test card is created at `~/.agon-sdcard/_test_agon_unix_utils` because the Flatpak only allows access under `~/.agon-sdcard`. Override with `AGON_TEST_SDCARD=/path` if needed (must be visible to the Flatpak).

## Layout

| Path | Purpose |
|------|---------|
| `fixtures/` | Input files |
| `expected/` | Golden outputs |
| `cases.conf` | Case table (name, command, outfile, expected, mode) |
| `run-emulator-tests.sh` | Runner |
| `tools/lib.sh` | Compare helpers |
| `sdcard/` | Ephemeral test SD image (gitignored) |

## Adding a case

1. Add fixtures under `fixtures/` if needed.
2. Add expected output under `expected/`.
3. Append a line to `cases.conf` (see comments in that file for modes).
