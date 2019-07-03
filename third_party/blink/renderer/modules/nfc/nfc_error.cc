// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_error.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

using device::mojom::blink::NFCErrorType;

namespace blink {

DOMException* NFCError::Take(ScriptPromiseResolver*,
                             const NFCErrorType& error_type) {
  switch (error_type) {
    case NFCErrorType::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, kNfcNotAllowed);
    case NFCErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, kNfcNotSupported);
    case NFCErrorType::NOT_READABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError, kNfcNotReadable);
    case NFCErrorType::NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, kNfcWatchIdNotFound);
    case NFCErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                kNfcInvalidMsg);
    case NFCErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                kNfcCancelled);
    case NFCErrorType::TIMER_EXPIRED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kTimeoutError,
                                                kNfcTimeout);
    case NFCErrorType::CANNOT_CANCEL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNoModificationAllowedError,
          kNfcNoModificationAllowed);
    case NFCErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                kNfcDataTransferError);
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return nullptr;
}

}  // namespace blink
