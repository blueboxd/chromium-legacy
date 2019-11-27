// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_store_manager.h"

#include <utility>

#include "base/optional.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_list_item.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_store_get_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Returns null if and only if an exception is thrown.
mojom::blink::CookieChangeSubscriptionPtr ToBackendSubscription(
    const KURL& default_cookie_url,
    const CookieStoreGetOptions* subscription,
    ExceptionState& exception_state) {
  auto backend_subscription = mojom::blink::CookieChangeSubscription::New();

  if (subscription->hasURL()) {
    KURL subscription_url(default_cookie_url, subscription->url());
    // TODO(crbug.com/729800): Check that the URL is under default_cookie_url.
    backend_subscription->url = subscription_url;
  } else {
    backend_subscription->url = default_cookie_url;
  }

  if (subscription->matchType() == "starts-with") {
    backend_subscription->match_type =
        network::mojom::blink::CookieMatchType::STARTS_WITH;
  } else {
    DCHECK_EQ(subscription->matchType(), WTF::String("equals"));
    backend_subscription->match_type =
        network::mojom::blink::CookieMatchType::EQUALS;
  }

  if (subscription->hasName()) {
    backend_subscription->name = subscription->name();
  } else {
    // No name provided. Use a filter that matches all cookies. This overrides
    // a user-provided matchType.
    backend_subscription->match_type =
        network::mojom::blink::CookieMatchType::STARTS_WITH;
    backend_subscription->name = g_empty_string;
  }

  return backend_subscription;
}

CookieStoreGetOptions* ToCookieChangeSubscription(
    const mojom::blink::CookieChangeSubscription& backend_subscription) {
  CookieStoreGetOptions* subscription = CookieStoreGetOptions::Create();
  subscription->setURL(backend_subscription.url);

  if (backend_subscription.match_type !=
          network::mojom::blink::CookieMatchType::STARTS_WITH ||
      !backend_subscription.name.IsEmpty()) {
    subscription->setName(backend_subscription.name);
  }

  switch (backend_subscription.match_type) {
    case network::mojom::blink::CookieMatchType::STARTS_WITH:
      subscription->setMatchType(WTF::String("starts-with"));
      break;
    case network::mojom::blink::CookieMatchType::EQUALS:
      subscription->setMatchType(WTF::String("equals"));
      break;
  }

  return subscription;
}

KURL DefaultCookieURL(ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  return KURL(registration->scope());
}

}  // namespace

CookieStoreManager::CookieStoreManager(
    ServiceWorkerRegistration* registration,
    mojo::Remote<mojom::blink::CookieStore> backend)
    : registration_(registration),
      backend_(std::move(backend)),
      default_cookie_url_(DefaultCookieURL(registration)) {
  DCHECK(registration_);
  DCHECK(backend_);
}

CookieStoreManager::~CookieStoreManager() = default;

ScriptPromise CookieStoreManager::subscribe(
    ScriptState* script_state,
    const HeapVector<Member<CookieStoreGetOptions>>& subscriptions,
    ExceptionState& exception_state) {
  Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_subscriptions;
  backend_subscriptions.ReserveInitialCapacity(subscriptions.size());
  for (const CookieStoreGetOptions* subscription : subscriptions) {
    mojom::blink::CookieChangeSubscriptionPtr backend_subscription =
        ToBackendSubscription(default_cookie_url_, subscription,
                              exception_state);
    if (backend_subscription.is_null()) {
      DCHECK(exception_state.HadException());
      return ScriptPromise();
    }
    backend_subscriptions.push_back(std::move(backend_subscription));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->AddSubscriptions(
      registration_->RegistrationId(), std::move(backend_subscriptions),
      WTF::Bind(&CookieStoreManager::OnSubscribeResult, WrapPersistent(this),
                WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CookieStoreManager::unsubscribe(
    ScriptState* script_state,
    const HeapVector<Member<CookieStoreGetOptions>>& subscriptions,
    ExceptionState& exception_state) {
  Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_subscriptions;
  backend_subscriptions.ReserveInitialCapacity(subscriptions.size());
  for (const CookieStoreGetOptions* subscription : subscriptions) {
    mojom::blink::CookieChangeSubscriptionPtr backend_subscription =
        ToBackendSubscription(default_cookie_url_, subscription,
                              exception_state);
    if (backend_subscription.is_null()) {
      DCHECK(exception_state.HadException());
      return ScriptPromise();
    }
    backend_subscriptions.push_back(std::move(backend_subscription));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->RemoveSubscriptions(
      registration_->RegistrationId(), std::move(backend_subscriptions),
      WTF::Bind(&CookieStoreManager::OnSubscribeResult, WrapPersistent(this),
                WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CookieStoreManager::getSubscriptions(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "CookieStore backend went away");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->GetSubscriptions(
      registration_->RegistrationId(),
      WTF::Bind(&CookieStoreManager::OnGetSubscriptionsResult,
                WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

void CookieStoreManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

void CookieStoreManager::OnSubscribeResult(ScriptPromiseResolver* resolver,
                                           bool backend_success) {
  if (!backend_success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "An unknown error occured while subscribing to cookie changes."));
    return;
  }
  resolver->Resolve();
}

void CookieStoreManager::OnGetSubscriptionsResult(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_result,
    bool backend_success) {
  if (!backend_success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "An unknown error occured while reading cookie change subscriptions."));
    return;
  }

  HeapVector<Member<CookieStoreGetOptions>> subscriptions;
  subscriptions.ReserveInitialCapacity(backend_result.size());
  for (const auto& backend_subscription : backend_result) {
    CookieStoreGetOptions* subscription =
        ToCookieChangeSubscription(*backend_subscription);
    subscriptions.push_back(subscription);
  }

  resolver->Resolve(std::move(subscriptions));
}

}  // namespace blink
