// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_reader.h"

#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reader_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reading_event.h"

namespace blink {

// static
NFCReader* NFCReader::Create(ExecutionContext* context,
                             NFCReaderOptions* options) {
  return MakeGarbageCollected<NFCReader>(context, options);
}

NFCReader::NFCReader(ExecutionContext* context, NFCReaderOptions* options)
    : ContextLifecycleObserver(context), options_(options) {}

NFCReader::~NFCReader() = default;

const AtomicString& NFCReader::InterfaceName() const {
  return event_target_names::kNFCReader;
}

ExecutionContext* NFCReader::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool NFCReader::HasPendingActivity() const {
  return GetExecutionContext() && GetNfcProxy()->IsReading(this) &&
         HasEventListeners();
}

void NFCReader::start() {
  if (!CheckSecurity())
    return;
  GetNfcProxy()->StartReading(this);
}

void NFCReader::stop() {
  if (!CheckSecurity())
    return;
  GetNfcProxy()->StopReading(this);
}

void NFCReader::Trace(blink::Visitor* visitor) {
  visitor->Trace(options_);
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void NFCReader::OnReading(const String& serial_number,
                          const device::mojom::blink::NDEFMessage& message) {
  DCHECK(GetNfcProxy()->IsReading(this));
  DispatchEvent(*MakeGarbageCollected<NFCReadingEvent>(
      event_type_names::kReading, serial_number,
      MakeGarbageCollected<NDEFMessage>(message)));
}

void NFCReader::OnError(device::mojom::blink::NFCErrorType error) {
  DispatchEvent(*MakeGarbageCollected<NFCErrorEvent>(
      event_type_names::kError, NFCError::Take(nullptr, error)));
}

void NFCReader::ContextDestroyed(ExecutionContext*) {
  GetNfcProxy()->StopReading(this);
}

bool NFCReader::CheckSecurity() {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context)
    return false;
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!To<Document>(execution_context)->IsInMainFrame()) {
    DispatchEvent(*MakeGarbageCollected<NFCErrorEvent>(
        event_type_names::kError,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           kNfcAccessInNonTopFrame)));
    return false;
  }
  return true;
}

NFCProxy* NFCReader::GetNfcProxy() const {
  DCHECK(GetExecutionContext());
  return NFCProxy::From(*To<Document>(GetExecutionContext()));
}

}  // namespace blink
