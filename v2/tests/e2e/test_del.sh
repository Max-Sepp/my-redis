#!/usr/bin/env bash
# e2e test for DelRequestHandler (server/handler/del_request_handler.h).
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6395
start_server "$PORT"

send_command "$PORT" SET foo bar >/dev/null

expect_eq "DEL on an existing key replies :1" \
  "$(send_command "$PORT" DEL foo)" \
  "$(printf ':1\r\n')"

expect_eq "the key is actually gone after DEL" \
  "$(send_command "$PORT" GET foo)" \
  "$(printf '$-1\r\n')"

expect_eq "DEL with an empty key is unrecognised" \
  "$(send_command "$PORT" DEL "")" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
