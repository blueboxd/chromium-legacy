// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NDEFRecordInit;
class ScriptState;
class StringOrUnrestrictedDoubleOrArrayBufferOrDictionary;

class MODULES_EXPORT NDEFRecord final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NDEFRecord* Create(const NDEFRecordInit*);

  NDEFRecord(const NDEFRecordInit*);
  NDEFRecord(const device::mojom::blink::NDEFRecordPtr&);

  const String& recordType() const;
  const String& mediaType() const;
  void data(ScriptState*,
            StringOrUnrestrictedDoubleOrArrayBufferOrDictionary&) const;

  void Trace(blink::Visitor*) override;

 private:
  String record_type_;
  String media_type_;

  enum class DataType {
    kNone,
    kArrayBuffer,
    kDictionary,
    kString,
    kUnrestrictedDouble,
  };
  DataType data_type_ = DataType::kNone;
  // For array buffer data type.
  scoped_refptr<WTF::ArrayBuffer> array_buffer_;
  // For dictionary or string data type.
  String string_;
  // For double data type.
  double unrestricted_double_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
