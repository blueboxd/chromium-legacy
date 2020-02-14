// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/service_worker_registration_cookies.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_store_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"

namespace blink {

namespace {

class ServiceWorkerRegistrationCookiesImpl final
    : public GarbageCollected<ServiceWorkerRegistrationCookiesImpl>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationCookiesImpl);

 public:
  static const char kSupplementName[];

  static ServiceWorkerRegistrationCookiesImpl& From(
      ServiceWorkerRegistration& registration) {
    ServiceWorkerRegistrationCookiesImpl* supplement =
        Supplement<ServiceWorkerRegistration>::From<
            ServiceWorkerRegistrationCookiesImpl>(registration);
    if (!supplement) {
      supplement = MakeGarbageCollected<ServiceWorkerRegistrationCookiesImpl>(
          registration);
      ProvideTo(registration, supplement);
    }
    return *supplement;
  }

  explicit ServiceWorkerRegistrationCookiesImpl(
      ServiceWorkerRegistration& registration)
      : registration_(&registration) {}

  ~ServiceWorkerRegistrationCookiesImpl() = default;

  CookieStoreManager* GetCookieStoreManager() {
    if (!cookie_store_manager_) {
      ExecutionContext* execution_context =
          registration_->GetExecutionContext();
      DCHECK(execution_context);

      mojo::Remote<mojom::blink::CookieStore> backend;
      // TODO(pwnall): Replace TaskType::kInternalDefault with the task queue in
      //               the Cookie Store spec, once that spec is finalized.
      execution_context->GetBrowserInterfaceBroker().GetInterface(
          backend.BindNewPipeAndPassReceiver(
              execution_context->GetTaskRunner(TaskType::kInternalDefault)));
      cookie_store_manager_ = MakeGarbageCollected<CookieStoreManager>(
          registration_, std::move(backend));
    }
    return cookie_store_manager_.Get();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(registration_);
    visitor->Trace(cookie_store_manager_);
    Supplement<ServiceWorkerRegistration>::Trace(visitor);
  }

 private:
  Member<ServiceWorkerRegistration> registration_;
  Member<CookieStoreManager> cookie_store_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationCookiesImpl);
};

const char ServiceWorkerRegistrationCookiesImpl::kSupplementName[] =
    "ServiceWorkerRegistrationCookies";

}  // namespace

// static
CookieStoreManager* ServiceWorkerRegistrationCookies::cookies(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationCookiesImpl::From(registration)
      .GetCookieStoreManager();
}

}  // namespace blink
