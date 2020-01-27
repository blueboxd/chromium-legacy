// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_array_buffer_view_or_ndef_message_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_record_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

using NDEFRecordDataSource =
    StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

namespace {

WTF::Vector<uint8_t> GetUTF8DataFromString(const String& string) {
  StringUTF8Adaptor utf8_string(string);
  WTF::Vector<uint8_t> data;
  data.Append(utf8_string.data(), utf8_string.size());
  return data;
}

bool IsBufferSource(const NDEFRecordDataSource& data) {
  return data.IsArrayBuffer() || data.IsArrayBufferView();
}

bool GetBytesOfBufferSource(const NDEFRecordDataSource& buffer_source,
                            WTF::Vector<uint8_t>* target,
                            ExceptionState& exception_state) {
  DCHECK(IsBufferSource(buffer_source));
  uint8_t* data;
  size_t data_length;
  if (buffer_source.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = buffer_source.GetAsArrayBuffer();
    data = reinterpret_cast<uint8_t*>(array_buffer->Data());
    data_length = array_buffer->ByteLengthAsSizeT();
  } else if (buffer_source.IsArrayBufferView()) {
    const DOMArrayBufferView* array_buffer_view =
        buffer_source.GetAsArrayBufferView().View();
    data = reinterpret_cast<uint8_t*>(array_buffer_view->BaseAddress());
    data_length = array_buffer_view->byteLengthAsSizeT();
  } else {
    NOTREACHED();
    return false;
  }
  wtf_size_t checked_length;
  if (!base::CheckedNumeric<wtf_size_t>(data_length)
           .AssignIfValid(&checked_length)) {
    exception_state.ThrowRangeError(
        "The provided buffer source exceeds the maximum supported length");
    return false;
  }
  target->Append(data, checked_length);
  return true;
}

// https://w3c.github.io/web-nfc/#dfn-validate-external-type
// Validates |input| as an external type.
bool IsValidExternalType(const String& input) {
  static const String kOtherCharsForCustomType(":!()+,-=@;$_*'.");

  // Ensure |input| is an ASCII string.
  if (!input.ContainsOnlyASCIIOrEmpty())
    return false;

  // As all characters in |input| is ASCII, limiting its length within 255 just
  // limits the length of its utf-8 encoded bytes we finally write into the
  // record payload.
  if (input.IsEmpty() || input.length() > 255)
    return false;

  // Finds the first occurrence of ':'.
  wtf_size_t colon_index = input.find(':');
  if (colon_index == kNotFound)
    return false;

  // Validates the domain (the part before ':').
  String domain = input.Left(colon_index);
  if (domain.IsEmpty())
    return false;
  // TODO(https://crbug.com/520391): Validate |domain|.

  // Validates the type (the part after ':').
  String type = input.Substring(colon_index + 1);
  if (type.IsEmpty())
    return false;
  for (wtf_size_t i = 0; i < type.length(); i++) {
    if (!IsASCIIAlphanumeric(type[i]) &&
        !kOtherCharsForCustomType.Contains(type[i])) {
      return false;
    }
  }

  return true;
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

static NDEFRecord* CreateTextRecord(const String& id,
                                    const ExecutionContext* execution_context,
                                    const String& encoding,
                                    const String& lang,
                                    const NDEFRecordDataSource& data,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-string-to-ndef
  if (!(data.IsString() || IsBufferSource(data))) {
    exception_state.ThrowTypeError(
        "The data for 'text' NDEFRecords must be a String or a BufferSource.");
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
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Lang length cannot be stored in 6 bit.");
    return nullptr;
  }

  String encoding_label = encoding.IsNull() ? "utf-8" : encoding;
  WTF::Vector<uint8_t> bytes;
  if (data.IsString()) {
    if (encoding_label != "utf-8") {
      exception_state.ThrowTypeError(
          "A DOMString data source is always encoded as \"utf-8\" so other "
          "encodings are not allowed.");
      return nullptr;
    }
    StringUTF8Adaptor utf8_string(data.GetAsString());
    bytes.Append(utf8_string.data(), utf8_string.size());
  } else {
    DCHECK(IsBufferSource(data));
    if (encoding_label != "utf-8" && encoding_label != "utf-16" &&
        encoding_label != "utf-16be" && encoding_label != "utf-16le") {
      exception_state.ThrowTypeError(
          "Encoding must be either \"utf-8\", \"utf-16\", \"utf-16be\", or "
          "\"utf-16le\".");
      return nullptr;
    }
    if (!GetBytesOfBufferSource(data, &bytes, exception_state)) {
      return nullptr;
    }
  }

  return MakeGarbageCollected<NDEFRecord>(id, encoding_label, language,
                                          std::move(bytes));
}

// Create a 'url' record or an 'absolute-url' record.
static NDEFRecord* CreateUrlRecord(const String& record_type,
                                   const String& id,
                                   const NDEFRecordDataSource& data,
                                   ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-url-to-ndef
  if (!data.IsString()) {
    exception_state.ThrowTypeError(
        "The data for url NDEFRecord must be a String.");
    return nullptr;
  }

  // No need to check mediaType according to the spec.
  String url = data.GetAsString();
  if (!KURL(NullURL(), url).IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Cannot parse data for url record.");
    return nullptr;
  }
  return MakeGarbageCollected<NDEFRecord>(
      device::mojom::NDEFRecordTypeCategory::kStandardized, record_type, id,
      GetUTF8DataFromString(url));
}

static NDEFRecord* CreateMimeRecord(const String& id,
                                    const NDEFRecordDataSource& data,
                                    const String& media_type,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-binary-data-to-ndef
  if (!IsBufferSource(data)) {
    exception_state.ThrowTypeError(
        "The data for 'mime' NDEFRecord must be a BufferSource.");
    return nullptr;
  }

  WTF::Vector<uint8_t> bytes;
  if (!GetBytesOfBufferSource(data, &bytes, exception_state)) {
    return nullptr;
  }

  // ExtractMIMETypeFromMediaType() ignores parameters of the MIME type.
  String mime_type = ExtractMIMETypeFromMediaType(AtomicString(media_type));
  if (mime_type.IsEmpty()) {
    mime_type = "application/octet-stream";
  }

  return MakeGarbageCollected<NDEFRecord>(id, mime_type, bytes);
}

static NDEFRecord* CreateUnknownRecord(const String& id,
                                       const NDEFRecordDataSource& data,
                                       ExceptionState& exception_state) {
  if (!IsBufferSource(data)) {
    exception_state.ThrowTypeError(
        "The data for 'unknown' NDEFRecord must be a BufferSource.");
    return nullptr;
  }

  WTF::Vector<uint8_t> bytes;
  if (!GetBytesOfBufferSource(data, &bytes, exception_state)) {
    return nullptr;
  }
  return MakeGarbageCollected<NDEFRecord>(
      device::mojom::NDEFRecordTypeCategory::kStandardized, "unknown", id,
      bytes);
}

static NDEFRecord* CreateExternalRecord(
    const ExecutionContext* execution_context,
    const String& record_type,
    const String& id,
    const NDEFRecordDataSource& data,
    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#dfn-map-external-data-to-ndef
  if (IsBufferSource(data)) {
    WTF::Vector<uint8_t> bytes;
    if (!GetBytesOfBufferSource(data, &bytes, exception_state)) {
      return nullptr;
    }
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::NDEFRecordTypeCategory::kExternal, record_type, id,
        bytes);
  } else if (data.IsNDEFMessageInit()) {
    NDEFMessage* payload_message = NDEFMessage::Create(
        execution_context, data.GetAsNDEFMessageInit(), exception_state);
    if (exception_state.HadException())
      return nullptr;
    DCHECK(payload_message);
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::NDEFRecordTypeCategory::kExternal, record_type, id,
        payload_message);
  }

