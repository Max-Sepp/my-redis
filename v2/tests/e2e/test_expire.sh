#!/usr/bin/env bash
# e2e test for key expiry (EXPIRE/PEXPIRE/TTL/PTTL/PERSIST/EXPIREAT).
#
# NOTE: this is a spec written against real Redis semantics. EXPIRE/PEXPIRE/
# TTL/PTTL are implemented; PERSIST and EXPIREAT are not (see ROADMAP.md's
# "Key expiry" item), so those sections are expected to fail until they land.
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

# expect_ttl_close <description> <actual_resp> <expected> [<tolerance>]
#
# TTL/PTTL only ever count down, and every send_command round trip in
# lib.sh costs about a second — it blocks on `timeout 1 cat` waiting for
# EOF that a kept-alive connection never sends, so `cat` is only ever
# reaped by the timeout. That means several seconds of real time elapse
# between the EXPIRE/PEXPIRE that sets a TTL and the later TTL/PTTL call
# that reads it back, so asserting an exact value is wrong by design;
# assert a bounded range instead.
expect_ttl_close() {
  local desc="$1" actual="$2" expected="$3" tolerance="${4:-10}"
  local body="${actual%$'\r'}"
  body="${body#:}"
  if [[ "$body" =~ ^-?[0-9]+$ ]] &&
     (( body <= expected && body >= expected - tolerance )); then
    echo "  ok   - $desc"
    PASS=$((PASS + 1))
  else
    echo "  FAIL - $desc"
    printf '         expected: an integer in [%s, %s]\n' \
      "$((expected - tolerance))" "$expected"
    printf '         actual:   %q\n' "$actual"
    FAIL=$((FAIL + 1))
  fi
}

PORT=6397
start_server "$PORT"

# -- TTL/PTTL on a missing key -----------------------------------------------

expect_eq "TTL on a missing key replies :-2" \
  "$(send_command "$PORT" TTL missing-key)" \
  "$(printf ':-2\r\n')"

expect_eq "PTTL on a missing key replies :-2" \
  "$(send_command "$PORT" PTTL missing-key)" \
  "$(printf ':-2\r\n')"

# -- TTL/PTTL on a key with no expiry -----------------------------------------

send_command "$PORT" SET foo bar >/dev/null

expect_eq "TTL on a key with no expiry replies :-1" \
  "$(send_command "$PORT" TTL foo)" \
  "$(printf ':-1\r\n')"

expect_eq "PTTL on a key with no expiry replies :-1" \
  "$(send_command "$PORT" PTTL foo)" \
  "$(printf ':-1\r\n')"

# -- EXPIRE ---------------------------------------------------------------

expect_eq "EXPIRE on a missing key replies :0" \
  "$(send_command "$PORT" EXPIRE missing-key 100)" \
  "$(printf ':0\r\n')"

expect_eq "EXPIRE on an existing key replies :1" \
  "$(send_command "$PORT" EXPIRE foo 100)" \
  "$(printf ':1\r\n')"

expect_ttl_close "TTL reflects the EXPIRE just set" \
  "$(send_command "$PORT" TTL foo)" 100 5

expect_ttl_close "PTTL reflects the EXPIRE just set, in milliseconds" \
  "$(send_command "$PORT" PTTL foo)" 100000 5000

expect_eq "GET still returns the value while the TTL hasn't elapsed" \
  "$(send_command "$PORT" GET foo)" \
  "$(printf '$3\r\nbar\r\n')"

# -- PEXPIRE ------------------------------------------------------------------

expect_eq "PEXPIRE on a missing key replies :0" \
  "$(send_command "$PORT" PEXPIRE missing-key 100000)" \
  "$(printf ':0\r\n')"

expect_eq "PEXPIRE on an existing key replies :1" \
  "$(send_command "$PORT" PEXPIRE foo 50000)" \
  "$(printf ':1\r\n')"

expect_ttl_close "TTL reflects the PEXPIRE just set" \
  "$(send_command "$PORT" TTL foo)" 50 5

