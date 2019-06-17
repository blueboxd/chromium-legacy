// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/bundled_exchanges_parser.h"

#include <utility>

#include "base/callback.h"

namespace data_decoder {

BundledExchangesParser::BundledExchangesParser(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

BundledExchangesParser::~BundledExchangesParser() = default;

void BundledExchangesParser::ParseMetadata(
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    ParseMetadataCallback callback) {
  std::move(callback).Run(nullptr /* metadata */, "Not implemented");
}

void BundledExchangesParser::ParseResponse(
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    uint64_t response_offset,
    uint64_t response_length,
    ParseResponseCallback callback) {
  std::move(callback).Run(nullptr /* response */, "Not implemented");
}

}  // namespace data_decoder
