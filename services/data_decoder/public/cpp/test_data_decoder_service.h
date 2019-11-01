// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_DATA_DECODER_SERVICE_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_DATA_DECODER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"

namespace service_manager {
class Connector;
}

namespace data_decoder {

// A class that can be used by tests that need access to a Connector that can
// bind the DataDecoder's interfaces. Bypasses the Service Manager entirely.
class TestDataDecoderService {
 public:
  TestDataDecoderService();
  ~TestDataDecoderService();

  // Returns a connector that can be used to bind DataDecoder's interfaces.
  // The returned connector is valid as long as |this| is valid.
  service_manager::Connector* connector() const { return connector_.get(); }

 private:
  service_manager::TestConnectorFactory connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  DataDecoderService service_;

  DISALLOW_COPY_AND_ASSIGN(TestDataDecoderService);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_DATA_DECODER_SERVICE_H_