  exception_state.ThrowTypeError(
      "The data for external type NDEFRecord must be a BufferSource or an "
      "NDEFMessageInit.");
  return nullptr;
}

}  // namespace

// static
NDEFRecord* NDEFRecord::Create(const ExecutionContext* execution_context,
                               const NDEFRecordInit* init,
                               ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#creating-ndef-record

  // NDEFRecordInit#recordType is a required field.
  DCHECK(init->hasRecordType());
  const String& record_type = init->recordType();

  // https://w3c.github.io/web-nfc/#dom-ndefrecordinit-mediatype
  if (init->hasMediaType() && record_type != "mime") {
    exception_state.ThrowTypeError(
        "NDEFRecordInit#mediaType is only applicable for 'mime' records.");
    return nullptr;
  }

  // https://w3c.github.io/web-nfc/#dfn-map-empty-record-to-ndef
  if (init->hasId() && record_type == "empty") {
    exception_state.ThrowTypeError(
        "NDEFRecordInit#id is not applicable for 'empty' records.");
    return nullptr;
  }

  if (record_type == "empty") {
    // https://w3c.github.io/web-nfc/#mapping-empty-record-to-ndef
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::NDEFRecordTypeCategory::kStandardized, record_type,
        init->id(), WTF::Vector<uint8_t>());
  } else if (record_type == "text") {
    return CreateTextRecord(init->id(), execution_context, init->encoding(),
                            init->lang(), init->data(), exception_state);
  } else if (record_type == "url" || record_type == "absolute-url") {
    return CreateUrlRecord(record_type, init->id(), init->data(),
                           exception_state);
  } else if (record_type == "mime") {
    return CreateMimeRecord(init->id(), init->data(), init->mediaType(),
                            exception_state);
  } else if (record_type == "unknown") {
    return CreateUnknownRecord(init->id(), init->data(), exception_state);
  } else if (record_type == "smart-poster") {
    // TODO(https://crbug.com/520391): Support creating smart-poster records.
    exception_state.ThrowTypeError("smart-poster type is not supported yet");
    return nullptr;
  } else if (IsValidExternalType(record_type)) {
    return CreateExternalRecord(execution_context, record_type, init->id(),
                                init->data(), exception_state);
  } else {
    // TODO(https://crbug.com/520391): Support local type records.
  }

  exception_state.ThrowTypeError("Invalid NDEFRecord type.");
  return nullptr;
}

