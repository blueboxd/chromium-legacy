// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_writer.h"

#include <utility>

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_array_buffer_view_or_ndef_message_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_write_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

// static
NDEFWriter* NDEFWriter::Create(ExecutionContext* context) {
  context->GetScheduler()->RegisterStickyFeature(
      blink::SchedulingPolicy::Feature::kWebNfc,
      {blink::SchedulingPolicy::RecordMetricsForBackForwardCache()});
  return MakeGarbageCollected<NDEFWriter>(context);
}

NDEFWriter::NDEFWriter(ExecutionContext* context)
    : ExecutionContextClient(context), permission_service_(context) {}

void NDEFWriter::Trace(Visitor* visitor) {
  visitor->Trace(permission_service_);
  visitor->Trace(requests_);
  visitor->Trace(nfc_proxy_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

// https://w3c.github.io/web-nfc/#writing-content
// https://w3c.github.io/web-nfc/#the-write-method
ScriptPromise NDEFWriter::write(ScriptState* script_state,
                                const NDEFMessageSource& write_message,
                                const NDEFWriteOptions* options,
                                ExceptionState& exception_state) {
  LocalDOMWindow* window = script_state->ContextIsValid()
                               ? LocalDOMWindow::From(script_state)
                               : nullptr;
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!window || !window->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "NFC interfaces are only avaliable "
                                      "in a top-level browsing context");
    return ScriptPromise();
  }

  if (options->hasSignal() && options->signal()->aborted()) {
    // If signal’s aborted flag is set, then reject p with an "AbortError"
    // DOMException and return p.
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The NFC operation was cancelled.");
    return ScriptPromise();
  }

  // Step 11.2: Run "create NDEF message", if this throws an exception,
  // reject p with that exception and abort these steps.
  NDEFMessage* ndef_message =
      NDEFMessage::Create(window, write_message, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  auto message = device::mojom::blink::NDEFMessage::From(ndef_message);
  DCHECK(message);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  InitNfcProxyIfNeeded();
  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(window->GetFrame()),
      WTF::Bind(&NDEFWriter::OnRequestPermission, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(options),
                std::move(message)));

  return resolver->Promise();
}

PermissionService* NDEFWriter::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return permission_service_.get();
}

void NDEFWriter::OnRequestPermission(
    ScriptPromiseResolver* resolver,
    const NDEFWriteOptions* options,
    device::mojom::blink::NDEFMessagePtr message,
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

  // If signal is not null, then add the abort steps to signal.
  if (options->hasSignal() && !options->signal()->aborted()) {
    options->signal()->AddAlgorithm(WTF::Bind(
        &NDEFWriter::Abort, WrapPersistent(this), WrapPersistent(resolver)));
  }

  UseCounter::Count(GetExecutionContext(), WebFeature::kWebNfcNdefWriterWrite);
  // TODO(https://crbug.com/994936) remove when origin trial is complete.
  UseCounter::Count(GetExecutionContext(), WebFeature::kWebNfcAPI);

  auto callback = WTF::Bind(&NDEFWriter::OnRequestCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));
  nfc_proxy_->Push(std::move(message),
                   device::mojom::blink::NDEFWriteOptions::From(options),
                   std::move(callback));
}

void NDEFWriter::OnMojoConnectionError() {
  nfc_proxy_.Clear();

  // If the mojo connection breaks, all push requests will be rejected with a
  // default error.
  for (ScriptPromiseResolver* resolver : requests_) {
    resolver->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        "WebNFC feature is unavailable or permission denied."));
  }
  requests_.clear();
}

void NDEFWriter::InitNfcProxyIfNeeded() {
  // Init NfcProxy if needed.
  if (nfc_proxy_)
    return;

  nfc_proxy_ = NFCProxy::From(*To<LocalDOMWindow>(GetExecutionContext()));
  DCHECK(nfc_proxy_);

  // Add the writer to proxy's writer list for mojo connection error
  // notification.
  nfc_proxy_->AddWriter(this);
}

void NDEFWriter::Abort(ScriptPromiseResolver* resolver) {
  // |nfc_proxy_| could be null on Mojo connection failure, simply ignore the
  // abort request in this case.
  if (!nfc_proxy_)
    return;

  // OnRequestCompleted() should always be called whether the push operation is
  // cancelled successfully or not. So do nothing for the cancelled callback.
  nfc_proxy_->CancelPush(device::mojom::blink::NFC::CancelPushCallback());
}

void NDEFWriter::OnRequestCompleted(ScriptPromiseResolver* resolver,
                                    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(requests_.Contains(resolver));

  requests_.erase(resolver);

  if (error.is_null()) {
    resolver->Resolve();
  } else {
    resolver->Reject(
        NDEFErrorTypeToDOMException(error->error_type, error->error_message));
  }
}

}  // namespace blink
