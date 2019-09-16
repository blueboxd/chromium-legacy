// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

using device::mojom::blink::NDEFCompatibility;
using device::mojom::blink::NFCPushTarget;

namespace blink {

size_t GetNDEFMessageSize(const device::mojom::blink::NDEFMessage& message) {
  size_t message_size = message.url.CharactersSizeInBytes();
  for (wtf_size_t i = 0; i < message.data.size(); ++i) {
    message_size += message.data[i]->media_type.CharactersSizeInBytes();
    message_size += message.data[i]->data.size();
  }
  return message_size;
}

bool SetNDEFMessageURL(const String& origin,
                       device::mojom::blink::NDEFMessage* message) {
  KURL origin_url(origin);

  if (!message->url.IsEmpty() && origin_url.CanSetPathname()) {
    origin_url.SetPath(message->url);
  }

  message->url = origin_url;
  return origin_url.IsValid();
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
          DOMExceptionCode::kNotAllowedError, "NFC operation not allowed.");
    case device::mojom::blink::NFCErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "No NFC adapter or cannot establish connection.");
    case device::mojom::blink::NFCErrorType::NOT_READABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError, "NFC is not enabled.");
    case device::mojom::blink::NFCErrorType::NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError,
          "Provided watch id cannot be found.");
    case device::mojom::blink::NFCErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSyntaxError, "Invalid NFC message was provided.");
    case device::mojom::blink::NFCErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "The NFC operation was cancelled.");
    case device::mojom::blink::NFCErrorType::TIMER_EXPIRED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kTimeoutError,
                                                "NFC operation has timed out.");
    case device::mojom::blink::NFCErrorType::CANNOT_CANCEL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNoModificationAllowedError,
          "NFC operation cannot be cancelled.");
    case device::mojom::blink::NFCErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "NFC data transfer error has occurred.");
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return nullptr;
}

}  // namespace blink
