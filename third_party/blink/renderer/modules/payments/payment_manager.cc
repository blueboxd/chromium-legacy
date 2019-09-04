// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_manager.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/payments/payment_instruments.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

PaymentManager* PaymentManager::Create(
    ServiceWorkerRegistration* registration) {
  return MakeGarbageCollected<PaymentManager>(registration);
}

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

void PaymentManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(instruments_);
  ScriptWrappable::Trace(visitor);
}

PaymentManager::PaymentManager(ServiceWorkerRegistration* registration)
    : registration_(registration), instruments_(nullptr) {
  DCHECK(registration);

  if (ExecutionContext* context = registration->GetExecutionContext()) {
    if (auto* interface_provider = context->GetInterfaceProvider()) {
      interface_provider->GetInterface(manager_.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kUserInteraction)));
    }
  }

  manager_.set_disconnect_handler(WTF::Bind(
      &PaymentManager::OnServiceConnectionError, WrapWeakPersistent(this)));
  manager_->Init(registration_->GetExecutionContext()->Url(),
                 registration_->scope());
}

void PaymentManager::OnServiceConnectionError() {
  manager_.reset();
}

}  // namespace blink
