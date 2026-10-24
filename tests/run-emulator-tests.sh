#!/usr/bin/env bash
# Run moslet tests inside Fab Agon Emulator (CLI) using an isolated SD card.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TESTS="$ROOT/tests"
# shellcheck source=tools/lib.sh
source "$TESTS/tools/lib.sh"

# Flatpak fab-agon-emulator only grants filesystem access to ~/.agon-sdcard,
# so the isolated test card must live under that path.
SDCARD="${AGON_TEST_SDCARD:-$HOME/.agon-sdcard/_test_agon_unix_utils}"
EMULATOR_TIMEOUT="${EMULATOR_TIMEOUT:-12}"
FLATPAK_APP="${FLATPAK_APP:-io.github.tomm.fab_agon_emulator}"
CASES_FILE="${CASES_FILE:-$TESTS/cases.conf}"

require_cmd flatpak
require_cmd timeout

if ! flatpak --user info "$FLATPAK_APP" >/dev/null 2>&1 \
   && ! flatpak info "$FLATPAK_APP" >/dev/null 2>&1; then
  echo "error: Flatpak app not installed: $FLATPAK_APP" >&2
  exit 127
fi

prepare_sdcard() {
  rm -rf "$SDCARD"
  mkdir -p "$SDCARD/mos" "$SDCARD/fixtures" "$SDCARD/out"
  cp -f "$ROOT"/mos/*.bin "$SDCARD/mos/"
  cp -a "$TESTS/fixtures/." "$SDCARD/fixtures/"
  # Pre-seed tee -a destination
  cp -f "$TESTS/fixtures/tee_append.txt" "$SDCARD/out/tee_app.txt"
}

run_emulator() {
  local logfile="$1"
  # stdin must be /dev/null: otherwise the emulator reads the cases file
  # (the while-read loop's stdin) as MOS keyboard input.
  timeout "$EMULATOR_TIMEOUT" flatpak run --command=agon-cli-emulator "$FLATPAK_APP" \
    --sdcard "$SDCARD" -u < /dev/null >"$logfile" 2>&1 || true
}

pass=0
fail=0
skip=0
results=()

run_case() {
  local name="$1" cmd="$2" outfile="$3" expected="$4" mode="$5"
  local autoexec="$SDCARD/autoexec.txt"
  local log="$SDCARD/out/_emu_${name}.log"
  local outpath="$SDCARD/$outfile"
  local exp_path="$TESTS/expected/$expected"

  mkdir -p "$(dirname "$outpath")"
  rm -f "$outpath"

  # Special setup for tee_append: restore seed each time
  if [ "$name" = "tee_append" ]; then
    cp -f "$TESTS/fixtures/tee_append.txt" "$SDCARD/out/tee_app.txt"
  fi

  # Redirect stdout of the utility into outfile when command doesn't already
  # contain a redirect. For commands that write their own dest (touch, tee dest
  # checks), still capture stdout harmlessly.
  case "$mode" in
    exists)
      printf '%s\n' "$cmd" >"$autoexec"
      ;;
    *)
      if [[ "$cmd" == *">"* ]] || [[ " $cmd " == *" $outfile "* ]]; then
        # Command already redirects, or OUTFILE is a path argument (e.g. tee dest).
        printf '%s\n' "$cmd" >"$autoexec"
      else
        printf '%s > %s\n' "$cmd" "$outfile" >"$autoexec"
      fi
      ;;
  esac

  run_emulator "$log"
  rm -f "$autoexec"

  local ok=0
  case "$mode" in
    file)
      if [ ! -f "$outpath" ]; then
        results+=("FAIL $name (missing output)")
      elif files_equal "$outpath" "$exp_path"; then
        ok=1
      else
        results+=("FAIL $name (output mismatch)")
        results+=("  --- expected ---")
        results+=("$(norm_text "$exp_path" | sed 's/^/  /')")
        results+=("  --- got ---")
        results+=("$(norm_text "$outpath" | sed 's/^/  /')")
      fi
      ;;
    empty)
      if [ ! -f "$outpath" ] || [ -z "$(norm_text "$outpath")" ]; then
        ok=1
      else
        results+=("FAIL $name (expected empty)")
        results+=("  got: $(norm_text "$outpath")")
      fi
      ;;
    exists)
      if [ -e "$outpath" ]; then
        ok=1
      else
        results+=("FAIL $name (path not created: $outfile)")
      fi
      ;;
    regex)
      if [ ! -f "$outpath" ]; then
        results+=("FAIL $name (missing output)")
      elif norm_text "$outpath" | grep -Eq "$expected"; then
        ok=1
      else
        results+=("FAIL $name (regex /$expected/)")
        results+=("  got:")
        results+=("$(norm_text "$outpath" | sed 's/^/  /')")
      fi
      ;;
    du_s)
      if [ ! -f "$outpath" ]; then
        results+=("FAIL $name (missing output)")
      else
        local size
        size=$(norm_text "$outpath" | awk 'NF{print $1; exit}')
        if [ "$size" = "$expected" ]; then
          ok=1
        else
          results+=("FAIL $name (size $size != $expected)")
          results+=("  got: $(norm_text "$outpath")")
        fi
      fi
      ;;
    *)
      results+=("FAIL $name (unknown mode $mode)")
      ;;
  esac

  if [ "$ok" -eq 1 ]; then
    pass=$((pass + 1))
    results+=("PASS $name")
  else
    fail=$((fail + 1))
    # Keep emulator log for failures
    if [ -f "$log" ]; then
      results+=("  emu log: $log")
    fi
  fi
}

echo "== agon-unix-utils emulator tests =="
echo "SD card: $SDCARD"
prepare_sdcard

while IFS=$'\t' read -r name cmd outfile expected mode || [ -n "${name:-}" ]; do
  # Skip blanks/comments
  [[ -z "${name:-}" || "$name" =~ ^# ]] && continue
  run_case "$name" "$cmd" "$outfile" "$expected" "$mode"
done < <(grep -v '^[[:space:]]*$' "$CASES_FILE" | grep -v '^#')

echo
printf '%s\n' "${results[@]}"
echo
echo "Summary: $pass passed, $fail failed, $skip skipped"
if [ "$fail" -ne 0 ]; then
  exit 1
fi
exit 0
