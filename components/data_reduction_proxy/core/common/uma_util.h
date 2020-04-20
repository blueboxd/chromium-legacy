// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_UMA_UTIL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_UMA_UTIL_H_

#include "net/base/proxy_server.h"

namespace data_reduction_proxy {

// Scheme of the proxy used.
enum ProxyScheme {
  PROXY_SCHEME_UNKNOWN = 0,
  PROXY_SCHEME_HTTP,
  PROXY_SCHEME_HTTPS,
  PROXY_SCHEME_QUIC,
  PROXY_SCHEME_DIRECT,
  PROXY_SCHEME_MAX
};

// Converts net::ProxyServer::Scheme to type ProxyScheme.
ProxyScheme ConvertNetProxySchemeToProxyScheme(net::ProxyServer::Scheme scheme);


}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_UMA_UTIL_H_
