// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error.h"
#include "third_party/blink/renderer/modules/nfc/nfc_push_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reader_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using device::mojom::blink::NDEFCompatibility;
using device::mojom::blink::NDEFMessage;
using device::mojom::blink::NDEFMessagePtr;
using device::mojom::blink::NDEFRecord;
using device::mojom::blink::NDEFRecordPtr;
using device::mojom::blink::NDEFRecordType;
using device::mojom::blink::NDEFRecordTypeFilter;
using device::mojom::blink::NFCPushOptions;
using device::mojom::blink::NFCPushOptionsPtr;
using device::mojom::blink::NFCPushTarget;
using device::mojom::blink::NFCReaderOptions;
using device::mojom::blink::NFCReaderOptionsPtr;

using WTF::String;

namespace {

void SetMediaType(NDEFRecordPtr& recordPtr,
                  const String& recordMediaType,
                  const String& defaultMediaType) {
  recordPtr->media_type =
      recordMediaType.IsEmpty() ? defaultMediaType : recordMediaType;
}

}  // anonymous namespace

// Mojo type converters
namespace mojo {

Vector<uint8_t> TypeConverter<Vector<uint8_t>, String>::Convert(
    const String& string) {
  StringUTF8Adaptor utf8_string(string);
  Vector<uint8_t> array;
  array.Append(utf8_string.data(), utf8_string.size());
  return array;
}

Vector<uint8_t> TypeConverter<Vector<uint8_t>, blink::DOMArrayBuffer*>::Convert(
    blink::DOMArrayBuffer* buffer) {
  Vector<uint8_t> array;
  array.Append(static_cast<uint8_t*>(buffer->Data()), buffer->ByteLength());
  return array;
}

NDEFRecordPtr TypeConverter<NDEFRecordPtr, String>::Convert(
    const String& string) {
  NDEFRecordPtr record = NDEFRecord::New();
  record->record_type = NDEFRecordType::TEXT;
  record->media_type =
      StringView(blink::kNfcPlainTextMimeType) + blink::kNfcCharSetUTF8;
  record->data = mojo::ConvertTo<Vector<uint8_t>>(string);
  return record;
}

NDEFRecordPtr TypeConverter<NDEFRecordPtr, blink::DOMArrayBuffer*>::Convert(
    blink::DOMArrayBuffer* buffer) {
  NDEFRecordPtr record = NDEFRecord::New();
  record->record_type = NDEFRecordType::OPAQUE_RECORD;
  record->media_type = blink::kNfcOpaqueMimeType;
  record->data = mojo::ConvertTo<Vector<uint8_t>>(buffer);
  return record;
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, String>::Convert(
    const String& string) {
  NDEFMessagePtr message = NDEFMessage::New();
  message->data.push_back(NDEFRecord::From(string));
  return message;
}

base::Optional<Vector<uint8_t>>
TypeConverter<base::Optional<Vector<uint8_t>>, blink::NDEFRecordData>::Convert(
    const blink::NDEFRecordData& value) {
  if (value.IsUnrestrictedDouble()) {
    return mojo::ConvertTo<Vector<uint8_t>>(
        String::Number(value.GetAsUnrestrictedDouble()));
  }

  if (value.IsString()) {
    return mojo::ConvertTo<Vector<uint8_t>>(value.GetAsString());
  }

  // TODO(https://crbug.com/520391): Remove this duplicate code for stringifying
  // json, by reusing the same code in NDEFRecord ctor. i.e. Make users of this
  // type converter:
  // 1. first construct a NDEFRecord from the given NDEFRecordInit, the
  // NDEFRecord ctor would stringify json.
  // 2. then convert the NDEFRecord into an mojom::NDEFRecordPtr, which should
  // be just simple.
  if (value.IsDictionary()) {
    v8::Local<v8::String> jsonString;
    blink::Dictionary dictionary = value.GetAsDictionary();
    v8::Isolate* isolate = dictionary.GetIsolate();
    v8::TryCatch try_catch(isolate);

    // https://w3c.github.io/web-nfc/#mapping-json-to-ndef
    // If serialization throws, reject promise with a "SyntaxError" exception.
    if (!v8::JSON::Stringify(dictionary.V8Context(), dictionary.V8Value())
             .ToLocal(&jsonString) ||
        try_catch.HasCaught()) {
      return base::nullopt;
    }

    String string =
        blink::ToBlinkString<String>(jsonString, blink::kDoNotExternalize);
    return mojo::ConvertTo<Vector<uint8_t>>(string);
  }

  if (value.IsArrayBuffer()) {
    return mojo::ConvertTo<Vector<uint8_t>>(value.GetAsArrayBuffer());
  }

  return base::nullopt;
}

NDEFRecordPtr TypeConverter<NDEFRecordPtr, blink::NDEFRecordInit*>::Convert(
    const blink::NDEFRecordInit* record) {
  NDEFRecordPtr recordPtr = NDEFRecord::New();

  if (record->hasRecordType()) {
    recordPtr->record_type =
        blink::StringToNDEFRecordType(record->recordType());
  } else {
    recordPtr->record_type = blink::DeduceRecordTypeFromDataType(record);
  }

  // If record type is "empty", no need to set media type or data.
  // https://w3c.github.io/web-nfc/#creating-web-nfc-message
  if (recordPtr->record_type == NDEFRecordType::EMPTY)
    return recordPtr;

  switch (recordPtr->record_type) {
    case NDEFRecordType::TEXT:
    case NDEFRecordType::URL:
      SetMediaType(recordPtr, record->mediaType(),
                   blink::kNfcPlainTextMimeType);
      recordPtr->media_type = recordPtr->media_type + blink::kNfcCharSetUTF8;
      break;
    case NDEFRecordType::JSON:
      SetMediaType(recordPtr, record->mediaType(), blink::kNfcJsonMimeType);
      break;
    case NDEFRecordType::OPAQUE_RECORD:
      SetMediaType(recordPtr, record->mediaType(), blink::kNfcOpaqueMimeType);
      break;
    default:
      NOTREACHED();
      break;
  }

  auto recordData =
      mojo::ConvertTo<base::Optional<Vector<uint8_t>>>(record->data());
  // If JS object cannot be converted to uint8_t array, return null,
  // interrupt NDEFMessage conversion algorithm and reject promise with
  // SyntaxError exception.
  if (!recordData)
    return nullptr;

  recordPtr->data = recordData.value();
  return recordPtr;
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, blink::NDEFMessageInit*>::Convert(
    const blink::NDEFMessageInit* message) {
  NDEFMessagePtr messagePtr = NDEFMessage::New();
  messagePtr->url = message->url();
  messagePtr->data.resize(message->records().size());
  for (wtf_size_t i = 0; i < message->records().size(); ++i) {
    NDEFRecordPtr record = NDEFRecord::From(message->records()[i].Get());
    if (record.is_null())
      return nullptr;

    messagePtr->data[i] = std::move(record);
  }
  return messagePtr;
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, blink::DOMArrayBuffer*>::Convert(
    blink::DOMArrayBuffer* buffer) {
  NDEFMessagePtr message = NDEFMessage::New();
  message->data.push_back(NDEFRecord::From(buffer));
  return message;
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, blink::NDEFMessageSource>::Convert(
    const blink::NDEFMessageSource& message) {
  if (message.IsString())
    return NDEFMessage::From(message.GetAsString());

  if (message.IsNDEFMessageInit())
    return NDEFMessage::From(message.GetAsNDEFMessageInit());

  if (message.IsArrayBuffer())
    return NDEFMessage::From(message.GetAsArrayBuffer());

  NOTREACHED();
  return nullptr;
}

NFCPushOptionsPtr
TypeConverter<NFCPushOptionsPtr, const blink::NFCPushOptions*>::Convert(
    const blink::NFCPushOptions* pushOptions) {
  // https://w3c.github.io/web-nfc/#the-nfcpushoptions-dictionary
  // Default values for NFCPushOptions dictionary are:
  // target = 'any', timeout = Infinity, ignoreRead = true,
  // compatibility = "nfc-forum"
  NFCPushOptionsPtr pushOptionsPtr = NFCPushOptions::New();
  pushOptionsPtr->target = blink::StringToNFCPushTarget(pushOptions->target());
  pushOptionsPtr->ignore_read = pushOptions->ignoreRead();
  pushOptionsPtr->compatibility =
      blink::StringToNDEFCompatibility(pushOptions->compatibility());

  if (pushOptions->hasTimeout())
    pushOptionsPtr->timeout = pushOptions->timeout();
  else
    pushOptionsPtr->timeout = std::numeric_limits<double>::infinity();

  return pushOptionsPtr;
}

NFCReaderOptionsPtr
TypeConverter<NFCReaderOptionsPtr, const blink::NFCReaderOptions*>::Convert(
    const blink::NFCReaderOptions* watchOptions) {
  // https://w3c.github.io/web-nfc/#dom-nfcreaderoptions
  // Default values for NFCReaderOptions dictionary are:
  // url = "", recordType = null, mediaType = "", compatibility = "nfc-forum"
  NFCReaderOptionsPtr watchOptionsPtr = NFCReaderOptions::New();
  watchOptionsPtr->url = watchOptions->url();
  watchOptionsPtr->media_type = watchOptions->mediaType();
  watchOptionsPtr->compatibility =
      blink::StringToNDEFCompatibility(watchOptions->compatibility());

  if (watchOptions->hasRecordType()) {
    watchOptionsPtr->record_filter = NDEFRecordTypeFilter::New();
    watchOptionsPtr->record_filter->record_type =
        blink::StringToNDEFRecordType(watchOptions->recordType());
  }

  return watchOptionsPtr;
}

}  // namespace mojo
