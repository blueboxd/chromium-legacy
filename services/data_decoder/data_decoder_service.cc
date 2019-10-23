// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/data_decoder_service.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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
    : keepalive_(&binding_, kMaxServiceIdleTime) {}

DataDecoderService::DataDecoderService(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : DataDecoderService() {
  binding_.Bind(std::move(receiver));
}

DataDecoderService::DataDecoderService(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver)
    : DataDecoderService() {
  receiver_.Bind(std::move(receiver));
}

DataDecoderService::~DataDecoderService() = default;

void DataDecoderService::BindReceiver(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  receiver_.Bind(std::move(receiver));
}

void DataDecoderService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  mojo::GenericPendingReceiver receiver(interface_name,
                                        std::move(interface_pipe));
  if (auto r = receiver.As<mojom::ImageDecoder>())
    BindImageDecoder(std::move(r));
  else if (auto r = receiver.As<mojom::JsonParser>())
    BindJsonParser(std::move(r));
  else if (auto r = receiver.As<mojom::XmlParser>())
    BindXmlParser(std::move(r));
  else if (auto r = receiver.As<mojom::BundledExchangesParserFactory>())
    BindBundledExchangesParserFactory(std::move(r));
#if defined(OS_CHROMEOS)
  else if (auto r = receiver.As<mojom::BleScanParser>())
    BindBleScanParser(std::move(r));
#endif
}

void DataDecoderService::BindImageDecoder(
    mojo::PendingReceiver<mojom::ImageDecoder> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ImageDecoderImpl>(keepalive_.CreateRef()),
      std::move(receiver));
}

void DataDecoderService::BindJsonParser(
    mojo::PendingReceiver<mojom::JsonParser> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<JsonParserImpl>(keepalive_.CreateRef()),
      std::move(receiver));
}

void DataDecoderService::BindXmlParser(
    mojo::PendingReceiver<mojom::XmlParser> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<XmlParser>(keepalive_.CreateRef()), std::move(receiver));
}

void DataDecoderService::BindBundledExchangesParserFactory(
    mojo::PendingReceiver<mojom::BundledExchangesParserFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BundledExchangesParserFactory>(keepalive_.CreateRef()),
      std::move(receiver));
}

#ifdef OS_CHROMEOS
void DataDecoderService::BindBleScanParser(
    mojo::PendingReceiver<mojom::BleScanParser> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BleScanParserImpl>(keepalive_.CreateRef()),
      std::move(receiver));
}
#endif  // OS_CHROMEOS

}  // namespace data_decoder
