#!/usr/bin/env bash
# e2e test for UnknownRequestHandler (server/handler/unknown_request_handler.h),
# the fallback that catches anything the other handlers rejected.
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6396
start_server "$PORT"

expect_eq "an unrecognised command name is rejected" \
  "$(send_command "$PORT" FROBNICATE foo)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

expect_eq "command names are case-sensitive" \
  "$(send_command "$PORT" get foo)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
