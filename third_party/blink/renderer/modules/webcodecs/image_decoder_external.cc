// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

namespace blink {

// static
ImageDecoderExternal* ImageDecoderExternal::Create(
    ScriptState* script_state,
    const ImageDecoderInit* init,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ImageDecoderExternal>(script_state, init,
                                                    exception_state);
}

ImageDecoderExternal::DecodeRequest::DecodeRequest(
    ScriptPromiseResolver* resolver,
    uint32_t frame_index)
    : resolver(resolver), frame_index(frame_index) {}

void ImageDecoderExternal::DecodeRequest::Trace(Visitor* visitor) {
  visitor->Trace(resolver);
}

ImageDecoderExternal::ImageDecoderExternal(ScriptState* script_state,
                                           const ImageDecoderInit* init,
                                           ExceptionState& exception_state)
    : script_state_(script_state) {
  // |data| is a required field.
  DCHECK(init->hasData());
  DCHECK(!init->data().IsNull());

  options_ =
      init->hasOptions() ? init->options() : ImageBitmapOptions::Create();

  if (init->data().IsReadableStream()) {
    consumer_ = MakeGarbageCollected<ReadableStreamBytesConsumer>(
        script_state, init->data().GetAsReadableStream(), exception_state);
    if (exception_state.HadException())
      return;

    stream_buffer_ = WTF::SharedBuffer::Create();
    consumer_->SetClient(this);

    // We can't create the ImageDecoder until we have some data, so we may be
    // done for now; we need one initial call to OnStateChange(), but thereafter
    // calls will be driven by the ReadableStreamBytesConsumer.
    OnStateChange();
    return;
  }

  // Since we don't make a copy of buffer passed in, we must retain a reference.
  init_data_ = init;

  DOMArrayPiece buffer;
  if (init->data().IsArrayBuffer()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBuffer());
  } else if (init->data().IsArrayBufferView()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBufferView().View());
  } else {
    NOTREACHED();
    return;
  }

  // TODO: Data is owned by the caller who may be free to manipulate it. We will
  // probably need to make a copy to our own internal data or neuter the buffers
  // as seen by JS.
  auto sr = SegmentReader::CreateFromSkData(
      SkData::MakeWithoutCopy(buffer.Data(), buffer.ByteLengthAsSizeT()));
  if (!sr) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Failed to read image data");
    return;
  }

  data_complete_ = true;
  MaybeCreateImageDecoder(std::move(sr));
  if (!decoder_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Failed to create image decoder");
    return;
  }

  UpdateFrameAndRepetitionCount();
  if (decoder_->Failed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Image decoding failed");
    return;
  }
}

ImageDecoderExternal::~ImageDecoderExternal() {
  DVLOG(1) << __func__;
}

ScriptPromise ImageDecoderExternal::decode(uint32_t frame_index) {
  DVLOG(1) << __func__;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();
  pending_decodes_.push_back(
      MakeGarbageCollected<DecodeRequest>(resolver, frame_index));
  MaybeSatisfyPendingDecodes();
  return promise;
}

uint32_t ImageDecoderExternal::frameCount() const {
  return frame_count_;
}

String ImageDecoderExternal::type() const {
  return mime_type_;
}

uint32_t ImageDecoderExternal::repetitionCount() const {
  return repetition_count_;
}

void ImageDecoderExternal::OnStateChange() {
  const char* buffer;
  size_t available;
  while (!data_complete_) {
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;

    if (result == BytesConsumer::Result::kOk) {
      if (available > 0)
        stream_buffer_->Append(buffer, SafeCast<wtf_size_t>(available));
      result = consumer_->EndRead(available);
    }

    if (result == BytesConsumer::Result::kError) {
      data_complete_ = true;
      return;
    }

    data_complete_ = result == BytesConsumer::Result::kDone;
    if (!decoder_)
      MaybeCreateImageDecoder(nullptr);
    else
      decoder_->SetData(stream_buffer_, data_complete_);

    UpdateFrameAndRepetitionCount();
    MaybeSatisfyPendingDecodes();
  }
}

String ImageDecoderExternal::DebugName() const {
  return "ImageDecoderExternal";
}

void ImageDecoderExternal::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(consumer_);
  visitor->Trace(pending_decodes_);
  visitor->Trace(init_data_);
  visitor->Trace(options_);
  ScriptWrappable::Trace(visitor);
}

