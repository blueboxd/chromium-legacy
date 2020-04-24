// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"

#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_scan_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reading_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

constexpr char kNotSupportedOrPermissionDenied[] =
    "WebNFC feature is unavailable or permission denied.";

}  // namespace

// static
NDEFReader* NDEFReader::Create(ExecutionContext* context) {
  context->GetScheduler()->RegisterStickyFeature(
      SchedulingPolicy::Feature::kWebNfc,
      {SchedulingPolicy::RecordMetricsForBackForwardCache()});
  return MakeGarbageCollected<NDEFReader>(context);
}

NDEFReader::NDEFReader(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context), permission_service_(context) {
  // Call GetNFCProxy to create a proxy. This guarantees no allocation will
  // be needed when calling HasPendingActivity later during gc tracing.
  GetNfcProxy();
}

NDEFReader::~NDEFReader() = default;

const AtomicString& NDEFReader::InterfaceName() const {
  return event_target_names::kNDEFReader;
}

ExecutionContext* NDEFReader::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool NDEFReader::HasPendingActivity() const {
  return GetExecutionContext() && GetNfcProxy()->IsReading(this) &&
         HasEventListeners();
}

// https://w3c.github.io/web-nfc/#the-scan-method
ScriptPromise NDEFReader::scan(ScriptState* script_state,
                               const NDEFScanOptions* options,
                               ExceptionState& exception_state) {
  LocalFrame* frame = script_state->ContextIsValid()
                          ? LocalDOMWindow::From(script_state)->GetFrame()
                          : nullptr;
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!frame || !frame->IsMainFrame()) {
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

  // With the note in
  // https://w3c.github.io/web-nfc/#the-ndefreader-and-ndefwriter-objects,
  // successive invocations of NDEFReader.scan() with new options should replace
  // existing filters. So stop current reading for this case.
  if (GetNfcProxy()->IsReading(this)) {
    Abort(signal_.Get());
  }

  resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // 8. If reader.[[Signal]] is not null, then add the following abort steps to
  // reader.[[Signal]]:
  if (options->hasSignal()) {
    signal_ = options->signal();
    signal_->AddAlgorithm(WTF::Bind(&NDEFReader::Abort, WrapPersistent(this),
                                    WrapPersistent(options->signal())));
  }

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(frame),
      WTF::Bind(&NDEFReader::OnRequestPermission, WrapPersistent(this),
                WrapPersistent(options)));
  return resolver_->Promise();
}

PermissionService* NDEFReader::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return permission_service_.get();
}

void NDEFReader::OnRequestPermission(const NDEFScanOptions* options,
                                     PermissionStatus status) {
  if (!resolver_)
    return;

  if (status != PermissionStatus::GRANTED) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    resolver_.Clear();
    return;
  }
  if (options->hasSignal() && options->signal()->aborted()) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "The NFC operation was cancelled."));
    resolver_.Clear();
    return;
  }

  UseCounter::Count(GetExecutionContext(), WebFeature::kWebNfcNdefReaderScan);
  // TODO(https://crbug.com/994936) remove when origin trial is complete.
  UseCounter::Count(GetExecutionContext(), WebFeature::kWebNfcAPI);

  GetNfcProxy()->StartReading(
      this, options,
      WTF::Bind(&NDEFReader::OnScanRequestCompleted, WrapPersistent(this)));
}

void NDEFReader::OnScanRequestCompleted(
    device::mojom::blink::NDEFErrorPtr error) {
  if (!resolver_)
    return;

  if (error) {
    resolver_->Reject(
        NDEFErrorTypeToDOMException(error->error_type, error->error_message));
  } else {
    resolver_->Resolve();
  }

  resolver_.Clear();
}

void NDEFReader::Trace(Visitor* visitor) {
  visitor->Trace(permission_service_);
  visitor->Trace(resolver_);
  visitor->Trace(signal_);
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void NDEFReader::OnReading(const String& serial_number,
                           const device::mojom::blink::NDEFMessage& message) {
  DCHECK(GetNfcProxy()->IsReading(this));
  DispatchEvent(*MakeGarbageCollected<NDEFReadingEvent>(
      event_type_names::kReading, serial_number,
      MakeGarbageCollected<NDEFMessage>(message)));
}

void NDEFReader::OnError(const String& message) {
  ErrorEvent* event = ErrorEvent::Create(
      message, SourceLocation::Capture(GetExecutionContext()), nullptr);
  DispatchEvent(*event);
}

void NDEFReader::OnMojoConnectionError() {
  // If |resolver_| has already settled this rejection is silently ignored.
  if (resolver_) {
    resolver_->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
    resolver_.Clear();
  }

  // Dispatches an error event.
  OnError(kNotSupportedOrPermissionDenied);
}

void NDEFReader::ContextDestroyed() {
  // If |resolver_| has already settled this rejection is silently ignored.
  if (resolver_) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError,
        "The execution context is going to be gone."));
    resolver_.Clear();
  }
  GetNfcProxy()->StopReading(this);
}

void NDEFReader::Abort(AbortSignal* signal) {
  // In the case of successive invocations of NDEFReader.scan() with
  // different signals, we should make sure aborting on previous signal
  // won't abort current reading.
  // If this is not triggered by the current signal, just ignore it.
  if (signal && signal != signal_)
    return;

  if (resolver_) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "The NFC operation was cancelled."));
    resolver_.Clear();
  }

  GetNfcProxy()->StopReading(this);
}

NFCProxy* NDEFReader::GetNfcProxy() const {
  DCHECK(GetExecutionContext());
  return NFCProxy::From(*To<LocalDOMWindow>(GetExecutionContext()));
}

}  // namespace blink
