#!/usr/bin/env bash
# e2e test for EchoRequestHandler (server/handler/echo_request_handler.h).
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6391
start_server "$PORT"

expect_eq "ECHO returns the given message" \
  "$(send_command "$PORT" ECHO "hello world")" \
  "$(printf '$11\r\nhello world\r\n')"

expect_eq "ECHO with an empty string round-trips it" \
  "$(send_command "$PORT" ECHO "")" \
  "$(printf '$0\r\n\r\n')"

expect_eq "ECHO with no args is unrecognised" \
  "$(send_command "$PORT" ECHO)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

expect_eq "ECHO with too many args is unrecognised" \
  "$(send_command "$PORT" ECHO one two)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
