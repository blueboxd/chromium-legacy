// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_event.h"

#include <utility>

#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/payments/payment_method_change_response.h"
#include "third_party/blink/renderer/modules/payments/payments_validators.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit* initializer) {
  return MakeGarbageCollected<PaymentRequestEvent>(
      type, initializer, mojo::NullRemote(), nullptr, nullptr);
}

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit* initializer,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return MakeGarbageCollected<PaymentRequestEvent>(
      type, initializer, std::move(host), respond_with_observer,
      wait_until_observer);
}

PaymentRequestEvent::PaymentRequestEvent(
    const AtomicString& type,
    const PaymentRequestEventInit* initializer,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_origin_(initializer->topOrigin()),
      payment_request_origin_(initializer->paymentRequestOrigin()),
      payment_request_id_(initializer->paymentRequestId()),
      method_data_(initializer->hasMethodData()
                       ? initializer->methodData()
                       : HeapVector<Member<PaymentMethodData>>()),
      total_(initializer->hasTotal() ? initializer->total()
                                     : PaymentCurrencyAmount::Create()),
      modifiers_(initializer->hasModifiers()
                     ? initializer->modifiers()
                     : HeapVector<Member<PaymentDetailsModifier>>()),
      instrument_key_(initializer->instrumentKey()),
      payment_options_(initializer->hasPaymentOptions()
                           ? initializer->paymentOptions()
                           : PaymentOptions::Create()),
      shipping_options_(initializer->hasShippingOptions()
                            ? initializer->shippingOptions()
                            : HeapVector<Member<PaymentShippingOption>>()),
      observer_(respond_with_observer) {
  if (!host.is_valid())
    return;

  payment_handler_host_.Bind(std::move(host));
  payment_handler_host_.set_disconnect_handler(WTF::Bind(
      &PaymentRequestEvent::OnHostConnectionError, WrapWeakPersistent(this)));
}

PaymentRequestEvent::~PaymentRequestEvent() = default;

const AtomicString& PaymentRequestEvent::InterfaceName() const {
  return event_interface_names::kPaymentRequestEvent;
}

const String& PaymentRequestEvent::topOrigin() const {
  return top_origin_;
}

const String& PaymentRequestEvent::paymentRequestOrigin() const {
  return payment_request_origin_;
}

const String& PaymentRequestEvent::paymentRequestId() const {
  return payment_request_id_;
}

const HeapVector<Member<PaymentMethodData>>& PaymentRequestEvent::methodData()
    const {
  return method_data_;
}

const ScriptValue PaymentRequestEvent::total(ScriptState* script_state) const {
  return ScriptValue::From(script_state, total_);
}

const HeapVector<Member<PaymentDetailsModifier>>&
PaymentRequestEvent::modifiers() const {
  return modifiers_;
}

const String& PaymentRequestEvent::instrumentKey() const {
  return instrument_key_;
}

const ScriptValue PaymentRequestEvent::paymentOptions(
    ScriptState* script_state) const {
  if (!payment_options_)
    return ScriptValue::CreateNull(script_state);
  return ScriptValue::From(script_state, payment_options_);
}

const HeapVector<Member<PaymentShippingOption>>&
PaymentRequestEvent::shippingOptions(bool& is_null) const {
  is_null = shipping_options_.IsEmpty();
  return shipping_options_;
}

ScriptPromise PaymentRequestEvent::openWindow(ScriptState* script_state,
                                              const String& url) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!isTrusted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Cannot open a window when the event is not trusted"));
    return promise;
  }

  KURL parsed_url_to_open = context->CompleteURL(url);
  if (!parsed_url_to_open.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }

  if (!context->GetSecurityOrigin()->IsSameSchemeHostPort(
          SecurityOrigin::Create(parsed_url_to_open).get())) {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return promise;
  }

  if (!context->IsWindowInteractionAllowed()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Not allowed to open a window without user activation"));
    return promise;
  }
  context->ConsumeWindowInteraction();

  To<ServiceWorkerGlobalScope>(context)
      ->GetServiceWorkerHost()
      ->OpenPaymentHandlerWindow(
          parsed_url_to_open,
          ServiceWorkerWindowClient::CreateResolveWindowClientCallback(
              resolver));
  return promise;
}

ScriptPromise PaymentRequestEvent::changePaymentMethod(
    ScriptState* script_state,
    const String& method_name,
    ExceptionState& exception_state) {
  return changePaymentMethod(script_state, method_name, ScriptValue(),
                             exception_state);
}

