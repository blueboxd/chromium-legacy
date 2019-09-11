// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_reader.h"

#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reading_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_scan_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// static
NFCReader* NFCReader::Create(ExecutionContext* context) {
  return MakeGarbageCollected<NFCReader>(context);
}

NFCReader::NFCReader(ExecutionContext* context)
    : ContextLifecycleObserver(context) {
  // Call GetNFCProxy to create a proxy. This guarantees no allocation will
  // be needed when calling HasPendingActivity later during gc tracing.
  GetNfcProxy();
}

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

// https://w3c.github.io/web-nfc/#the-scan-method
void NFCReader::scan(const NFCScanOptions* options) {
  if (!CheckSecurity())
    return;

  if (options->hasSignal()) {
    // 6. If reader.[[Signal]]'s aborted flag is set, then return.
    if (options->signal()->aborted())
      return;

    // 7. If reader.[[Signal]] is not null, then add the following abort steps
    // to reader.[[Signal]]:
    options->signal()->AddAlgorithm(
        WTF::Bind(&NFCReader::Abort, WrapPersistent(this)));
  }

  // Step 8.4, if the url is not an empty string and it is not a valid URL
  // pattern, fire a NFCErrorEvent with "SyntaxError" DOMException, then return.
  if (options->hasURL() && !options->url().IsEmpty()) {
    KURL pattern_url(options->url());
    if (!pattern_url.IsValid() || pattern_url.Protocol() != kNfcProtocolHttps) {
      DispatchEvent(*MakeGarbageCollected<NFCErrorEvent>(
          event_type_names::kError,
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                             kNfcUrlPatternError)));
      return;
    }
  }

  GetNfcProxy()->StartReading(this, options);
}

void NFCReader::Trace(blink::Visitor* visitor) {
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
      event_type_names::kError, NFCErrorTypeToDOMException(error)));
}

void NFCReader::ContextDestroyed(ExecutionContext*) {
  GetNfcProxy()->StopReading(this);
}

void NFCReader::Abort() {
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
