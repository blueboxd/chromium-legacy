// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_message.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message_init.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

namespace blink {

// static
NDEFMessage* NDEFMessage::Create(const NDEFMessageInit* init) {
  return MakeGarbageCollected<NDEFMessage>(init);
}

NDEFMessage::NDEFMessage(const NDEFMessageInit* init) : url_(init->url()) {
  if (init->hasRecords()) {
    for (const NDEFRecordInit* record_init : init->records())
      records_.push_back(NDEFRecord::Create(record_init));
  }
}

NDEFMessage::NDEFMessage(const device::mojom::blink::NDEFMessage& message)
    : url_(message.url) {
  for (wtf_size_t i = 0; i < message.data.size(); ++i) {
    records_.push_back(MakeGarbageCollected<NDEFRecord>(message.data[i]));
  }
}

const String& NDEFMessage::url() const {
  return url_;
}

const HeapVector<Member<NDEFRecord>>& NDEFMessage::records() const {
  return records_;
}

void NDEFMessage::Trace(blink::Visitor* visitor) {
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
