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

CrashyDataDecoderService::CrashyDataDecoderService(
    service_manager::mojom::ServiceRequest request,
    bool crash_json,
    bool crash_image)
    : binding_(this, std::move(request)),
      crash_json_(crash_json),
      crash_image_(crash_image) {}

CrashyDataDecoderService::~CrashyDataDecoderService() = default;

void CrashyDataDecoderService::OnStart() {
  real_service_.OnStart();
}

void CrashyDataDecoderService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK(interface_name == mojom::JsonParser::Name_ ||
         interface_name == mojom::ImageDecoder::Name_);
  if (interface_name == mojom::JsonParser::Name_ && crash_json_) {
    DCHECK(!json_parser_receiver_);
    json_parser_receiver_ = std::make_unique<mojo::Receiver<mojom::JsonParser>>(
        this,
        mojo::PendingReceiver<mojom::JsonParser>(std::move(interface_pipe)));
    return;
  }
  if (interface_name == mojom::ImageDecoder::Name_ && crash_image_) {
    DCHECK(!image_decoder_receiver_);
    image_decoder_receiver_ =
        std::make_unique<mojo::Receiver<mojom::ImageDecoder>>(
            this, mojo::PendingReceiver<mojom::ImageDecoder>(
                      std::move(interface_pipe)));
    return;
  }
  real_service_.OnBindInterface(source_info, interface_name,
                                std::move(interface_pipe));
}

// Overridden from mojom::ImageDecoder:
void CrashyDataDecoderService::DecodeImage(
    const std::vector<uint8_t>& encoded_data,
    mojom::ImageCodec codec,
    bool shrink_to_fit,
    int64_t max_size_in_bytes,
    const gfx::Size& desired_image_frame_size,
    DecodeImageCallback callback) {
  image_decoder_receiver_.reset();
}

void CrashyDataDecoderService::DecodeAnimation(
    const std::vector<uint8_t>& encoded_data,
    bool shrink_to_fit,
    int64_t max_size_in_bytes,
    DecodeAnimationCallback callback) {
  image_decoder_receiver_.reset();
}

void CrashyDataDecoderService::Parse(const std::string& json,
                                     ParseCallback callback) {
  json_parser_receiver_.reset();
}

}  // namespace data_decoder