ScriptPromise PaymentRequestEvent::changePaymentMethod(
    ScriptState* script_state,
    const String& method_name,
    const ScriptValue& method_details,
    ExceptionState& exception_state) {
  if (change_payment_method_resolver_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "Waiting for response to the previous payment method change"));
  }

  if (!payment_handler_host_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "No corresponding PaymentRequest object found"));
  }

  auto method_data = payments::mojom::blink::PaymentHandlerMethodData::New();
  if (!method_details.IsEmpty()) {
    PaymentsValidators::ValidateAndStringifyObject(
        script_state->GetIsolate(), "Method details", method_details,
        method_data->stringified_data, exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  }

  method_data->method_name = method_name;
  payment_handler_host_->ChangePaymentMethod(
      std::move(method_data),
      WTF::Bind(&PaymentRequestEvent::OnChangePaymentMethodResponse,
                WrapWeakPersistent(this)));
  change_payment_method_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  return change_payment_method_resolver_->Promise();
}

void PaymentRequestEvent::respondWith(ScriptState* script_state,
                                      ScriptPromise script_promise,
                                      ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot respond with data when the event is not trusted");
    return;
  }

  stopImmediatePropagation();
  if (observer_) {
    observer_->RespondWith(script_state, script_promise, exception_state);
  }
}

void PaymentRequestEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(method_data_);
  visitor->Trace(total_);
  visitor->Trace(modifiers_);
  visitor->Trace(payment_options_);
  visitor->Trace(shipping_options_);
  visitor->Trace(change_payment_method_resolver_);
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

void PaymentRequestEvent::OnChangePaymentMethodResponse(
    payments::mojom::blink::PaymentMethodChangeResponsePtr response) {
  if (!change_payment_method_resolver_)
    return;

  auto* dictionary = MakeGarbageCollected<PaymentMethodChangeResponse>();
  if (!response->error.IsNull() && !response->error.IsEmpty()) {
    dictionary->setError(response->error);
  }

  if (response->total) {
    auto* total = MakeGarbageCollected<PaymentCurrencyAmount>();
    total->setCurrency(response->total->currency);
    total->setValue(response->total->value);
    dictionary->setTotal(total);
  }

  ScriptState* script_state = change_payment_method_resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "PaymentDetailsModifier");

  if (response->modifiers) {
    auto* modifiers =
        MakeGarbageCollected<HeapVector<Member<PaymentDetailsModifier>>>();
    for (const auto& response_modifier : *response->modifiers) {
      if (!response_modifier)
        continue;

      auto* mod = MakeGarbageCollected<PaymentDetailsModifier>();
      mod->setSupportedMethod(response_modifier->method_data->method_name);

      if (response_modifier->total) {
        auto* amount = MakeGarbageCollected<PaymentCurrencyAmount>();
        amount->setCurrency(response_modifier->total->currency);
        amount->setValue(response_modifier->total->value);
        auto* total = MakeGarbageCollected<PaymentItem>();
        total->setAmount(amount);
        total->setLabel("");
        mod->setTotal(total);
      }

      if (!response_modifier->method_data->stringified_data.IsEmpty()) {
        v8::Local<v8::Value> parsed_value = FromJSONString(
            script_state->GetIsolate(), script_state->GetContext(),
            response_modifier->method_data->stringified_data, exception_state);
        if (exception_state.HadException()) {
          change_payment_method_resolver_->Reject(
              MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                 exception_state.Message()));
          change_payment_method_resolver_.Clear();
          return;
        }
        mod->setData(ScriptValue(script_state, parsed_value));
        modifiers->emplace_back(mod);
      }
    }
    dictionary->setModifiers(*modifiers);
  }

  if (response->stringified_payment_method_errors &&
      !response->stringified_payment_method_errors.IsEmpty()) {
    v8::Local<v8::Value> parsed_value = FromJSONString(
        script_state->GetIsolate(), script_state->GetContext(),
        response->stringified_payment_method_errors, exception_state);
    if (exception_state.HadException()) {
      change_payment_method_resolver_->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                             exception_state.Message()));
      change_payment_method_resolver_.Clear();
      return;
    }
    dictionary->setPaymentMethodErrors(ScriptValue(script_state, parsed_value));
  }

  change_payment_method_resolver_->Resolve(
      dictionary->hasError() || dictionary->hasTotal() ||
              dictionary->hasModifiers() || dictionary->hasPaymentMethodErrors()
          ? dictionary
          : nullptr);
  change_payment_method_resolver_.Clear();
}

void PaymentRequestEvent::OnHostConnectionError() {
  if (change_payment_method_resolver_) {
    change_payment_method_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Browser process disconnected"));
  }
  change_payment_method_resolver_.Clear();
  payment_handler_host_.reset();
}

}  // namespace blink