# -- PERSIST --------------------------------------------------------------

expect_eq "PERSIST on a missing key replies :0" \
  "$(send_command "$PORT" PERSIST missing-key)" \
  "$(printf ':0\r\n')"

expect_eq "PERSIST removes the TTL and replies :1" \
  "$(send_command "$PORT" PERSIST foo)" \
  "$(printf ':1\r\n')"

expect_eq "PERSIST on a key with no TTL replies :0" \
  "$(send_command "$PORT" PERSIST foo)" \
  "$(printf ':0\r\n')"

expect_eq "TTL is back to -1 after PERSIST" \
  "$(send_command "$PORT" TTL foo)" \
  "$(printf ':-1\r\n')"

# -- EXPIREAT -------------------------------------------------------------

expect_eq "EXPIREAT on a missing key replies :0" \
  "$(send_command "$PORT" EXPIREAT missing-key 9999999999)" \
  "$(printf ':0\r\n')"

expect_eq "EXPIREAT with a far-future timestamp replies :1" \
  "$(send_command "$PORT" EXPIREAT foo 9999999999)" \
  "$(printf ':1\r\n')"

expect_eq "EXPIREAT with a past timestamp deletes the key immediately" \
  "$(send_command "$PORT" EXPIREAT foo 1)" \
  "$(printf ':1\r\n')"

expect_eq "the key is gone right after an EXPIREAT in the past" \
  "$(send_command "$PORT" GET foo)" \
  "$(printf '$-1\r\n')"

expect_eq "TTL confirms the key is gone" \
  "$(send_command "$PORT" TTL foo)" \
  "$(printf ':-2\r\n')"

# -- a negative EXPIRE deletes the key immediately -------------------------

send_command "$PORT" SET baz qux >/dev/null

expect_eq "EXPIRE with a negative TTL replies :1" \
  "$(send_command "$PORT" EXPIRE baz -1)" \
  "$(printf ':1\r\n')"

expect_eq "the key is gone right after a negative EXPIRE" \
  "$(send_command "$PORT" GET baz)" \
  "$(printf '$-1\r\n')"

# -- SET clears an existing TTL --------------------------------------------

send_command "$PORT" SET reset-me val1 >/dev/null
send_command "$PORT" EXPIRE reset-me 100 >/dev/null
send_command "$PORT" SET reset-me val2 >/dev/null

expect_eq "a plain SET clears a previously-set TTL" \
  "$(send_command "$PORT" TTL reset-me)" \
  "$(printf ':-1\r\n')"

# -- a short-lived key actually expires ------------------------------------
#
# The PEXPIRE here needs to outlast a send_command round trip (~1s, see
# expect_ttl_close above) so the "still there" check below — itself one
# more round trip after the PEXPIRE — reliably lands before expiry.

send_command "$PORT" SET soon-gone val >/dev/null
send_command "$PORT" PEXPIRE soon-gone 3000 >/dev/null

expect_eq "the key is still there just after PEXPIRE" \
  "$(send_command "$PORT" GET soon-gone)" \
  "$(printf '$3\r\nval\r\n')"

sleep 2

expect_eq "the key is gone once its PEXPIRE has elapsed" \
  "$(send_command "$PORT" GET soon-gone)" \
  "$(printf '$-1\r\n')"

expect_eq "TTL confirms the elapsed key is gone" \
  "$(send_command "$PORT" TTL soon-gone)" \
  "$(printf ':-2\r\n')"

# -- malformed commands are unrecognised -----------------------------------

expect_eq "EXPIRE with a non-integer seconds arg is unrecognised" \
  "$(send_command "$PORT" EXPIRE foo not-a-number)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

expect_eq "EXPIRE with an empty key is unrecognised" \
  "$(send_command "$PORT" EXPIRE "" 100)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

expect_eq "TTL with an empty key is unrecognised" \
  "$(send_command "$PORT" TTL "")" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
