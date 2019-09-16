// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record_init.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

WTF::Vector<uint8_t> GetUTF8DataFromString(const String& string) {
  StringUTF8Adaptor utf8_string(string);
  WTF::Vector<uint8_t> data;
  data.Append(utf8_string.data(), utf8_string.size());
  return data;
}

static NDEFRecord* CreateTextRecord(const String& media_type,
                                    const ScriptValue& data,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-string-to-ndef
  if (data.IsEmpty() || !data.V8Value()->IsString()) {
    exception_state.ThrowTypeError(
        "The data for 'text' NDEFRecords must be a String.");
    return nullptr;
  }

  // ExtractMIMETypeFromMediaType() ignores parameters of the MIME type.
  String mime_type = ExtractMIMETypeFromMediaType(AtomicString(media_type));

  // TODO(https://crbug.com/520391): Step 2-5, parse a MIME type on |media_type|
  // to get 'lang' and 'charset' parameters. Now we ignore them and the embedder
  // always uses "lang=en-US;charset=UTF-8" when pushing the record to a NFC
  // tag.
  if (mime_type.IsEmpty()) {
    mime_type = "text/plain";
  } else if (!mime_type.StartsWithIgnoringASCIICase("text/")) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid media type for 'text' record.");
    return nullptr;
  }

  String text = ToCoreString(data.V8Value().As<v8::String>());
  return MakeGarbageCollected<NDEFRecord>("text", mime_type,
                                          GetUTF8DataFromString(text));
}

static NDEFRecord* CreateUrlRecord(const String& media_type,
                                   const ScriptValue& data,
                                   ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-url-to-ndef
  if (data.IsEmpty() || !data.V8Value()->IsString()) {
    exception_state.ThrowTypeError(
        "The data for 'url' NDEFRecord must be a String.");
    return nullptr;
  }

  // No need to check mediaType according to the spec.
  String url = ToCoreString(data.V8Value().As<v8::String>());
  if (!KURL(NullURL(), url).IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Cannot parse data for 'url' record.");
    return nullptr;
  }
  return MakeGarbageCollected<NDEFRecord>("url", media_type,
                                          GetUTF8DataFromString(url));
}

static NDEFRecord* CreateJsonRecord(const String& media_type,
                                    const ScriptValue& data,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-json-to-ndef
  if (data.IsEmpty()) {
    exception_state.ThrowTypeError(
        "The data for 'json' NDEFRecord is missing.");
    return nullptr;
  }

  // ExtractMIMETypeFromMediaType() ignores parameters of the MIME type.
  String mime_type = ExtractMIMETypeFromMediaType(AtomicString(media_type));
  if (mime_type.IsEmpty()) {
    mime_type = "application/json";
  } else if (mime_type != "application/json" && mime_type != "text/json" &&
             !mime_type.EndsWithIgnoringASCIICase("+json")) {
    // According to https://mimesniff.spec.whatwg.org/#json-mime-type, a JSON
    // MIME type is any MIME type whose subtype ends in "+json" or whose
    // essence is "application/json" or "text/json".
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid media type for 'json' record.");
    return nullptr;
  }

  // Serialize JSON to bytes, rethrow any exceptions.
  v8::Local<v8::String> jsonString;
  v8::TryCatch try_catch(data.GetIsolate());
  if (!v8::JSON::Stringify(data.GetIsolate()->GetCurrentContext(),
                           data.V8Value())
           .ToLocal(&jsonString)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return MakeGarbageCollected<NDEFRecord>(
      "json", mime_type,
      GetUTF8DataFromString(
          ToBlinkString<String>(jsonString, kDoNotExternalize)));
}

static NDEFRecord* CreateOpaqueRecord(const String& media_type,
                                      const ScriptValue& data,
                                      ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-binary-data-to-ndef
  if (data.IsEmpty() || !data.V8Value()->IsArrayBuffer()) {
    exception_state.ThrowTypeError(
        "The data for 'opaque' NDEFRecord must be an ArrayBuffer.");
    return nullptr;
  }

  // ExtractMIMETypeFromMediaType() ignores parameters of the MIME type.
  String mime_type = ExtractMIMETypeFromMediaType(AtomicString(media_type));
  if (mime_type.IsEmpty()) {
    mime_type = "application/octet-stream";
  }
  DOMArrayBuffer* array_buffer =
      V8ArrayBuffer::ToImpl(data.V8Value().As<v8::Object>());
  WTF::Vector<uint8_t> bytes;
  bytes.Append(static_cast<uint8_t*>(array_buffer->Data()),
               array_buffer->ByteLength());
  return MakeGarbageCollected<NDEFRecord>("opaque", mime_type,
                                          std::move(bytes));
}

}  // namespace

