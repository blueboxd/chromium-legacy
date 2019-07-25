// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_ERROR_H_

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

// TODO(leonhsl): Remove this class and create a util function
// NFCErrorTypeToDOMException() to do what Take() does now, because we do not
// use CallbackPromiseAdapter across nfc code.
class NFCError {
  STATIC_ONLY(NFCError);

 public:
  // Required by CallbackPromiseAdapter
  using WebType = const device::mojom::blink::NFCErrorType&;
  static DOMException* Take(ScriptPromiseResolver*,
                            const device::mojom::blink::NFCErrorType&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_ERROR_H_
