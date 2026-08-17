#!/usr/bin/env bash
# e2e test for SetRequestHandler (server/handler/set_request_handler.h).
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6393
start_server "$PORT"

expect_eq "SET key value replies OK" \
  "$(send_command "$PORT" SET foo bar)" \
  "$(printf '+OK\r\n')"

expect_eq "SET overwrites an existing key" \
  "$(send_command "$PORT" SET foo baz)" \
  "$(printf '+OK\r\n')"

expect_eq "SET with a missing value is unrecognised" \
  "$(send_command "$PORT" SET foo)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

expect_eq "SET with an empty key is unrecognised" \
  "$(send_command "$PORT" SET "" bar)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
