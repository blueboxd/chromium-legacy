// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_FAKE_SERVICE_WORKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_FAKE_SERVICE_WORKER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

namespace content {

class EmbeddedWorkerTestHelper;

// The default fake for blink::mojom::ServiceWorker. It responds to event
// dispatches with success. It is owned by EmbeddedWorkerTestHelper and
// by default the lifetime is tied to the Mojo connection.
class FakeServiceWorker : public blink::mojom::ServiceWorker {
 public:
  using FetchHandlerExistence = blink::mojom::FetchHandlerExistence;

  // |helper| must outlive this instance.
  explicit FakeServiceWorker(EmbeddedWorkerTestHelper* helper);

  ~FakeServiceWorker() override;

  blink::mojom::ServiceWorkerHostAssociatedPtr& host() { return host_; }

  EmbeddedWorkerTestHelper* helper() { return helper_; }

  void Bind(blink::mojom::ServiceWorkerRequest request);

  // Returns after InitializeGlobalScope() is called.
  void RunUntilInitializeGlobalScope();

  bool is_zero_idle_timer_delay() const { return is_zero_idle_timer_delay_; }

  FetchHandlerExistence fetch_handler_existence() const {
    return fetch_handler_existence_;
  }

 protected:
  // blink::mojom::ServiceWorker overrides:
  void InitializeGlobalScope(
      blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
      FetchHandlerExistence fetch_handler_existence) override;
  void DispatchInstallEvent(DispatchInstallEventCallback callback) override;
  void DispatchActivateEvent(DispatchActivateEventCallback callback) override;
  void DispatchBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchAbortEventCallback callback) override;
  void DispatchBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchClickEventCallback callback) override;
  void DispatchBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchFailEventCallback callback) override;
  void DispatchBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchSuccessEventCallback callback) override;
  void DispatchCookieChangeEvent(
      const net::CanonicalCookie& cookie,
      ::network::mojom::CookieChangeCause cause,
      DispatchCookieChangeEventCallback callback) override;
  void DispatchFetchEventForMainResource(
      blink::mojom::DispatchFetchEventParamsPtr params,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventForMainResourceCallback callback) override;
  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      DispatchNotificationClickEventCallback callback) override;
  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      DispatchNotificationCloseEventCallback callback) override;
  void DispatchPushEvent(const base::Optional<std::string>& payload,
                         DispatchPushEventCallback callback) override;
  void DispatchPushSubscriptionChangeEvent(
      blink::mojom::PushSubscriptionPtr old_subscription,
      blink::mojom::PushSubscriptionPtr new_subscription,
      DispatchPushSubscriptionChangeEventCallback callback) override;
  void DispatchSyncEvent(const std::string& tag,
                         bool last_chance,
                         base::TimeDelta timeout,
                         DispatchSyncEventCallback callback) override;
  void DispatchPeriodicSyncEvent(
      const std::string& tag,
      base::TimeDelta timeout,
      DispatchPeriodicSyncEventCallback callback) override;
  void DispatchAbortPaymentEvent(
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchAbortPaymentEventCallback callback) override;
  void DispatchCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchCanMakePaymentEventCallback callback) override;
  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchPaymentRequestEventCallback callback) override;
  void DispatchExtendableMessageEvent(
      blink::mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override;
  void DispatchExtendableMessageEventWithCustomTimeout(
      blink::mojom::ExtendableMessageEventPtr event,
      base::TimeDelta timeout,
      DispatchExtendableMessageEventWithCustomTimeoutCallback callback)
      override;
  void DispatchContentDeleteEvent(
      const std::string& id,
      DispatchContentDeleteEventCallback callback) override;
  void Ping(PingCallback callback) override;
  void SetIdleTimerDelayToZero() override;

  virtual void OnConnectionError();

 private:
  void CallOnConnectionError();

  // |helper_| owns |this|.
  EmbeddedWorkerTestHelper* const helper_;

  blink::mojom::ServiceWorkerHostAssociatedPtr host_;
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info_;
  FetchHandlerExistence fetch_handler_existence_ =
      FetchHandlerExistence::UNKNOWN;
  base::OnceClosure quit_closure_for_initialize_global_scope_;

  mojo::Binding<blink::mojom::ServiceWorker> binding_;

  bool is_zero_idle_timer_delay_ = false;
};

}  // namespace content
#endif  // CONTENT_BROWSER_SERVICE_WORKER_FAKE_SERVICE_WORKER_H_