void ImageDecoderExternal::MaybeSatisfyPendingDecodes() {
  for (auto& request : pending_decodes_) {
    if (!data_complete_) {
      // We can't fulfill this promise at this time.
      if (request->frame_index >= frame_count_)
        continue;

      DCHECK(decoder_);
    } else if (!decoder_ || request->frame_index >= frame_count_) {
      request->complete = true;
      // TODO: Include frameIndex in rejection?
      request->resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Frame index out of range"));
      continue;
    }

    auto* image = decoder_->DecodeFrameBufferAtIndex(request->frame_index);
    if (decoder_->Failed() || !image) {
      request->complete = true;
      // TODO: Include frameIndex in rejection?
      request->resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Failed to decode frame"));
      continue;
    }

    // Only satisfy fully complete decode requests.
    if (image->GetStatus() != ImageFrame::kFrameComplete)
      continue;

    auto sk_image = image->FinalizePixelsAndGetImage();
    if (!sk_image) {
      request->complete = true;
      // TODO: Include frameIndex in rejection?
      request->resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Failed decode frame"));
      continue;
    }

    auto* result = ImageFrameExternal::Create();
    result->setImage(MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(sk_image),
                                               decoder_->Orientation()),
        base::nullopt, options_));
    result->setDuration(
        decoder_->FrameDurationAtIndex(request->frame_index).InMicroseconds());
    result->setOrientation(decoder_->Orientation().Orientation());
    request->complete = true;
    request->resolver->Resolve(result);
  }

  auto* new_end =
      std::remove_if(pending_decodes_.begin(), pending_decodes_.end(),
                     [](const auto& request) { return request->complete; });
  pending_decodes_.Shrink(
      static_cast<wtf_size_t>(new_end - pending_decodes_.begin()));
}

void ImageDecoderExternal::MaybeCreateImageDecoder(
    scoped_refptr<SegmentReader> sr) {
  // TODO: This does not handle SVG Images since they use another "decoder." It
  // is highly coupled with the DOM today, so isn't suitable for this API.

  // TODO: We should probably call ImageDecoder::SetMemoryAllocator() so that
  // we can recycle frame buffers for decoded images.

  constexpr char kNoneOption[] = "none";

  auto color_behavior = ColorBehavior::Tag();
  if (options_->colorSpaceConversion() == kNoneOption)
    color_behavior = ColorBehavior::Ignore();

  auto premultiply_alpha = ImageDecoder::kAlphaPremultiplied;
  if (options_->premultiplyAlpha() == kNoneOption)
    premultiply_alpha = ImageDecoder::kAlphaNotPremultiplied;

  // TODO: Is it okay to use resize size like this?
  auto desired_size = SkISize::MakeEmpty();
  if (options_->hasResizeWidth() && options_->hasResizeHeight()) {
    desired_size =
        SkISize::Make(options_->resizeWidth(), options_->resizeHeight());
  }

  if (stream_buffer_) {
    // TODO: If mime-type is supplied we must use that instead of sniffing.
    if (!ImageDecoder::HasSufficientDataToSniffImageType(*stream_buffer_))
      return;

    DCHECK(!sr);
    decoder_ = ImageDecoder::Create(
        stream_buffer_, data_complete_, premultiply_alpha,
        ImageDecoder::kHighBitDepthToHalfFloat, color_behavior,
        ImageDecoder::OverrideAllowDecodeToYuv::kDeny, desired_size);
    return;
  }

  DCHECK(data_complete_);
  decoder_ = ImageDecoder::Create(
      std::move(sr), data_complete_, premultiply_alpha,
      ImageDecoder::kHighBitDepthToHalfFloat, color_behavior,
      ImageDecoder::OverrideAllowDecodeToYuv::kDeny, desired_size);
}

void ImageDecoderExternal::UpdateFrameAndRepetitionCount() {
  if (!decoder_)
    return;

  const size_t decoded_frame_count = decoder_->FrameCount();
  if (decoder_->Failed())
    return;

  // TODO: Is this useful? We should have each decoder indicate its own mime
  // type then.
  mime_type_ = "image/todo";
  frame_count_ = static_cast<uint32_t>(decoded_frame_count);

  // The internal value has some magic negative numbers; for external purposes
  // we want to only surface positive repetition counts. The rest is up to the
  // client.
  const int decoded_repetition_count = decoder_->RepetitionCount();
  if (decoded_repetition_count > 0)
    repetition_count_ = decoded_repetition_count;
}

}  // namespace blink
