// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_manager.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/payments/payment_instruments.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

PaymentInstruments* PaymentManager::instruments() {
  if (!instruments_)
    instruments_ = MakeGarbageCollected<PaymentInstruments>(manager_);
  return instruments_;
}

const String& PaymentManager::userHint() {
  return user_hint_;
}

void PaymentManager::setUserHint(const String& user_hint) {
  user_hint_ = user_hint;
  manager_->SetUserHint(user_hint_);
}

ScriptPromise PaymentManager::enableDelegations(
    ScriptState* script_state,
    const Vector<String>& stringified_delegations,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot enable payment delegations");
    return ScriptPromise();
  }

  if (enable_delegations_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot call enableDelegations() again until the previous "
        "enableDelegations() is finished");
    return ScriptPromise();
  }

  Vector<payments::mojom::blink::PaymentDelegation> delegations;
  for (auto delegation : stringified_delegations) {
    if (delegation == "shippingAddress") {
      delegations.emplace_back(
          payments::mojom::blink::PaymentDelegation::SHIPPING_ADDRESS);
    } else if (delegation == "payerName") {
      delegations.emplace_back(
          payments::mojom::blink::PaymentDelegation::PAYER_NAME);
    } else if (delegation == "payerPhone") {
      delegations.emplace_back(
          payments::mojom::blink::PaymentDelegation::PAYER_PHONE);
    } else {
      DCHECK_EQ("payerEmail", delegation);
      delegations.emplace_back(
          payments::mojom::blink::PaymentDelegation::PAYER_EMAIL);
    }
  }

  manager_->EnableDelegations(
      std::move(delegations),
      WTF::Bind(&PaymentManager::OnEnableDelegationsResponse,
                WrapPersistent(this)));
  enable_delegations_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  return enable_delegations_resolver_->Promise();
}

void PaymentManager::Trace(Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(instruments_);
  visitor->Trace(enable_delegations_resolver_);
  ScriptWrappable::Trace(visitor);
}

PaymentManager::PaymentManager(ServiceWorkerRegistration* registration)
    : registration_(registration), instruments_(nullptr) {
  DCHECK(registration);

  if (ExecutionContext* context = registration->GetExecutionContext()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        manager_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kUserInteraction)));
  }

  manager_.set_disconnect_handler(WTF::Bind(
      &PaymentManager::OnServiceConnectionError, WrapWeakPersistent(this)));
  manager_->Init(registration_->GetExecutionContext()->Url(),
                 registration_->scope());
}

void PaymentManager::OnEnableDelegationsResponse(
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(enable_delegations_resolver_);
  enable_delegations_resolver_->Resolve(
      status == payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
  enable_delegations_resolver_.Clear();
}

void PaymentManager::OnServiceConnectionError() {
  if (enable_delegations_resolver_)
    enable_delegations_resolver_.Clear();

  manager_.reset();
}

}  // namespace blink
