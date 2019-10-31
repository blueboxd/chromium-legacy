// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record_init.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

WTF::Vector<uint8_t> GetUTF8DataFromString(const String& string) {
  StringUTF8Adaptor utf8_string(string);
  WTF::Vector<uint8_t> data;
  data.Append(utf8_string.data(), utf8_string.size());
  return data;
}

// https://w3c.github.io/web-nfc/#the-ndefrecordtype-string
// Derives a formatted custom type for the external type record from |input|.
// Returns a null string for an invalid |input|.
//
// TODO(https://crbug.com/520391): Refine the validation algorithm here
// accordingly once there is a conclusion on some case-sensitive things at
// https://github.com/w3c/web-nfc/issues/331.
String ValidateCustomRecordType(const String& input) {
  static const String kOtherCharsForCustomType("()+,-:=@;$_!*'.");

  if (input.IsEmpty())
    return String();

  // Finds the separator ':'.
  wtf_size_t colon_index = input.find(':');
  if (colon_index == kNotFound)
    return String();

  // Derives the domain (FQDN) from the part before ':'.
  String left = input.Left(colon_index);
  bool success = false;
  String domain = SecurityOrigin::CanonicalizeHost(left, &success);
  if (!success || domain.IsEmpty())
    return String();

  // Validates the part after ':'.
  String right = input.Substring(colon_index + 1);
  if (right.length() == 0)
    return String();
  for (wtf_size_t i = 0; i < right.length(); i++) {
    if (!IsASCIIAlphanumeric(right[i]) &&
        !kOtherCharsForCustomType.Contains(right[i])) {
      return String();
    }
  }

  return domain + ':' + right;
}

String getDocumentLanguage(const ExecutionContext* execution_context) {
  DCHECK(execution_context);
  String document_language;
  Element* document_element =
      To<Document>(execution_context)->documentElement();
  if (document_element) {
    document_language = document_element->getAttribute(html_names::kLangAttr);
  }
  if (document_language.IsEmpty()) {
    document_language = "en";
  }
  return document_language;
}

