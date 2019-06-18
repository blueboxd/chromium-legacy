// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

using device::mojom::blink::NDEFCompatibility;
using device::mojom::blink::NDEFRecordType;
using device::mojom::blink::NFCPushTarget;

namespace blink {

ScriptPromise RejectIfInvalidTextRecord(ScriptState* script_state,
                                        const NDEFRecord* record) {
  const NDEFRecordData& value = record->data();

  if (!value.IsString() && !(value.IsUnrestrictedDouble() &&
                             !std::isnan(value.GetAsUnrestrictedDouble()))) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcTextRecordTypeError));
  }

  if (record->hasMediaType() &&
      !record->mediaType().StartsWithIgnoringASCIICase(
          kNfcPlainTextMimePrefix)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                           kNfcTextRecordMediaTypeError));
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidURLRecord(ScriptState* script_state,
                                       const NDEFRecord* record) {
  if (!record->data().IsString()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcUrlRecordTypeError));
  }

  if (!KURL(NullURL(), record->data().GetAsString()).IsValid()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                           kNfcUrlRecordParseError));
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidJSONRecord(ScriptState* script_state,
                                        const NDEFRecord* record) {
  if (!record->data().IsDictionary()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcJsonRecordTypeError));
  }

  // If JSON record has media type, it must be equal to "application/json" or
  // start with "application/" and end with "+json".
  if (record->hasMediaType() &&
      (record->mediaType() != kNfcJsonMimeType &&
       !(record->mediaType().StartsWithIgnoringASCIICase(kNfcJsonMimePrefix) &&
         record->mediaType().EndsWithIgnoringASCIICase(kNfcJsonMimePostfix)))) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                           kNfcJsonRecordMediaTypeError));
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidOpaqueRecord(ScriptState* script_state,
                                          const NDEFRecord* record) {
  if (!record->data().IsArrayBuffer()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          kNfcOpaqueRecordTypeError));
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidNDEFRecord(ScriptState* script_state,
                                        const NDEFRecord* record) {
  NDEFRecordType type;
  if (record->hasRecordType()) {
    type = StringToNDEFRecordType(record->recordType());
  } else {
    type = DeduceRecordTypeFromDataType(record);

    // https://w3c.github.io/web-nfc/#creating-web-nfc-message
    // If NDEFRecord.recordType is not set and record type cannot be deduced
    // from NDEFRecord.data, reject promise with TypeError.
    if (type == NDEFRecordType::EMPTY) {
      return ScriptPromise::Reject(
          script_state, V8ThrowException::CreateTypeError(
                            script_state->GetIsolate(), kNfcRecordTypeError));
    }
  }

  // Non-empty records must have data.
  if (!record->hasData() && (type != NDEFRecordType::EMPTY)) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcRecordDataError));
  }

  switch (type) {
    case NDEFRecordType::TEXT:
      return RejectIfInvalidTextRecord(script_state, record);
    case NDEFRecordType::URL:
      return RejectIfInvalidURLRecord(script_state, record);
    case NDEFRecordType::JSON:
      return RejectIfInvalidJSONRecord(script_state, record);
    case NDEFRecordType::OPAQUE_RECORD:
      return RejectIfInvalidOpaqueRecord(script_state, record);
    case NDEFRecordType::EMPTY:
      return ScriptPromise();
  }

  NOTREACHED();
  return ScriptPromise::Reject(
      script_state, V8ThrowException::CreateTypeError(
                        script_state->GetIsolate(), kNfcRecordError));
}

ScriptPromise RejectIfInvalidNDEFRecordArray(
    ScriptState* script_state,
    const HeapVector<Member<NDEFRecord>>& records) {
  for (const auto& record : records) {
    ScriptPromise isValidRecord =
        RejectIfInvalidNDEFRecord(script_state, record);
    if (!isValidRecord.IsEmpty())
      return isValidRecord;
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidNDEFMessageSource(
    ScriptState* script_state,
    const NDEFMessageSource& push_message) {
  // If NDEFMessageSource of invalid type, reject promise with TypeError
  if (!push_message.IsNDEFMessage() && !push_message.IsString() &&
      !push_message.IsArrayBuffer()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcMsgTypeError));
  }

  if (push_message.IsNDEFMessage()) {
    // https://w3c.github.io/web-nfc/#the-push-method
    // If NDEFMessage.records is empty, reject promise with TypeError
    const NDEFMessage* message = push_message.GetAsNDEFMessage();
    if (!message->hasRecords() || message->records().IsEmpty()) {
      return ScriptPromise::Reject(
          script_state, V8ThrowException::CreateTypeError(
                            script_state->GetIsolate(), kNfcEmptyMsg));
    }

    return RejectIfInvalidNDEFRecordArray(script_state, message->records());
  }

  return ScriptPromise();
}

