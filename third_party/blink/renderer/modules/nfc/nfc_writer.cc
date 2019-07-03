// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_writer.h"

#include <utility>

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error.h"
#include "third_party/blink/renderer/modules/nfc/nfc_push_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

namespace blink {

// static
NFCWriter* NFCWriter::Create(ExecutionContext* context) {
  return MakeGarbageCollected<NFCWriter>(context);
}

NFCWriter::NFCWriter(ExecutionContext* context) : ContextClient(context) {}

void NFCWriter::Trace(blink::Visitor* visitor) {
  visitor->Trace(nfc_proxy_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
// https://w3c.github.io/web-nfc/#dom-nfc-push
ScriptPromise NFCWriter::push(ScriptState* script_state,
                              const NDEFMessageSource& push_message,
                              const NFCPushOptions* options) {
  ExecutionContext* execution_context = GetExecutionContext();
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!execution_context || !To<Document>(execution_context)->IsInMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           kNfcAccessInNonTopFrame));
  }

  ScriptPromise is_valid_message =
      RejectIfInvalidNDEFMessageSource(script_state, push_message);
  if (!is_valid_message.IsEmpty())
    return is_valid_message;

  // https://w3c.github.io/web-nfc/#dom-nfc-push
  // 9. If timeout value is NaN or negative, reject promise with "TypeError"
  // and abort these steps.
  if (options->hasTimeout() &&
      (std::isnan(options->timeout()) || options->timeout() < 0)) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNfcInvalidPushTimeout));
  }

  auto message = device::mojom::blink::NDEFMessage::From(push_message);
  if (!message) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kSyntaxError, kNfcMsgConvertError));
  }

  if (!SetNDEFMessageURL(execution_context->GetSecurityOrigin()->ToString(),
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
  auto callback = WTF::Bind(&NFCWriter::OnRequestCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));

  InitNfcProxyIfNeeded();
  nfc_proxy_->Push(std::move(message),
                   device::mojom::blink::NFCPushOptions::From(options),
                   std::move(callback));

  return resolver->Promise();
}

void NFCWriter::OnMojoConnectionError() {
  nfc_proxy_.Clear();

  // If the mojo connection breaks, all push requests will be reject with a
  // default error.
  for (ScriptPromiseResolver* resolver : requests_) {
    resolver->Reject(NFCError::Take(
        resolver, device::mojom::blink::NFCErrorType::NOT_READABLE));
  }
  requests_.clear();
}

void NFCWriter::InitNfcProxyIfNeeded() {
  // Init NfcProxy if needed.
  if (nfc_proxy_)
    return;

  nfc_proxy_ = NFCProxy::From(*To<Document>(GetExecutionContext()));
  DCHECK(nfc_proxy_);

  // Add the writer to proxy's writer list for mojo connection error
  // notification.
  nfc_proxy_->AddWriter(this);
}

void NFCWriter::OnRequestCompleted(ScriptPromiseResolver* resolver,
                                   device::mojom::blink::NFCErrorPtr error) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (error.is_null())
    resolver->Resolve();
  else
    resolver->Reject(NFCError::Take(resolver, error->error_type));
}

}  // namespace blink
