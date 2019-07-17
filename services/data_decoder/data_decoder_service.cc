// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/data_decoder_service.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/data_decoder/bundled_exchanges_parser_factory.h"
#include "services/data_decoder/image_decoder_impl.h"
#include "services/data_decoder/json_parser_impl.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/xml_parser.h"

#ifdef OS_CHROMEOS
#include "services/data_decoder/ble_scan_parser_impl.h"
#endif  // OS_CHROMEOS

namespace data_decoder {

namespace {

constexpr auto kMaxServiceIdleTime = base::TimeDelta::FromSeconds(5);

}  // namespace

DataDecoderService::DataDecoderService()
    : keepalive_(&binding_, kMaxServiceIdleTime) {
  registry_.AddInterface(base::BindRepeating(
      &DataDecoderService::BindImageDecoder, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &DataDecoderService::BindJsonParser, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(&DataDecoderService::BindXmlParser,
                                             base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &DataDecoderService::BindBundledExchangesParserFactory,
      base::Unretained(this)));

#ifdef OS_CHROMEOS
  registry_.AddInterface(base::BindRepeating(
      &DataDecoderService::BindBleScanParser, base::Unretained(this)));
#endif  // OS_CHROMEOS
}

DataDecoderService::DataDecoderService(
    service_manager::mojom::ServiceRequest request)
    : DataDecoderService() {
  BindRequest(std::move(request));
}

DataDecoderService::~DataDecoderService() = default;

void DataDecoderService::BindRequest(
    service_manager::mojom::ServiceRequest request) {
  binding_.Bind(std::move(request));
}

void DataDecoderService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

#ifdef OS_CHROMEOS
void DataDecoderService::BindBleScanParser(
    mojom::BleScanParserRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<BleScanParserImpl>(keepalive_.CreateRef()),
      std::move(request));
}
#endif  // OS_CHROMEOS

void DataDecoderService::BindImageDecoder(mojom::ImageDecoderRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<ImageDecoderImpl>(keepalive_.CreateRef()),
      std::move(request));
}

void DataDecoderService::BindJsonParser(mojom::JsonParserRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<JsonParserImpl>(keepalive_.CreateRef()),
      std::move(request));
}

void DataDecoderService::BindXmlParser(mojom::XmlParserRequest request) {
  mojo::MakeStrongBinding(std::make_unique<XmlParser>(keepalive_.CreateRef()),
                          std::move(request));
}

void DataDecoderService::BindBundledExchangesParserFactory(
    mojom::BundledExchangesParserFactoryRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<BundledExchangesParserFactory>(keepalive_.CreateRef()),
      std::move(request));
}

}  // namespace data_decoder