NDEFRecordData BuildRecordData(
    ScriptState* script_state,
    const device::mojom::blink::NDEFRecordPtr& record) {
  NDEFRecordData result = NDEFRecordData();
  switch (record->record_type) {
    case NDEFRecordType::TEXT:
    case NDEFRecordType::URL:
    case NDEFRecordType::JSON: {
      String string_data;
      if (!record->data.IsEmpty()) {
        string_data = String::FromUTF8WithLatin1Fallback(
            static_cast<unsigned char*>(&record->data.front()),
            record->data.size());
      }

      // Convert back the stringified double.
      if (record->record_type == NDEFRecordType::TEXT) {
        bool can_convert = false;
        double number = string_data.ToDouble(&can_convert);
        if (can_convert) {
          result.SetUnrestrictedDouble(number);
          return result;
        }
      }

      // Stringified JSON must be converted back to an Object.
      if (record->record_type == NDEFRecordType::JSON) {
        v8::Isolate* isolate = script_state->GetIsolate();
        v8::Local<v8::String> string = V8String(isolate, string_data);
        v8::Local<v8::Value> json_object;
        v8::TryCatch try_catch(isolate);
        NonThrowableExceptionState exception_state;

        if (v8::JSON::Parse(script_state->GetContext(), string)
                .ToLocal(&json_object)) {
          result.SetDictionary(
              Dictionary(isolate, json_object, exception_state));
          return result;
        }
      }

      result.SetString(string_data);
      return result;
    }

    case NDEFRecordType::OPAQUE_RECORD: {
      if (!record->data.IsEmpty()) {
        result.SetArrayBuffer(DOMArrayBuffer::Create(
            static_cast<void*>(&record->data.front()), record->data.size()));
      }

      return result;
    }

    case NDEFRecordType::EMPTY:
      return result;
  }

  NOTREACHED();
  return result;
}

// https://w3c.github.io/web-nfc/#creating-web-nfc-message Step 2.1
// If NDEFRecord type is not provided, deduce NDEFRecord type from JS data type:
// String or Number => 'text' record
// ArrayBuffer => 'opaque' record
// Dictionary, JSON serializable Object => 'json' record
NDEFRecordType DeduceRecordTypeFromDataType(const blink::NDEFRecord* record) {
  if (record->hasData()) {
    const blink::NDEFRecordData& value = record->data();

    if (value.IsString() || (value.IsUnrestrictedDouble() &&
                             !std::isnan(value.GetAsUnrestrictedDouble()))) {
      return NDEFRecordType::TEXT;
    }

    if (value.IsDictionary()) {
      return NDEFRecordType::JSON;
    }

    if (value.IsArrayBuffer()) {
      return NDEFRecordType::OPAQUE_RECORD;
    }
  }

  return NDEFRecordType::EMPTY;
}

size_t GetNDEFMessageSize(const device::mojom::blink::NDEFMessagePtr& message) {
  size_t message_size = message->url.CharactersSizeInBytes();
  for (wtf_size_t i = 0; i < message->data.size(); ++i) {
    message_size += message->data[i]->media_type.CharactersSizeInBytes();
    message_size += message->data[i]->data.size();
  }
  return message_size;
}

bool SetNDEFMessageURL(const String& origin,
                       device::mojom::blink::NDEFMessagePtr& message) {
  KURL origin_url(origin);

  if (!message->url.IsEmpty() && origin_url.CanSetPathname()) {
    origin_url.SetPath(message->url);
  }

  message->url = origin_url;
  return origin_url.IsValid();
}

String NDEFRecordTypeToString(const NDEFRecordType& type) {
  switch (type) {
    case NDEFRecordType::TEXT:
      return "text";
    case NDEFRecordType::URL:
      return "url";
    case NDEFRecordType::JSON:
      return "json";
    case NDEFRecordType::OPAQUE_RECORD:
      return "opaque";
    case NDEFRecordType::EMPTY:
      return "empty";
  }

  NOTREACHED();
  return String();
}

NDEFCompatibility StringToNDEFCompatibility(const String& compatibility) {
  if (compatibility == "nfc-forum")
    return NDEFCompatibility::NFC_FORUM;

  if (compatibility == "vendor")
    return NDEFCompatibility::VENDOR;

  if (compatibility == "any")
    return NDEFCompatibility::ANY;

  NOTREACHED();
  return NDEFCompatibility::NFC_FORUM;
}

NDEFRecordType StringToNDEFRecordType(const String& recordType) {
  if (recordType == "empty")
    return NDEFRecordType::EMPTY;

  if (recordType == "text")
    return NDEFRecordType::TEXT;

  if (recordType == "url")
    return NDEFRecordType::URL;

  if (recordType == "json")
    return NDEFRecordType::JSON;

  if (recordType == "opaque")
    return NDEFRecordType::OPAQUE_RECORD;

  NOTREACHED();
  return NDEFRecordType::EMPTY;
}

NFCPushTarget StringToNFCPushTarget(const String& target) {
  if (target == "tag")
    return NFCPushTarget::TAG;

  if (target == "peer")
    return NFCPushTarget::PEER;

  return NFCPushTarget::ANY;
}

NDEFRecord* MojoToBlinkNDEFRecord(
    ScriptState* script_state,
    const device::mojom::blink::NDEFRecordPtr& record) {
  NDEFRecord* nfc_record = NDEFRecord::Create();
  nfc_record->setMediaType(record->media_type);
  nfc_record->setRecordType(NDEFRecordTypeToString(record->record_type));
  nfc_record->setData(BuildRecordData(script_state, record));
  return nfc_record;
}

NDEFMessage* MojoToBlinkNDEFMessage(
    ScriptState* script_state,
    const device::mojom::blink::NDEFMessagePtr& message) {
  NDEFMessage* ndef_message = NDEFMessage::Create();
  ndef_message->setURL(message->url);
  blink::HeapVector<Member<NDEFRecord>> records;
  for (wtf_size_t i = 0; i < message->data.size(); ++i)
    records.push_back(MojoToBlinkNDEFRecord(script_state, message->data[i]));
  ndef_message->setRecords(records);
  return ndef_message;
}

}  // namespace blink
