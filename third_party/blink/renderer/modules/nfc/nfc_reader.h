// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_READER_H_

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class MODULES_EXPORT NFCReader : public GarbageCollectedMixin {
 public:
  explicit NFCReader(device::mojom::blink::NFCReaderOptionsPtr options);
  virtual ~NFCReader() {}

  void Trace(blink::Visitor*) override;

  // Get reader options.
  const device::mojom::blink::NFCReaderOptions& options() const {
    return *options_;
  }

  // An reading event with NDEF message.
  virtual void OnMessage(const device::mojom::blink::NDEFMessage& message);

  // An reading error has occurred.
  virtual void OnError(const device::mojom::blink::NFCError& error);

 private:
  device::mojom::blink::NFCReaderOptionsPtr options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_READER_H_