static NDEFRecord* CreateTextRecord(const ExecutionContext* execution_context,
                                    const String& media_type,
                                    const String& encoding,
                                    const String& lang,
                                    const ScriptValue& data,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-string-to-ndef
  if (data.IsEmpty() ||
      !(data.V8Value()->IsString() || data.V8Value()->IsArrayBuffer() ||
        data.V8Value()->IsArrayBufferView())) {
    exception_state.ThrowTypeError(
        "The data for 'text' NDEFRecords must be a String or a BufferSource.");
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

  // Set language to lang if it exists, or the document element's lang
  // attribute, or 'en'.
  String language = lang;
  if (execution_context && language.IsEmpty()) {
    language = getDocumentLanguage(execution_context);
  }

  // Bits 0 to 5 define the length of the language tag
  // https://w3c.github.io/web-nfc/#text-record
  if (language.length() > 63) {
    exception_state.ThrowTypeError("Lang length cannot be stored in 6 bit.");
    return nullptr;
  }

  String encoding_label = !encoding.IsEmpty() ? encoding : "utf-8";
  if (encoding_label != "utf-8" && encoding_label != "utf-16" &&
      encoding_label != "utf-16be" && encoding_label != "utf-16le") {
    exception_state.ThrowTypeError(
        "Encoding must be either \"utf-8\", \"utf-16\", \"utf-16be\", or "
        "\"utf-16le\".");
    return nullptr;
  }

  WTF::Vector<uint8_t> bytes;
  if (data.V8Value()->IsString()) {
    String text = ToCoreString(data.V8Value().As<v8::String>());
    StringUTF8Adaptor utf8_string(text);
    bytes.Append(utf8_string.data(), utf8_string.size());
  } else if (data.V8Value()->IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer =
        V8ArrayBuffer::ToImpl(data.V8Value().As<v8::Object>());
    bytes.Append(static_cast<uint8_t*>(array_buffer->Data()),
                 array_buffer->ByteLength());
  } else if (data.V8Value()->IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view =
        V8ArrayBufferView::ToImpl(data.V8Value().As<v8::Object>());
    bytes.Append(
        static_cast<uint8_t*>(array_buffer_view->View()->BaseAddress()),
        array_buffer_view->View()->ByteLength());
  } else {
    DCHECK(false);
  }

  return MakeGarbageCollected<NDEFRecord>("text", mime_type, encoding_label,
                                          language, std::move(bytes));
}

// Create a 'url' record or an 'absolute-url' record.
static NDEFRecord* CreateUrlRecord(const String& record_type,
                                   const String& media_type,
                                   const ScriptValue& data,
                                   ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-url-to-ndef
  if (data.IsEmpty() || !data.V8Value()->IsString()) {
    exception_state.ThrowTypeError(
        "The data for url NDEFRecord must be a String.");
    return nullptr;
  }

  // No need to check mediaType according to the spec.
  String url = ToCoreString(data.V8Value().As<v8::String>());
  if (!KURL(NullURL(), url).IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Cannot parse data for url record.");
    return nullptr;
  }
  return MakeGarbageCollected<NDEFRecord>(record_type, media_type,
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

static NDEFRecord* CreateExternalRecord(const String& custom_type,
                                        const ScriptValue& data,
                                        ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#dfn-map-external-data-to-ndef
  if (data.IsEmpty() || !data.V8Value()->IsArrayBuffer()) {
    exception_state.ThrowTypeError(
        "The data for external type NDEFRecord must be an ArrayBuffer.");
    return nullptr;
  }

  DOMArrayBuffer* array_buffer =
      V8ArrayBuffer::ToImpl(data.V8Value().As<v8::Object>());
  WTF::Vector<uint8_t> bytes;
  bytes.Append(static_cast<uint8_t*>(array_buffer->Data()),
               array_buffer->ByteLength());
  return MakeGarbageCollected<NDEFRecord>(
      custom_type, "application/octet-stream", std::move(bytes));
}

}  // namespace

// static
NDEFRecord* NDEFRecord::Create(const ExecutionContext* execution_context,
                               const NDEFRecordInit* init,
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
    return CreateTextRecord(execution_context, init->mediaType(),
                            init->encoding(), init->lang(), init->data(),
                            exception_state);
  } else if (record_type == "url" || record_type == "absolute-url") {
    return CreateUrlRecord(record_type, init->mediaType(), init->data(),
                           exception_state);
  } else if (record_type == "json") {
    return CreateJsonRecord(init->mediaType(), init->data(), exception_state);
  } else if (record_type == "opaque") {
    return CreateOpaqueRecord(init->mediaType(), init->data(), exception_state);
  } else if (record_type == "smart-poster") {
    // TODO(https://crbug.com/520391): Support creating smart-poster records.
    exception_state.ThrowTypeError("smart-poster type is not supported yet");
    return nullptr;
  } else {
    String formated_type = ValidateCustomRecordType(record_type);
    if (!formated_type.IsNull())
      return CreateExternalRecord(formated_type, init->data(), exception_state);
  }

    exception_state.ThrowTypeError("Unknown NDEFRecord type.");
    return nullptr;
}

NDEFRecord::NDEFRecord(const String& record_type,
                       const String& media_type,
                       WTF::Vector<uint8_t> data)
    : record_type_(record_type),
      media_type_(media_type),
      payload_data_(std::move(data)) {}

NDEFRecord::NDEFRecord(const String& record_type,
                       const String& media_type,
                       const String& encoding,
                       const String& lang,
                       WTF::Vector<uint8_t> data)
    : record_type_(record_type),
      media_type_(media_type),
      encoding_(encoding),
      lang_(lang),
      payload_data_(std::move(data)) {}

NDEFRecord::NDEFRecord(const ExecutionContext* execution_context,
                       const String& text)
    : record_type_("text"),
      media_type_("text/plain;charset=UTF-8"),
      encoding_("utf-8"),
      lang_(getDocumentLanguage(execution_context)),
      payload_data_(GetUTF8DataFromString(text)) {}

NDEFRecord::NDEFRecord(DOMArrayBuffer* array_buffer)
    : record_type_("opaque"), media_type_("application/octet-stream") {
  payload_data_.Append(static_cast<uint8_t*>(array_buffer->Data()),
                       array_buffer->ByteLength());
}

NDEFRecord::NDEFRecord(const device::mojom::blink::NDEFRecord& record)
    : record_type_(record.record_type),
      media_type_(record.media_type),
      id_(record.id),
      encoding_(record.encoding),
      lang_(record.lang),
      payload_data_(record.data) {}

const String& NDEFRecord::recordType() const {
  return record_type_;
}

const String& NDEFRecord::mediaType() const {
  return media_type_;
}

const String& NDEFRecord::id() const {
  return id_;
}

const String& NDEFRecord::encoding() const {
  return encoding_;
}

const String& NDEFRecord::lang() const {
  return lang_;
}

DOMDataView* NDEFRecord::data() const {
  DOMArrayBuffer* dom_buffer =
      DOMArrayBuffer::Create(payload_data_.data(), payload_data_.size());
  return DOMDataView::Create(dom_buffer, 0, payload_data_.size());
}

String NDEFRecord::text() const {
  if (record_type_ == "empty")
    return String();

  // TODO(https://crbug.com/520391): Support utf-16 decoding for 'TEXT' record
  // as described at
  // http://w3c.github.io/web-nfc/#dfn-convert-ndefrecord-payloaddata-bytes.
  return String::FromUTF8WithLatin1Fallback(payload_data_.data(),
                                            payload_data_.size());
}

DOMArrayBuffer* NDEFRecord::arrayBuffer() const {
  if (record_type_ == "empty" || record_type_ == "text" ||
      record_type_ == "url" || record_type_ == "absolute-url") {
    return nullptr;
  }
  DCHECK(record_type_ == "json" || record_type_ == "opaque" ||
         !ValidateCustomRecordType(record_type_).IsNull());

  return DOMArrayBuffer::Create(payload_data_.data(), payload_data_.size());
}

ScriptValue NDEFRecord::json(ScriptState* script_state,
                             ExceptionState& exception_state) const {
  if (record_type_ == "empty" || record_type_ == "text" ||
      record_type_ == "url" || record_type_ == "absolute-url") {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  DCHECK(record_type_ == "json" || record_type_ == "opaque" ||
         !ValidateCustomRecordType(record_type_).IsNull());

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> json_object =
      FromJSONString(script_state->GetIsolate(), script_state->GetContext(),
                     String::FromUTF8WithLatin1Fallback(payload_data_.data(),
                                                        payload_data_.size()),
                     exception_state);
  if (exception_state.HadException())
    return ScriptValue::CreateNull(script_state->GetIsolate());
  return ScriptValue(script_state->GetIsolate(), json_object);
}

const WTF::Vector<uint8_t>& NDEFRecord::payloadData() const {
  return payload_data_;
}

void NDEFRecord::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
