// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMArrayBuffer;
class DOMDataView;
class ExceptionState;
class NDEFRecordInit;
class ScriptState;

class MODULES_EXPORT NDEFRecord final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NDEFRecord* Create(const NDEFRecordInit*, ExceptionState&);

  // Construct a "text" record from a string.
  explicit NDEFRecord(const String&);

  // Construct a "opaque" record from an array buffer.
  explicit NDEFRecord(DOMArrayBuffer*);

  NDEFRecord(const String&, const String&, WTF::Vector<uint8_t>);
  explicit NDEFRecord(const device::mojom::blink::NDEFRecord&);

  const String& recordType() const;
  const String& mediaType() const;
  const String& id() const;
  DOMDataView* data() const;
  String text() const;
  DOMArrayBuffer* arrayBuffer() const;
  ScriptValue json(ScriptState*, ExceptionState&) const;

  const WTF::Vector<uint8_t>& payloadData() const;

  void Trace(blink::Visitor*) override;

 private:
  String record_type_;
  String media_type_;
  String id_;
  // Holds the NDEFRecord.[[PayloadData]] bytes defined at
  // https://w3c.github.io/web-nfc/#the-ndefrecord-interface.
  WTF::Vector<uint8_t> payload_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
