// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/decode_image.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace data_decoder {

namespace {

// Helper callback which owns a mojo::Remote<ImageDecoder> until invoked. This
// keeps the ImageDecoder pipe open just long enough to dispatch a reply, at
// which point the reply is forwarded to the wrapped |callback|.
void OnDecodeImage(mojo::Remote<mojom::DataDecoderService> service,
                   mojo::Remote<mojom::ImageDecoder> decoder,
                   mojom::ImageDecoder::DecodeImageCallback callback,
                   const SkBitmap& bitmap) {
  std::move(callback).Run(bitmap);
}

void OnDecodeImages(mojo::Remote<mojom::DataDecoderService> service,
                    mojo::Remote<mojom::ImageDecoder> decoder,
                    mojom::ImageDecoder::DecodeAnimationCallback callback,
                    std::vector<mojom::AnimationFramePtr> bitmaps) {
  std::move(callback).Run(std::move(bitmaps));
}

}  // namespace

void DecodeImage(service_manager::Connector* connector,
                 const std::vector<uint8_t>& encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 mojom::ImageDecoder::DecodeImageCallback callback) {
  mojo::PendingRemote<mojom::ImageDecoder> pending_decoder;
  connector->Connect(mojom::kServiceName,
                     pending_decoder.InitWithNewPipeAndPassReceiver());
  return DecodeImage(std::move(pending_decoder), encoded_bytes, codec,
                     shrink_to_fit, max_size_in_bytes, desired_image_frame_size,
                     std::move(callback));
}

void DecodeImage(mojo::PendingRemote<mojom::ImageDecoder> pending_decoder,
                 const std::vector<uint8_t>& encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 mojom::ImageDecoder::DecodeImageCallback callback) {
  mojo::Remote<mojom::DataDecoderService> null_service;
  mojo::Remote<mojom::ImageDecoder> decoder(std::move(pending_decoder));

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(call_once, SkBitmap()));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeImage(
      encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      desired_image_frame_size,
      base::BindOnce(&OnDecodeImage, std::move(null_service),
                     std::move(decoder), std::move(call_once)));
}

void DecodeImage(mojo::Remote<mojom::DataDecoderService> service,
                 const std::vector<uint8_t>& encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 mojom::ImageDecoder::DecodeImageCallback callback) {
  mojo::Remote<mojom::ImageDecoder> decoder;
  service->BindImageDecoder(decoder.BindNewPipeAndPassReceiver());

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(call_once, SkBitmap()));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeImage(
      encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      desired_image_frame_size,
      base::BindOnce(&OnDecodeImage, std::move(service), std::move(decoder),
                     std::move(call_once)));
}

void DecodeAnimation(mojo::PendingRemote<mojom::ImageDecoder> pending_decoder,
                     const std::vector<uint8_t>& encoded_bytes,
                     bool shrink_to_fit,
                     uint64_t max_size_in_bytes,
                     mojom::ImageDecoder::DecodeAnimationCallback callback) {
  mojo::Remote<mojom::DataDecoderService> null_service;
  mojo::Remote<mojom::ImageDecoder> decoder(std::move(pending_decoder));

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(
      call_once, base::Passed(std::vector<mojom::AnimationFramePtr>())));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeAnimation(
      encoded_bytes, shrink_to_fit, max_size_in_bytes,
      base::BindOnce(&OnDecodeImages, std::move(null_service),
                     std::move(decoder), std::move(call_once)));
}

void DecodeAnimation(mojo::Remote<mojom::DataDecoderService> service,
                     const std::vector<uint8_t>& encoded_bytes,
                     bool shrink_to_fit,
                     uint64_t max_size_in_bytes,
                     mojom::ImageDecoder::DecodeAnimationCallback callback) {
  mojo::Remote<mojom::ImageDecoder> decoder;
  service->BindImageDecoder(decoder.BindNewPipeAndPassReceiver());

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(
      call_once, base::Passed(std::vector<mojom::AnimationFramePtr>())));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeAnimation(
      encoded_bytes, shrink_to_fit, max_size_in_bytes,
      base::BindOnce(&OnDecodeImages, std::move(service), std::move(decoder),
                     std::move(call_once)));
}

}  // namespace data_decoder
