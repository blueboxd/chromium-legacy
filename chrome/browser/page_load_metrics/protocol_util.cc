// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/protocol_util.h"

namespace page_load_metrics {

NetworkProtocol GetNetworkProtocol(
    net::HttpResponseInfo::ConnectionInfo connection_info) {
  switch (connection_info) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return NetworkProtocol::kOther;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      return NetworkProtocol::kHttp11;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      return NetworkProtocol::kHttp2;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_47:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_48:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_999:
      return NetworkProtocol::kQuic;
  }
}

}  // namespace page_load_metrics
