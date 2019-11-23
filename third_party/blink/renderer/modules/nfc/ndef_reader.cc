// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"

#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_error_event.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reading_event.h"
#include "third_party/blink/renderer/modules/nfc/ndef_scan_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

void OnScanRequestCompleted(ScriptPromiseResolver* resolver,
                            device::mojom::blink::NDEFErrorPtr error) {
  if (error) {
    resolver->Reject(NDEFErrorTypeToDOMException(error->error_type));
    return;
  }
  resolver->Resolve();
}

}  // namespace

// static
NDEFReader* NDEFReader::Create(ExecutionContext* context) {
  return MakeGarbageCollected<NDEFReader>(context);
}

NDEFReader::NDEFReader(ExecutionContext* context)
    : ContextLifecycleObserver(context) {
  // Call GetNFCProxy to create a proxy. This guarantees no allocation will
  // be needed when calling HasPendingActivity later during gc tracing.
  GetNfcProxy();
}

NDEFReader::~NDEFReader() = default;

const AtomicString& NDEFReader::InterfaceName() const {
  return event_target_names::kNDEFReader;
}

ExecutionContext* NDEFReader::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool NDEFReader::HasPendingActivity() const {
  return GetExecutionContext() && GetNfcProxy()->IsReading(this) &&
         HasEventListeners();
}

// https://w3c.github.io/web-nfc/#the-scan-method
ScriptPromise NDEFReader::scan(ScriptState* script_state,
                               const NDEFScanOptions* options,
                               ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  Document* document = To<Document>(execution_context);
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!execution_context || !document->IsInMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "NFC interfaces are only avaliable "
                                      "in a top-level browsing context");
    return ScriptPromise();
  }

  // 7. If reader.[[Signal]]'s aborted flag is set, then reject p with a
  // "AbortError" DOMException and return p.
  if (options->hasSignal() && options->signal()->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The NFC operation was cancelled.");
    return ScriptPromise();
  }

  // 9.4 If the reader.[[Id]] is not an empty string and it is not a valid URL
  // pattern, then reject p with a "SyntaxError" DOMException and return p.
  // TODO(https://crbug.com/520391): Instead of NDEFScanOptions#url, introduce
  // and use NDEFScanOptions#id to do the filtering after we add support for
  // writing NDEFRecord#id.
  if (options->hasId() && !options->id().IsEmpty()) {
    KURL pattern_url(options->id());
    if (!pattern_url.IsValid() || pattern_url.Protocol() != "https") {
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "Invalid URL pattern was provided.");
      return ScriptPromise();
    }
  }

  // TODO(https://crbug.com/520391): With the note in
  // https://w3c.github.io/web-nfc/#the-ndefreader-and-ndefwriter-objects,
  // successive invocations of NDEFReader.scan() with new options should replace
  // existing filters. For now we just reject this new scan() when there is an
  // ongoing filter active.
  if (GetNfcProxy()->IsReading(this)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "There is already a scan() operation ongoing.");
    return ScriptPromise();
  }

  resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // 8. If reader.[[Signal]] is not null, then add the following abort steps to
  // reader.[[Signal]]:
  if (options->hasSignal()) {
    options->signal()->AddAlgorithm(WTF::Bind(&NDEFReader::Abort,
                                              WrapPersistent(this),
                                              WrapPersistent(resolver_.Get())));
  }

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(document->GetFrame()),
      WTF::Bind(&NDEFReader::OnRequestPermission, WrapPersistent(this),
                WrapPersistent(resolver_.Get()), WrapPersistent(options)));
  return resolver_->Promise();
}

PermissionService* NDEFReader::GetPermissionService() {
  if (!permission_service_) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver());
  }
  return permission_service_.get();
}

void NDEFReader::OnRequestPermission(ScriptPromiseResolver* resolver,
                                     const NDEFScanOptions* options,
                                     PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    return;
  }
  if (options->hasSignal() && options->signal()->aborted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "The NFC operation was cancelled."));
    return;
  }

  UseCounter::Count(GetExecutionContext(), WebFeature::kWebNfcNdefReaderScan);

  GetNfcProxy()->StartReading(
      this, options,
      WTF::Bind(&OnScanRequestCompleted, WrapPersistent(resolver)));
}

void NDEFReader::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void NDEFReader::OnReading(const String& serial_number,
                           const device::mojom::blink::NDEFMessage& message) {
  DCHECK(GetNfcProxy()->IsReading(this));
  DispatchEvent(*MakeGarbageCollected<NDEFReadingEvent>(
      event_type_names::kReading, serial_number,
      MakeGarbageCollected<NDEFMessage>(message)));
}

void NDEFReader::OnError(device::mojom::blink::NDEFErrorType error) {
  DispatchEvent(*MakeGarbageCollected<NDEFErrorEvent>(
      event_type_names::kError, NDEFErrorTypeToDOMException(error)));
}

void NDEFReader::OnMojoConnectionError() {
  // If |resolver_| has already settled this rejection is silently ignored.
  if (resolver_) {
    resolver_->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED));
  }
  // Dispatches an error event.
  OnError(device::mojom::blink::NDEFErrorType::NOT_SUPPORTED);
}

void NDEFReader::ContextDestroyed(ExecutionContext*) {
  // If |resolver_| has already settled this rejection is silently ignored.
  if (resolver_) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError,
        "The execution context is going to be gone."));
  }
  GetNfcProxy()->StopReading(this);
}

void NDEFReader::Abort(ScriptPromiseResolver* resolver) {
  // If |resolver| has already settled this rejection is silently ignored.
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "The NFC operation was cancelled."));
  GetNfcProxy()->StopReading(this);
}

NFCProxy* NDEFReader::GetNfcProxy() const {
  DCHECK(GetExecutionContext());
  return NFCProxy::From(*To<Document>(GetExecutionContext()));
}

}  // namespace blink
