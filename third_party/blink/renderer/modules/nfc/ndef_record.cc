// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_unrestricted_double_or_array_buffer_or_dictionary.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record_init.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

WTF::Vector<uint8_t> GetUTF8DataFromString(const String& string) {
  StringUTF8Adaptor utf8_string(string);
  WTF::Vector<uint8_t> data;
  data.Append(utf8_string.data(), utf8_string.size());
  return data;
}

}  // namespace

// static
NDEFRecord* NDEFRecord::Create(const NDEFRecordInit* init) {
  // TODO(https://crbug.com/520391):
  // - Modify IDL definition of NDEFRecordData to be "typedef any
  //   NDEFRecordData;" as described at
  //   http://w3c.github.io/web-nfc/#dom-ndefrecorddata.
  // - Check validity of |init|, like whether the record type matches the
  //   provided NDEFRecordData as described at
  //   http://w3c.github.io/web-nfc/#creating-web-nfc-message, if not, return a
  //   nullptr.
  return MakeGarbageCollected<NDEFRecord>(init);
}

NDEFRecord::NDEFRecord(const NDEFRecordInit* init)
    : record_type_(init->recordType()), media_type_(init->mediaType()) {
  if (init->data().IsArrayBuffer()) {
    DOMArrayBuffer* buffer = init->data().GetAsArrayBuffer();
    data_.Append(static_cast<uint8_t*>(buffer->Data()), buffer->ByteLength());
  } else if (init->data().IsDictionary()) {
    Dictionary dictionary = init->data().GetAsDictionary();
    v8::Local<v8::String> jsonString;
    v8::Isolate* isolate = dictionary.GetIsolate();
    v8::TryCatch try_catch(isolate);
    if (v8::JSON::Stringify(dictionary.V8Context(), dictionary.V8Value())
            .ToLocal(&jsonString) &&
        !try_catch.HasCaught()) {
      data_ = GetUTF8DataFromString(
          ToBlinkString<String>(jsonString, kDoNotExternalize));
    }
  } else if (init->data().IsString()) {
    data_ = GetUTF8DataFromString(init->data().GetAsString());
  } else if (init->data().IsUnrestrictedDouble()) {
    data_ = GetUTF8DataFromString(
        String::Number(init->data().GetAsUnrestrictedDouble()));
  }
}

NDEFRecord::NDEFRecord(const device::mojom::blink::NDEFRecordPtr& record)
    : record_type_(NDEFRecordTypeToString(record->record_type)),
      media_type_(record->media_type),
      data_(record->data) {}

const String& NDEFRecord::recordType() const {
  return record_type_;
}

const String& NDEFRecord::mediaType() const {
  return media_type_;
}

String NDEFRecord::toText() const {
  if (record_type_.IsEmpty())
    return String();

  device::mojom::blink::NDEFRecordType type =
      StringToNDEFRecordType(record_type_);
  if (type == device::mojom::blink::NDEFRecordType::EMPTY)
    return String();

  // TODO(https://crbug.com/520391): Support utf-16 decoding for 'TEXT' record
  // as described at
  // http://w3c.github.io/web-nfc/#dfn-convert-ndefrecord-payloaddata-bytes.
  return String::FromUTF8WithLatin1Fallback(data_.data(), data_.size());
}

DOMArrayBuffer* NDEFRecord::toArrayBuffer() const {
  if (record_type_.IsEmpty())
    return nullptr;

  device::mojom::blink::NDEFRecordType type =
      StringToNDEFRecordType(record_type_);
  if (type != device::mojom::blink::NDEFRecordType::JSON &&
      type != device::mojom::blink::NDEFRecordType::OPAQUE_RECORD) {
    return nullptr;
  }

  return DOMArrayBuffer::Create(data_.data(), data_.size());
}

ScriptValue NDEFRecord::toJSON(ScriptState* script_state,
                               ExceptionState& exception_state) const {
  if (record_type_.IsEmpty())
    return ScriptValue();

  device::mojom::blink::NDEFRecordType type =
      StringToNDEFRecordType(record_type_);
  if (type != device::mojom::blink::NDEFRecordType::JSON &&
      type != device::mojom::blink::NDEFRecordType::OPAQUE_RECORD) {
    return ScriptValue();
  }

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> json_object = FromJSONString(
      script_state->GetIsolate(), script_state->GetContext(),
      String::FromUTF8WithLatin1Fallback(data_.data(), data_.size()),
      exception_state);
  if (exception_state.HadException())
    return ScriptValue();
  return ScriptValue(script_state, json_object);
}

void NDEFRecord::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
