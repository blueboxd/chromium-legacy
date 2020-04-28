// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/uma_util.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace data_reduction_proxy {

ProxyScheme ConvertNetProxySchemeToProxyScheme(
    net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return PROXY_SCHEME_HTTP;
    case net::ProxyServer::SCHEME_HTTPS:
      return PROXY_SCHEME_HTTPS;
    case net::ProxyServer::SCHEME_QUIC:
      return PROXY_SCHEME_QUIC;
    case net::ProxyServer::SCHEME_DIRECT:
      return PROXY_SCHEME_DIRECT;
    default:
      NOTREACHED() << scheme;
      return PROXY_SCHEME_UNKNOWN;
  }
}


}  // namespace data_reduction_proxy
