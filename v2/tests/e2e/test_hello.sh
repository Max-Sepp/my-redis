#!/usr/bin/env bash
# e2e test for HelloRequestHandler (server/handler/hello_request_handler.h).
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6392
start_server "$PORT"

# Only RESP2 is implemented, so HELLO (with no protover, which defaults to
# the connection's current protocol) and the explicit "HELLO 2" both reply
# with the same RESP2 server-info array.
hello_reply="$(printf '*12\r\n$6\r\nserver\r\n$7\r\nmyredis\r\n$7\r\nversion\r\n$5\r\n0.1.0\r\n$5\r\nproto\r\n:2\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n')"

expect_eq "HELLO with no args replies with server info" \
  "$(send_command "$PORT" HELLO)" \
  "$hello_reply"

expect_eq "HELLO 2 replies with the same server info" \
  "$(send_command "$PORT" HELLO 2)" \
  "$hello_reply"

expect_eq "HELLO 3 is rejected (RESP3 unsupported)" \
  "$(send_command "$PORT" HELLO 3)" \
  "$(printf -- '-NOPROTO unsupported protocol version\r\n')"

summary
