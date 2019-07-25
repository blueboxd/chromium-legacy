// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc.h"

#include <utility>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error.h"
#include "third_party/blink/renderer/modules/nfc/nfc_push_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reader_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

NFC::NFC(LocalFrame* frame)
    : PageVisibilityObserver(frame->GetPage()),
      ContextLifecycleObserver(frame->GetDocument()),
      client_binding_(this) {
  String error_message;

  // Only connect to NFC if we are in a context that supports it.
  if (!IsSupportedInContext(GetExecutionContext(), error_message))
    return;

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = frame->GetTaskRunner(TaskType::kMiscPlatformAPI);
  frame->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&nfc_, task_runner));
  nfc_.set_connection_error_handler(
      WTF::Bind(&NFC::OnConnectionError, WrapWeakPersistent(this)));
  device::mojom::blink::NFCClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client, task_runner), task_runner);
  nfc_->SetClient(std::move(client));
}

NFC* NFC::Create(LocalFrame* frame) {
  NFC* nfc = MakeGarbageCollected<NFC>(frame);
  return nfc;
}

NFC::~NFC() {
  // |m_nfc| may hold persistent handle to |this| object, therefore, there
  // should be no more outstanding requests when NFC object is destructed.
  DCHECK(requests_.IsEmpty());
}

void NFC::Dispose() {
  client_binding_.Close();
}

void NFC::ContextDestroyed(ExecutionContext*) {
  nfc_.reset();
  requests_.clear();
  callbacks_.clear();
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
// https://w3c.github.io/web-nfc/#dom-nfc-push
ScriptPromise NFC::push(ScriptState* script_state,
                        const NDEFMessageSource& push_message,
                        const NFCPushOptions* options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  ScriptPromise isValidMessage =
      RejectIfInvalidNDEFMessageSource(script_state, push_message);
  if (!isValidMessage.IsEmpty())
    return isValidMessage;

  // https://w3c.github.io/web-nfc/#dom-nfc-push
  // 9. If timeout value is NaN or negative, reject promise with "TypeError"
  // and abort these steps.
  if (options->hasTimeout() &&
      (std::isnan(options->timeout()) || options->timeout() < 0)) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcInvalidPushTimeout));
  }

  device::mojom::blink::NDEFMessagePtr message =
      device::mojom::blink::NDEFMessage::From(push_message);
  if (!message) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kSyntaxError, kNfcMsgConvertError));
  }

  if (!SetNDEFMessageURL(
          ExecutionContext::From(script_state)->GetSecurityOrigin()->ToString(),
          message)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kSyntaxError, kNfcSetIdError));
  }

  if (GetNDEFMessageSize(message) >
      device::mojom::blink::NDEFMessage::kMaxSize) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                           kNfcMsgMaxSizeError));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  auto callback = WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                            WrapPersistent(resolver));
  nfc_->Push(std::move(message),
             device::mojom::blink::NFCPushOptions::From(options),
             std::move(callback));

  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelpush
ScriptPromise NFC::cancelPush(ScriptState* script_state, const String& target) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  auto callback = WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                            WrapPersistent(resolver));
  nfc_->CancelPush(StringToNFCPushTarget(target), std::move(callback));

  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#watching-for-content
// https://w3c.github.io/web-nfc/#dom-nfc-watch
ScriptPromise NFC::watch(ScriptState* script_state,
                         V8MessageCallback* callback,
                         const NFCReaderOptions* options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  // https://w3c.github.io/web-nfc/#dom-nfc-watch (Step 9)
  if (options->hasURL() && !options->url().IsEmpty()) {
    KURL pattern_url(options->url());
    if (!pattern_url.IsValid() || pattern_url.Protocol() != kNfcProtocolHttps) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                             kNfcUrlPatternError));
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  auto watch_callback =
      WTF::Bind(&NFC::OnWatchRegistered, WrapPersistent(this),
                WrapPersistent(callback), WrapPersistent(resolver));
  nfc_->Watch(device::mojom::blink::NFCReaderOptions::From(options),
              std::move(watch_callback));
  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
