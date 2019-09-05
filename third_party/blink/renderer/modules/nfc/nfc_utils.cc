// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

using device::mojom::blink::NDEFCompatibility;
using device::mojom::blink::NDEFRecordType;
using device::mojom::blink::NFCPushTarget;

namespace blink {

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

DOMException* NFCErrorTypeToDOMException(
    device::mojom::blink::NFCErrorType error_type) {
  switch (error_type) {
    case device::mojom::blink::NFCErrorType::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, kNfcNotAllowed);
    case device::mojom::blink::NFCErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, kNfcNotSupported);
    case device::mojom::blink::NFCErrorType::NOT_READABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError, kNfcNotReadable);
    case device::mojom::blink::NFCErrorType::NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, kNfcWatchIdNotFound);
    case device::mojom::blink::NFCErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                kNfcInvalidMsg);
    case device::mojom::blink::NFCErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                kNfcCancelled);
    case device::mojom::blink::NFCErrorType::TIMER_EXPIRED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kTimeoutError,
                                                kNfcTimeout);
    case device::mojom::blink::NFCErrorType::CANNOT_CANCEL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNoModificationAllowedError,
          kNfcNoModificationAllowed);
    case device::mojom::blink::NFCErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                kNfcDataTransferError);
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return nullptr;
}

}  // namespace blink
