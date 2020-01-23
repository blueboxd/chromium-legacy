// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_utils_chromium.h"

#include "base/containers/adapters.h"
#include "base/strings/string_split.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace net {

quic::QuicTagVector ParseQuicConnectionOptions(
    const std::string& connection_options) {
  quic::QuicTagVector options;
  // Tokens are expected to be no more than 4 characters long, but
  // handle overflow gracefully.
  for (const quiche::QuicheStringPiece& token :
       base::SplitStringPiece(connection_options, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    uint32_t option = 0;
    for (char token_char : base::Reversed(token)) {
      option <<= 8;
      option |= static_cast<unsigned char>(token_char);
    }
    options.push_back(option);
  }
  return options;
}

quic::ParsedQuicVersionVector ParseQuicVersions(
    const std::string& quic_versions) {
  quic::ParsedQuicVersionVector supported_versions;
  quic::QuicTransportVersionVector all_supported_versions =
      quic::AllSupportedTransportVersions();

  for (const base::StringPiece& version : base::SplitStringPiece(
           quic_versions, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    auto it = all_supported_versions.begin();
    while (it != all_supported_versions.end()) {
      if (quic::QuicVersionToString(*it) == version) {
        supported_versions.push_back(
            quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO, *it));
        // Remove the supported version to deduplicate versions extracted from
        // |quic_versions|.
        all_supported_versions.erase(it);
        break;
      }
      it++;
    }
    for (const auto& supported_version : quic::AllSupportedVersions()) {
      if (quic::AlpnForVersion(supported_version) == version) {
        supported_versions.push_back(supported_version);
        break;
      }
    }
  }
  return supported_versions;
}

}  // namespace net
