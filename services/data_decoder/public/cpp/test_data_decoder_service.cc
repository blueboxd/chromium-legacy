// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/test_data_decoder_service.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"

namespace data_decoder {

TestDataDecoderService::TestDataDecoderService()
    : connector_(connector_factory_.CreateConnector()),
      service_(connector_factory_.RegisterInstance(
          data_decoder::mojom::kServiceName)) {}

TestDataDecoderService::~TestDataDecoderService() = default;

}  // namespace data_decoder
