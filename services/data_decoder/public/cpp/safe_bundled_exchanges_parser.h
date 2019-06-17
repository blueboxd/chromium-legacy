// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_BUNDLED_EXCHANGES_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_BUNDLED_EXCHANGES_PARSER_H_

#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"

namespace service_manager {
class Connector;
}

namespace data_decoder {

// Parses bundle's metadata from |data_source| via the data_decoder service.
void ParseBundledExchangesMetadata(
    service_manager::Connector* connector,
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    mojom::BundledExchangesParser::ParseMetadataCallback callback);

// Parses a response from |data_source| that starts at |response_offset| and
// has length |response_length|, via the data_decoder service.
void ParseBundledExchangesResponse(
    service_manager::Connector* connector,
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    uint64_t response_offset,
    uint64_t response_length,
    mojom::BundledExchangesParser::ParseResponseCallback callback);

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_BUNDLED_EXCHANGES_PARSER_H_
