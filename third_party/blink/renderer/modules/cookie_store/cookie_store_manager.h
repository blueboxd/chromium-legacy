// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_MANAGER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CookieStoreGetOptions;
class ScriptPromiseResolver;
class ScriptState;

class CookieStoreManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CookieStoreManager(ServiceWorkerRegistration* registration,
                     mojo::Remote<mojom::blink::CookieStore> backend);
  // Needed because of the
  // mojo::Remote<network::mojom::blink::CookieStore>
  ~CookieStoreManager() override;

  ScriptPromise subscribe(
      ScriptState* script_state,
      const HeapVector<Member<CookieStoreGetOptions>>& subscriptions,
      ExceptionState& exception_state);
  ScriptPromise unsubscribe(
      ScriptState* script_state,
      const HeapVector<Member<CookieStoreGetOptions>>& subscription,
      ExceptionState& exception_state);
  ScriptPromise getSubscriptions(ScriptState* script_state,
                                 ExceptionState& exception_state);

  // GarbageCollected
  void Trace(blink::Visitor* visitor) override;

 private:
  // The non-static callbacks keep CookieStoreManager alive during mojo calls.
  //
  // The browser-side implementation of the mojo calls assumes the SW
  // registration is live. When CookieStoreManager is used from a Window global,
  // the CookieStoreManager needs to live through the mojo call, so it can keep
  // its ServiceWorkerRegistration alive.
  void OnSubscribeResult(ScriptPromiseResolver* resolver, bool backend_result);
  void OnGetSubscriptionsResult(
      ScriptPromiseResolver* resolver,
      Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_result,
      bool backend_success);

  // SW registration whose cookie change subscriptions are managed by this.
  Member<ServiceWorkerRegistration> registration_;

  // Wraps a Mojo pipe for managing service worker cookie change subscriptions.
  mojo::Remote<mojom::blink::CookieStore> backend_;

  // Default for cookie_url in CookieStoreGetOptions.
  //
  // This is the Service Worker registration's scope.
  const KURL default_cookie_url_;

  // The context in which cookies are accessed.
  const scoped_refptr<SecurityOrigin> default_top_frame_origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_MANAGER_H_
