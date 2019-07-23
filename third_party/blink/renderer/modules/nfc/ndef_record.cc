// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_unrestricted_double_or_array_buffer_or_dictionary.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record_init.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

namespace blink {

// static
NDEFRecord* NDEFRecord::Create(const NDEFRecordInit* init) {
  return MakeGarbageCollected<NDEFRecord>(init);
}

NDEFRecord::NDEFRecord(const NDEFRecordInit* init)
    : record_type_(init->recordType()), media_type_(init->mediaType()) {
  if (init->data().IsArrayBuffer()) {
    data_type_ = DataType::kArrayBuffer;
    // Keep our own copy of data so that modifications of the original
    // ArrayBuffer |init->data()| in JS world do not affect us.
    DOMArrayBuffer* original = init->data().GetAsArrayBuffer();
    array_buffer_ =
        WTF::ArrayBuffer::Create(original->Data(), original->ByteLength());
  } else if (init->data().IsDictionary()) {
    data_type_ = DataType::kDictionary;
    // |dictionary| holds a V8 value of a JS Dictionary object, because WebIDL
    // spec (https://heycam.github.io/webidl/#idl-dictionaries) says that
    // dictionaries are always passed by value, rather than holding a reference
    // to it, here we stringify and save the dictionary data as |string_|, from
    // which we can build new Dictionary objects later when necessary.
    Dictionary dictionary = init->data().GetAsDictionary();
    v8::Local<v8::String> jsonString;
    v8::Isolate* isolate = dictionary.GetIsolate();
    v8::TryCatch try_catch(isolate);
    if (v8::JSON::Stringify(dictionary.V8Context(), dictionary.V8Value())
            .ToLocal(&jsonString) &&
        !try_catch.HasCaught()) {
      string_ = ToBlinkString<String>(jsonString, kDoNotExternalize);
    }
  } else if (init->data().IsString()) {
    data_type_ = DataType::kString;
    string_ = init->data().GetAsString();
  } else if (init->data().IsUnrestrictedDouble()) {
    data_type_ = DataType::kUnrestrictedDouble;
    unrestricted_double_ = init->data().GetAsUnrestrictedDouble();
  }
}

NDEFRecord::NDEFRecord(const device::mojom::blink::NDEFRecordPtr& record)
    : record_type_(NDEFRecordTypeToString(record->record_type)),
      media_type_(record->media_type) {
  switch (record->record_type) {
    case device::mojom::blink::NDEFRecordType::TEXT:
    case device::mojom::blink::NDEFRecordType::URL:
    case device::mojom::blink::NDEFRecordType::JSON: {
      String string_data;
      if (!record->data.IsEmpty()) {
        string_data = String::FromUTF8WithLatin1Fallback(
            static_cast<unsigned char*>(&record->data.front()),
            record->data.size());
      }

      // Convert back the stringified double.
      if (record->record_type == device::mojom::blink::NDEFRecordType::TEXT) {
        bool can_convert = false;
        double number = string_data.ToDouble(&can_convert);
        if (can_convert) {
          data_type_ = DataType::kUnrestrictedDouble;
          unrestricted_double_ = number;
          return;
        }
      }

      // Save the stringified JSON.
      if (record->record_type == device::mojom::blink::NDEFRecordType::JSON) {
        data_type_ = DataType::kDictionary;
        string_ = string_data;
        return;
      }

      data_type_ = DataType::kString;
      string_ = string_data;
      return;
    }

    case device::mojom::blink::NDEFRecordType::OPAQUE_RECORD: {
      if (!record->data.IsEmpty()) {
        data_type_ = DataType::kArrayBuffer;
        array_buffer_ = WTF::ArrayBuffer::Create(
            static_cast<void*>(&record->data.front()), record->data.size());
      }
      return;
    }

    case device::mojom::blink::NDEFRecordType::EMPTY:
      return;
  }
  NOTREACHED();
  return;
}

const String& NDEFRecord::recordType() const {
  return record_type_;
}

const String& NDEFRecord::mediaType() const {
  return media_type_;
}

void NDEFRecord::data(
    ScriptState* script_state,
    StringOrUnrestrictedDoubleOrArrayBufferOrDictionary& out) const {
  switch (data_type_) {
    case DataType::kNone:
      return;
    case DataType::kArrayBuffer:
      // We want NDEFRecord.data() to return a new ArrayBuffer object in JS
      // world each time.
      out.SetArrayBuffer(DOMArrayBuffer::Create(array_buffer_->Data(),
                                                array_buffer_->ByteLength()));
      return;
    case DataType::kDictionary: {
      // We want NDEFRecord.data() to return a new Dictionary object in JS world
      // each time.
      v8::Isolate* isolate = script_state->GetIsolate();
      v8::Local<v8::String> string = V8String(isolate, string_);
      v8::Local<v8::Value> json_object;
      v8::TryCatch try_catch(isolate);
      NonThrowableExceptionState exception_state;
      if (v8::JSON::Parse(script_state->GetContext(), string)
              .ToLocal(&json_object)) {
        out.SetDictionary(Dictionary(isolate, json_object, exception_state));
      } else {
        out.SetDictionary(Dictionary());
      }
      return;
    }
    case DataType::kString:
      out.SetString(string_);
      return;
    case DataType::kUnrestrictedDouble:
      out.SetUnrestrictedDouble(unrestricted_double_);
      return;
  }
  NOTREACHED();
  return;
}

void NDEFRecord::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
