// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"

#include <utility>
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"

namespace blink {

BlinkTransferableMessage::BlinkTransferableMessage() = default;
BlinkTransferableMessage::~BlinkTransferableMessage() = default;

BlinkTransferableMessage::BlinkTransferableMessage(BlinkTransferableMessage&&) =
    default;
BlinkTransferableMessage& BlinkTransferableMessage::operator=(
    BlinkTransferableMessage&&) = default;

scoped_refptr<blink::StaticBitmapImage> ToStaticBitmapImage(
    const SkBitmap& sk_bitmap) {
  auto handle = WTF::ArrayBufferContents::CreateDataHandle(
      sk_bitmap.computeByteSize(), WTF::ArrayBufferContents::kZeroInitialize);
  if (!handle)
    return nullptr;

  WTF::ArrayBufferContents array_buffer_contents(
      std::move(handle), WTF::ArrayBufferContents::kNotShared);
  if (!array_buffer_contents.Data())
    return nullptr;

  SkImageInfo info = sk_bitmap.info();
  if (!sk_bitmap.readPixels(info, array_buffer_contents.Data(),
                            info.minRowBytes(), 0, 0))
    return nullptr;

  return blink::StaticBitmapImage::Create(array_buffer_contents, info);
}

base::Optional<SkBitmap> ToSkBitmap(
    const scoped_refptr<blink::StaticBitmapImage>& static_bitmap_image) {
  const sk_sp<SkImage> image =
      static_bitmap_image->PaintImageForCurrentFrame().GetSkImage();
  SkBitmap result;
  if (image && image->asLegacyBitmap(
                   &result, SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode)) {
    return result;
  }
  return base::nullopt;
}

BlinkTransferableMessage ToBlinkTransferableMessage(
    TransferableMessage message) {
  BlinkTransferableMessage result;
  result.message = SerializedScriptValue::Create(
      reinterpret_cast<const char*>(message.encoded_message.data()),
      message.encoded_message.size());
  for (auto& blob : message.blobs) {
    result.message->BlobDataHandles().Set(
        String::FromUTF8(blob->uuid),
        BlobDataHandle::Create(
            String::FromUTF8(blob->uuid), String::FromUTF8(blob->content_type),
            blob->size,
            mojom::blink::BlobPtrInfo(blob->blob.PassHandle(),
                                      mojom::Blob::Version_)));
  }
  result.sender_stack_trace_id = v8_inspector::V8StackTraceId(
      static_cast<uintptr_t>(message.stack_trace_id),
      std::make_pair(message.stack_trace_debugger_id_first,
                     message.stack_trace_debugger_id_second));
  result.locked_agent_cluster_id = message.locked_agent_cluster_id;
  result.ports.AppendRange(message.ports.begin(), message.ports.end());
  result.message->GetStreamChannels().AppendRange(
      message.stream_channels.begin(), message.stream_channels.end());
  result.has_user_gesture = message.has_user_gesture;
  if (message.user_activation) {
    result.user_activation = mojom::blink::UserActivationSnapshot::New(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }
  result.transfer_user_activation = message.transfer_user_activation;
  result.allow_autoplay = message.allow_autoplay;

  if (!message.array_buffer_contents_array.empty()) {
    SerializedScriptValue::ArrayBufferContentsArray array_buffer_contents_array;
    array_buffer_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            message.array_buffer_contents_array.size()));

    for (auto& item : message.array_buffer_contents_array) {
      mojo_base::BigBuffer& big_buffer = item->contents;
      auto handle = WTF::ArrayBufferContents::CreateDataHandle(
          big_buffer.size(), WTF::ArrayBufferContents::kZeroInitialize);
      WTF::ArrayBufferContents contents(std::move(handle),
                                        WTF::ArrayBufferContents::kNotShared);
      memcpy(contents.Data(), big_buffer.data(), big_buffer.size());
      array_buffer_contents_array.push_back(std::move(contents));
    }
    result.message->SetArrayBufferContentsArray(
        std::move(array_buffer_contents_array));
  }

  if (!message.image_bitmap_contents_array.empty()) {
    SerializedScriptValue::ImageBitmapContentsArray image_bitmap_contents_array;
    image_bitmap_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            message.image_bitmap_contents_array.size()));

    for (auto& sk_bitmap : message.image_bitmap_contents_array) {
      const scoped_refptr<StaticBitmapImage> bitmap_contents =
          ToStaticBitmapImage(sk_bitmap);
      if (!bitmap_contents)
        continue;
      image_bitmap_contents_array.push_back(bitmap_contents);
    }
    result.message->SetImageBitmapContentsArray(
        std::move(image_bitmap_contents_array));
  }

  return result;
}

TransferableMessage ToTransferableMessage(BlinkTransferableMessage message) {
  TransferableMessage result;
  result.encoded_message = message.message->GetWireData();
  result.blobs.reserve(message.message->BlobDataHandles().size());
  for (const auto& blob : message.message->BlobDataHandles()) {
    result.blobs.push_back(mojom::SerializedBlob::New(
        blob.value->Uuid().Utf8(), blob.value->GetType().Utf8(),
        blob.value->size(),
        mojom::BlobPtrInfo(
            blob.value->CloneBlobPtr().PassInterface().PassHandle(),
            mojom::Blob::Version_)));
  }
  result.stack_trace_id = message.sender_stack_trace_id.id;
  result.stack_trace_debugger_id_first =
      message.sender_stack_trace_id.debugger_id.first;
  result.stack_trace_debugger_id_second =
      message.sender_stack_trace_id.debugger_id.second;
  result.locked_agent_cluster_id = message.locked_agent_cluster_id;
  result.ports.assign(message.ports.begin(), message.ports.end());
  auto& stream_channels = message.message->GetStreamChannels();
  result.stream_channels.assign(stream_channels.begin(), stream_channels.end());
  result.has_user_gesture = message.has_user_gesture;
  if (message.user_activation) {
    result.user_activation = mojom::UserActivationSnapshot::New(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }
  result.transfer_user_activation = message.transfer_user_activation;
  result.allow_autoplay = message.allow_autoplay;

  auto& array_buffer_contents_array =
      message.message->GetArrayBufferContentsArray();
  result.array_buffer_contents_array.reserve(
      array_buffer_contents_array.size());
  for (auto& contents : array_buffer_contents_array) {
    uint8_t* allocation_start = static_cast<uint8_t*>(contents.Data());
    mojo_base::BigBuffer buffer(
        base::make_span(allocation_start, contents.DataLength()));
    result.array_buffer_contents_array.push_back(
        mojom::SerializedArrayBufferContents::New(std::move(buffer)));
  }

  auto& image_bitmap_contents_array =
      message.message->GetImageBitmapContentsArray();
  result.image_bitmap_contents_array.reserve(
      image_bitmap_contents_array.size());
  for (auto& contents : image_bitmap_contents_array) {
    base::Optional<SkBitmap> bitmap = ToSkBitmap(contents);
    if (!bitmap)
      continue;
    result.image_bitmap_contents_array.push_back(std::move(bitmap.value()));
  }

  return result;
}

}  // namespace blink
