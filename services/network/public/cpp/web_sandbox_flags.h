// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEB_SANDBOX_FLAGS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEB_SANDBOX_FLAGS_H_

#include <cstdint>

namespace network {
namespace mojom {

enum class WebSandboxFlags : int32_t;

inline constexpr WebSandboxFlags operator&(WebSandboxFlags a,
                                           WebSandboxFlags b) {
  return static_cast<WebSandboxFlags>(static_cast<int>(a) &
                                      static_cast<int>(b));
}

inline constexpr WebSandboxFlags operator|(WebSandboxFlags a,
                                           WebSandboxFlags b) {
  return static_cast<WebSandboxFlags>(static_cast<int>(a) |
                                      static_cast<int>(b));
}

inline WebSandboxFlags& operator|=(WebSandboxFlags& a, WebSandboxFlags b) {
  return a = a | b;
}

inline constexpr WebSandboxFlags operator~(WebSandboxFlags flags) {
  return static_cast<WebSandboxFlags>(~static_cast<int>(flags));
}

}  // namespace mojom
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SANDBOX_FLAGS_H_
