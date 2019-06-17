// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_bundled_exchanges_parser.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace data_decoder {

namespace {

// Helper callback which owns a Remote<BundledExchangesParser> until invoked.
void OnMetadataParsed(
    mojo::Remote<mojom::BundledExchangesParser> parser,
    mojom::BundledExchangesParser::ParseMetadataCallback callback,
    mojom::BundleMetadataPtr metadata,
    const base::Optional<std::string>& error_message) {
  std::move(callback).Run(std::move(metadata), error_message);
}

// Helper callback which owns a Remote<BundledExchangesParser> until invoked.
void OnResponseParsed(
    mojo::Remote<mojom::BundledExchangesParser> parser,
    mojom::BundledExchangesParser::ParseResponseCallback callback,
    mojom::BundleResponsePtr response,
    const base::Optional<std::string>& error_message) {
  std::move(callback).Run(std::move(response), error_message);
}

}  // namespace

void ParseBundledExchangesMetadata(
    service_manager::Connector* connector,
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    mojom::BundledExchangesParser::ParseMetadataCallback callback) {
  mojo::Remote<mojom::BundledExchangesParser> parser;
  connector->Connect(mojom::kServiceName, parser.BindNewPipeAndPassReceiver());

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  parser.set_disconnect_handler(
      base::Bind(call_once, nullptr, "connection error"));

  mojom::BundledExchangesParser* raw_parser = parser.get();
  raw_parser->ParseMetadata(std::move(data_source),
                            base::BindOnce(&OnMetadataParsed, std::move(parser),
                                           std::move(call_once)));
}

void ParseBundledExchangesResponse(
    service_manager::Connector* connector,
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    uint64_t response_offset,
    uint64_t response_length,
    mojom::BundledExchangesParser::ParseResponseCallback callback) {
  mojo::Remote<mojom::BundledExchangesParser> parser;
  connector->Connect(mojom::kServiceName, parser.BindNewPipeAndPassReceiver());

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  parser.set_disconnect_handler(
      base::Bind(call_once, nullptr, "connection error"));

  mojom::BundledExchangesParser* raw_parser = parser.get();
  raw_parser->ParseResponse(std::move(data_source), response_offset,
                            response_length,
                            base::BindOnce(&OnResponseParsed, std::move(parser),
                                           std::move(call_once)));
}

}  // namespace data_decoder
