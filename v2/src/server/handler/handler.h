#ifndef MYREDIS_SERVER_HANDLER_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_HANDLER_H_

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"

namespace myredis {

// Handles whatever request it is given; the dispatcher's reply for requests no
// RequestHandler takes.
//
// A handler encodes its response into `out` rather than writing to a socket or
// building a RespValue. Command execution is single-threaded, so handlers run
// without locking; `out` streams the bytes back to the owning IO thread.
template <typename H>
concept FallbackHandler =
    requires(H& handler, const RespValue& request, RespWriter& out) {
      handler.Handle(request, out);
    };

// Executes one command. `kName` is the command keyword the dispatcher routes
// to this handler at compile time; IsHandler then says whether the handler
// accepts the request (e.g. its argument count/shape), and if so Handle
// produces the response.
template <typename H>
concept RequestHandler =
    FallbackHandler<H> && requires(const H& handler, const RespValue& request) {
      { H::kName } -> std::convertible_to<std::string_view>;
      { handler.IsHandler(request) } -> std::same_as<bool>;
    };

// A string usable as a template argument, so a handler parameterised on its
// command keyword (e.g. EXPIRE vs PEXPIRE) can expose it as a constant kName.
template <std::size_t N>
struct FixedString {
  // NOLINTNEXTLINE(google-explicit-constructor,modernize-avoid-c-arrays)
  consteval FixedString(const char (&str)[N]) {
    std::copy_n(str, N, chars.begin());
  }

  [[nodiscard]] constexpr std::string_view View() const {
    return {chars.data(), N - 1};
  }

  std::array<char, N> chars{};
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_HANDLER_H_
