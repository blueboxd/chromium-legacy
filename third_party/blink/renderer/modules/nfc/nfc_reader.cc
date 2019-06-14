// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_reader.h"

#include <utility>

namespace blink {

NFCReader::NFCReader(device::mojom::blink::NFCReaderOptionsPtr options)
    : options_(std::move(options)) {}

void NFCReader::Trace(blink::Visitor* visitor) {}

// An reading event with NDEF message.
void NFCReader::OnMessage(const device::mojom::blink::NDEFMessage& message) {}

// An reading error has occurred.
void NFCReader::OnError(const device::mojom::blink::NFCError& error) {}

}  // namespace blink