// static
NDEFRecord* NDEFRecord::Create(const NDEFRecordInit* init,
                               ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#creating-web-nfc-message
  String record_type;
  if (!init->hasRecordType()) {
    if (!init->hasData()) {
      exception_state.ThrowTypeError("The record has neither type nor data.");
      return nullptr;
    }
    v8::Local<v8::Value> data = init->data().V8Value();
    if (data->IsString()) {
      record_type = "text";
    } else if (data->IsArrayBuffer()) {
      record_type = "opaque";
    } else {
      record_type = "json";
    }
  } else {
    record_type = init->recordType();
  }

  if (record_type == "empty") {
    // https://w3c.github.io/web-nfc/#mapping-empty-record-to-ndef
    // If record type is "empty", no need to set media type and data.
    return MakeGarbageCollected<NDEFRecord>(record_type, String(),
                                            WTF::Vector<uint8_t>());
  } else if (record_type == "text") {
    return CreateTextRecord(init->mediaType(), init->data(), exception_state);
  } else if (record_type == "url") {
    return CreateUrlRecord(init->mediaType(), init->data(), exception_state);
  } else if (record_type == "json") {
    return CreateJsonRecord(init->mediaType(), init->data(), exception_state);
  } else if (record_type == "opaque") {
    return CreateOpaqueRecord(init->mediaType(), init->data(), exception_state);
  } else {
    // TODO(https://crbug.com/520391): Support creating smart-poster and
    // external type records.
    exception_state.ThrowTypeError("Unknown NDEFRecord type.");
    return nullptr;
  }
}

NDEFRecord::NDEFRecord(const String& record_type,
                       const String& media_type,
                       WTF::Vector<uint8_t> data)
    : record_type_(record_type),
      media_type_(media_type),
      data_(std::move(data)) {}

NDEFRecord::NDEFRecord(const String& text)
    : record_type_("text"),
      media_type_("text/plain;charset=UTF-8"),
      data_(GetUTF8DataFromString(text)) {}

NDEFRecord::NDEFRecord(DOMArrayBuffer* array_buffer)
    : record_type_("opaque"), media_type_("application/octet-stream") {
  data_.Append(static_cast<uint8_t*>(array_buffer->Data()),
               array_buffer->ByteLength());
}

NDEFRecord::NDEFRecord(const device::mojom::blink::NDEFRecord& record)
    : record_type_(record.record_type),
      media_type_(record.media_type),
      data_(record.data) {}

const String& NDEFRecord::recordType() const {
  return record_type_;
}

const String& NDEFRecord::mediaType() const {
  return media_type_;
}

String NDEFRecord::toText() const {
  if (record_type_ == "empty")
    return String();

  // TODO(https://crbug.com/520391): Support utf-16 decoding for 'TEXT' record
  // as described at
  // http://w3c.github.io/web-nfc/#dfn-convert-ndefrecord-payloaddata-bytes.
  return String::FromUTF8WithLatin1Fallback(data_.data(), data_.size());
}

DOMArrayBuffer* NDEFRecord::toArrayBuffer() const {
  if (record_type_ != "json" && record_type_ != "opaque") {
    return nullptr;
  }

  return DOMArrayBuffer::Create(data_.data(), data_.size());
}

ScriptValue NDEFRecord::toJSON(ScriptState* script_state,
                               ExceptionState& exception_state) const {
  if (record_type_ != "json" && record_type_ != "opaque") {
    return ScriptValue::CreateNull(script_state);
  }

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> json_object = FromJSONString(
      script_state->GetIsolate(), script_state->GetContext(),
      String::FromUTF8WithLatin1Fallback(data_.data(), data_.size()),
      exception_state);
  if (exception_state.HadException())
    return ScriptValue::CreateNull(script_state);
  return ScriptValue(script_state, json_object);
}

const WTF::Vector<uint8_t>& NDEFRecord::data() const {
  return data_;
}

void NDEFRecord::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
