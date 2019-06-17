// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_BUNDLED_EXCHANGES_PARSER_H_
#define SERVICES_DATA_DECODER_BUNDLED_EXCHANGES_PARSER_H_

#include <memory>

#include "base/macros.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace data_decoder {

class BundledExchangesParser : public mojom::BundledExchangesParser {
 public:
  explicit BundledExchangesParser(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~BundledExchangesParser() override;

 private:
  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  // mojom::BundledExchangesParser implementation.
  void ParseMetadata(mojo::PendingRemote<mojom::BundleDataSource> data_source,
                     ParseMetadataCallback callback) override;
  void ParseResponse(mojo::PendingRemote<mojom::BundleDataSource> data_source,
                     uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesParser);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_BUNDLED_EXCHANGES_PARSER_H_
