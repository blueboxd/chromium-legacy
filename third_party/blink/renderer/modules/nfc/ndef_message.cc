// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_message.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_array_buffer_view_or_ndef_message_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_message_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
NDEFMessage* NDEFMessage::Create(const ExecutionContext* execution_context,
                                 const NDEFMessageInit* init,
                                 ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#creating-ndef-message

  // NDEFMessageInit#records is a required field.
  DCHECK(init->hasRecords());
  if (init->records().IsEmpty()) {
    exception_state.ThrowTypeError(
        "NDEFMessageInit#records being empty makes no sense.");
    return nullptr;
  }

  NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
    for (const NDEFRecordInit* record_init : init->records()) {
      NDEFRecord* record =
          NDEFRecord::Create(execution_context, record_init, exception_state);
      if (exception_state.HadException())
        return nullptr;
      DCHECK(record);
      message->records_.push_back(record);
    }
  return message;
}

// static
NDEFMessage* NDEFMessage::Create(const ExecutionContext* execution_context,
                                 const NDEFMessageSource& source,
                                 ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#creating-ndef-message
  if (source.IsString()) {
    NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
    message->records_.push_back(MakeGarbageCollected<NDEFRecord>(
        execution_context, source.GetAsString()));
    return message;
  }

  if (source.IsArrayBuffer()) {
    WTF::Vector<uint8_t> payload_data;
    size_t byte_length = source.GetAsArrayBuffer()->ByteLengthAsSizeT();
    if (byte_length > std::numeric_limits<wtf_size_t>::max()) {
      exception_state.ThrowRangeError(
          "Buffer size exceeds maximum heap object size.");
      return nullptr;
    }
    payload_data.Append(
        static_cast<uint8_t*>(source.GetAsArrayBuffer()->Data()),
        static_cast<wtf_size_t>(byte_length));
    NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
    message->records_.push_back(MakeGarbageCollected<NDEFRecord>(
        String() /* id */, "application/octet-stream",
        std::move(payload_data)));
    return message;
  }

  if (source.IsArrayBufferView()) {
    size_t byte_length =
        source.GetAsArrayBufferView().View()->byteLengthAsSizeT();
    if (byte_length > std::numeric_limits<wtf_size_t>::max()) {
      exception_state.ThrowRangeError(
          "Buffer size exceeds maximum heap object size.");
      return nullptr;
    }
    WTF::Vector<uint8_t> payload_data;
    payload_data.Append(
        static_cast<uint8_t*>(
            source.GetAsArrayBufferView().View()->BaseAddress()),
        static_cast<wtf_size_t>(byte_length));
    NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
    message->records_.push_back(MakeGarbageCollected<NDEFRecord>(
        String() /* id */, "application/octet-stream",
        std::move(payload_data)));
    return message;
  }

  if (source.IsNDEFMessageInit()) {
    return Create(execution_context, source.GetAsNDEFMessageInit(),
                  exception_state);
  }

  NOTREACHED();
  return nullptr;
}

NDEFMessage::NDEFMessage() = default;

NDEFMessage::NDEFMessage(const device::mojom::blink::NDEFMessage& message) {
  for (wtf_size_t i = 0; i < message.data.size(); ++i) {
    records_.push_back(MakeGarbageCollected<NDEFRecord>(*message.data[i]));
  }
}

const HeapVector<Member<NDEFRecord>>& NDEFMessage::records() const {
  return records_;
}

void NDEFMessage::Trace(blink::Visitor* visitor) {
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
