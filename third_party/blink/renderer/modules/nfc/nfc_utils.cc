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
                                        const NDEFRecordInit* record) {
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
                                       const NDEFRecordInit* record) {
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
                                        const NDEFRecordInit* record) {
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
                                          const NDEFRecordInit* record) {
  if (!record->data().IsArrayBuffer()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          kNfcOpaqueRecordTypeError));
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidNDEFRecord(ScriptState* script_state,
                                        const NDEFRecordInit* record) {
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
    const HeapVector<Member<NDEFRecordInit>>& records) {
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
  if (!push_message.IsNDEFMessageInit() && !push_message.IsString() &&
      !push_message.IsArrayBuffer()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcMsgTypeError));
  }

  if (push_message.IsNDEFMessageInit()) {
    // https://w3c.github.io/web-nfc/#the-push-method
    // If NDEFMessage.records is empty, reject promise with TypeError
    const NDEFMessageInit* message = push_message.GetAsNDEFMessageInit();
    if (!message->hasRecords() || message->records().IsEmpty()) {
      return ScriptPromise::Reject(
          script_state, V8ThrowException::CreateTypeError(
                            script_state->GetIsolate(), kNfcEmptyMsg));
    }

    return RejectIfInvalidNDEFRecordArray(script_state, message->records());
  }

  return ScriptPromise();
}

// https://w3c.github.io/web-nfc/#creating-web-nfc-message Step 2.1
// If NDEFRecord type is not provided, deduce NDEFRecord type from JS data type:
// String or Number => 'text' record
// ArrayBuffer => 'opaque' record
// Dictionary, JSON serializable Object => 'json' record
NDEFRecordType DeduceRecordTypeFromDataType(
    const blink::NDEFRecordInit* record) {
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

}  // namespace blink