NDEFRecord::NDEFRecord(device::mojom::NDEFRecordTypeCategory category,
                       const String& record_type,
                       const String& id,
                       WTF::Vector<uint8_t> data)
    : category_(category),
      record_type_(record_type),
      id_(id),
      payload_data_(std::move(data)) {
  DCHECK_EQ(category_ == device::mojom::NDEFRecordTypeCategory::kExternal,
            IsValidExternalType(record_type_));
}

NDEFRecord::NDEFRecord(device::mojom::NDEFRecordTypeCategory category,
                       const String& record_type,
                       const String& id,
                       NDEFMessage* payload_message)
    : category_(category),
      record_type_(record_type),
      id_(id),
      payload_message_(payload_message) {
  DCHECK_EQ(category_ == device::mojom::NDEFRecordTypeCategory::kExternal,
            IsValidExternalType(record_type_));
  DCHECK(record_type_ == "smart-poster" ||
         category_ == device::mojom::NDEFRecordTypeCategory::kExternal);
}

NDEFRecord::NDEFRecord(const String& id,
                       const String& encoding,
                       const String& lang,
                       WTF::Vector<uint8_t> data)
    : category_(device::mojom::NDEFRecordTypeCategory::kStandardized),
      record_type_("text"),
      id_(id),
      encoding_(encoding),
      lang_(lang),
      payload_data_(std::move(data)) {}

NDEFRecord::NDEFRecord(const ExecutionContext* execution_context,
                       const String& text)
    : category_(device::mojom::NDEFRecordTypeCategory::kStandardized),
      record_type_("text"),
      encoding_("utf-8"),
      lang_(getDocumentLanguage(execution_context)),
      payload_data_(GetUTF8DataFromString(text)) {}

NDEFRecord::NDEFRecord(const String& id,
                       const String& media_type,
                       WTF::Vector<uint8_t> data)
    : category_(device::mojom::NDEFRecordTypeCategory::kStandardized),
      record_type_("mime"),
      id_(id),
      media_type_(media_type),
      payload_data_(std::move(data)) {}

NDEFRecord::NDEFRecord(const device::mojom::blink::NDEFRecord& record)
    : category_(record.category),
      record_type_(record.record_type),
      id_(record.id),
      media_type_(record.media_type),
      encoding_(record.encoding),
      lang_(record.lang),
      payload_data_(record.data),
      payload_message_(
          record.payload_message
              ? MakeGarbageCollected<NDEFMessage>(*record.payload_message)
              : nullptr) {
  DCHECK_NE(record_type_ == "mime", media_type_.IsNull());
  DCHECK_EQ(category_ == device::mojom::NDEFRecordTypeCategory::kExternal,
            IsValidExternalType(record_type_));
}

const String& NDEFRecord::mediaType() const {
  DCHECK_NE(record_type_ == "mime", media_type_.IsNull());
  return media_type_;
}

DOMDataView* NDEFRecord::data() const {
  // Step 4 in https://w3c.github.io/web-nfc/#dfn-parse-an-ndef-record
  if (record_type_ == "empty") {
    DCHECK(payload_data_.IsEmpty());
    return nullptr;
  }
  DOMArrayBuffer* dom_buffer =
      DOMArrayBuffer::Create(payload_data_.data(), payload_data_.size());
  return DOMDataView::Create(dom_buffer, 0, payload_data_.size());
}

// https://w3c.github.io/web-nfc/#dfn-convert-ndefrecord-payloaddata-bytes
base::Optional<HeapVector<Member<NDEFRecord>>> NDEFRecord::toRecords(
    ExceptionState& exception_state) const {
  if (record_type_ != "smart-poster" &&
      category_ != device::mojom::NDEFRecordTypeCategory::kExternal) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Only smart-poster records and external type records could have a ndef "
        "message as payload.");
    return base::nullopt;
  }

  if (!payload_message_)
    return base::nullopt;

  return payload_message_->records();
}

void NDEFRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(payload_message_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
