#!/usr/bin/env bash
# Shared helpers for Agon moslet emulator tests.

norm_text() {
  # Strip CR and trailing blank lines for stable compares
  tr -d '\r' < "$1" | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}'
}

files_equal() {
  local a b
  a=$(norm_text "$1")
  b=$(norm_text "$2")
  [ "$a" = "$b" ]
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: required command not found: $1" >&2
    exit 127
  }
}