ScriptPromise NFC::cancelWatch(ScriptState* script_state, int32_t id) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  if (id) {
    callbacks_.erase(id);
  } else {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotFoundError,
                                           kNfcWatchIdNotFound));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  nfc_->CancelWatch(id,
                    WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                              WrapPersistent(resolver)));
  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
// If watchId is not provided to nfc.cancelWatch, cancel all watch operations.
ScriptPromise NFC::cancelWatch(ScriptState* script_state) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  callbacks_.clear();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  nfc_->CancelAllWatches(WTF::Bind(&NFC::OnRequestCompleted,
                                   WrapPersistent(this),
                                   WrapPersistent(resolver)));
  return resolver->Promise();
}

void NFC::PageVisibilityChanged() {
  // If service is not initialized, there cannot be any pending NFC activities
  if (!nfc_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (GetPage()->IsPageVisible())
    nfc_->ResumeNFCOperations();
  else
    nfc_->SuspendNFCOperations();
}

void NFC::OnRequestCompleted(ScriptPromiseResolver* resolver,
                             device::mojom::blink::NFCErrorPtr error) {
  if (!requests_.Contains(resolver))
    return;

  requests_.erase(resolver);
  if (error.is_null())
    resolver->Resolve();
  else
    resolver->Reject(NFCError::Take(resolver, error->error_type));
}

void NFC::OnConnectionError() {
  nfc_.reset();
  client_binding_.Close();
  callbacks_.clear();

  // If NFCService is not available or disappears when NFC hardware is
  // disabled, reject promise with NotSupportedError exception.
  for (ScriptPromiseResolver* resolver : requests_)
    resolver->Reject(NFCError::Take(
        resolver, device::mojom::blink::NFCErrorType::NOT_SUPPORTED));

  requests_.clear();
}

void NFC::OnWatch(const Vector<uint32_t>& ids,
                  const String& serial_number,
                  device::mojom::blink::NDEFMessagePtr message) {
  if (!GetExecutionContext())
    return;

  for (const auto& id : ids) {
    auto it = callbacks_.find(id);
    if (it != callbacks_.end()) {
      V8MessageCallback* callback = it->value;
      ScriptState* script_state =
          callback->CallbackRelevantScriptStateOrReportError("NFC", "watch");
      if (!script_state)
        continue;
      callback->InvokeAndReportException(
          nullptr, MakeGarbageCollected<NDEFMessage>(*message));
    }
  }
}

bool NFC::IsSupportedInContext(ExecutionContext* context,
                               String& error_message) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!To<Document>(context)->domWindow()->GetFrame() ||
      !To<Document>(context)->GetFrame()->IsMainFrame()) {
    error_message = kNfcAccessInNonTopFrame;
    return false;
  }

  return true;
}

ScriptPromise NFC::RejectIfNotSupported(ScriptState* script_state) {
  String error_message;
  if (!IsSupportedInContext(ExecutionContext::From(script_state),
                            error_message)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError, error_message));
  }

  if (!nfc_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                           kNfcNotSupported));
  }

  return ScriptPromise();
}

void NFC::OnWatchRegistered(V8MessageCallback* callback,
                            ScriptPromiseResolver* resolver,
                            uint32_t id,
                            device::mojom::blink::NFCErrorPtr error) {
  requests_.erase(resolver);

  // Invalid id was returned.
  // https://w3c.github.io/web-nfc/#dom-nfc-watch
  // 8. If the request fails, reject promise with "NotSupportedError"
  // and abort these steps.
  if (!id) {
    resolver->Reject(NFCError::Take(
        resolver, device::mojom::blink::NFCErrorType::NOT_SUPPORTED));
    return;
  }

  if (error.is_null()) {
    callbacks_.insert(id, callback);
    resolver->Resolve(id);
  } else {
    resolver->Reject(NFCError::Take(resolver, error->error_type));
  }
}

void NFC::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  visitor->Trace(callbacks_);
  ScriptWrappable::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
