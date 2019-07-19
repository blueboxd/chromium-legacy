// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/fake_service_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

FakeServiceWorker::FakeServiceWorker(EmbeddedWorkerTestHelper* helper)
    : helper_(helper), binding_(this) {}

FakeServiceWorker::~FakeServiceWorker() = default;

void FakeServiceWorker::Bind(blink::mojom::ServiceWorkerRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::BindOnce(
      &FakeServiceWorker::CallOnConnectionError, base::Unretained(this)));
}

void FakeServiceWorker::RunUntilInitializeGlobalScope() {
  if (host_)
    return;
  base::RunLoop loop;
  quit_closure_for_initialize_global_scope_ = loop.QuitClosure();
  loop.Run();
}

void FakeServiceWorker::InitializeGlobalScope(
    blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
    blink::mojom::FetchHandlerExistence fetch_handler_existence) {
  host_.Bind(std::move(service_worker_host));

  // Enable callers to use these endpoints without us actually binding them
  // to an implementation.
  mojo::AssociateWithDisconnectedPipe(registration_info->request.PassHandle());
  if (registration_info->installing) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->installing->request.PassHandle());
  }
  if (registration_info->waiting) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->waiting->request.PassHandle());
  }
  if (registration_info->active) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->active->request.PassHandle());
  }

  registration_info_ = std::move(registration_info);
  if (quit_closure_for_initialize_global_scope_)
    std::move(quit_closure_for_initialize_global_scope_).Run();

  fetch_handler_existence_ = fetch_handler_existence;
}

void FakeServiceWorker::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          true /* has_fetch_handler */);
}

void FakeServiceWorker::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchAbortEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchFailEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchSuccessEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchCookieChangeEvent(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    DispatchCookieChangeEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchFetchEventForMainResource(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventForMainResourceCallback callback) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response_callback->OnResponse(
      std::move(response), blink::mojom::ServiceWorkerFetchEventTiming::New());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPushEvent(
    const base::Optional<std::string>& payload,
    DispatchPushEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPushSubscriptionChangeEvent(
    blink::mojom::PushSubscriptionPtr old_subscription,
    blink::mojom::PushSubscriptionPtr new_subscription,
    DispatchPushSubscriptionChangeEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchSyncEvent(const std::string& tag,
                                          bool last_chance,
                                          base::TimeDelta timeout,
                                          DispatchSyncEventCallback callback) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::DispatchPeriodicSyncEvent(
    const std::string& tag,
    base::TimeDelta timeout,
    DispatchPeriodicSyncEventCallback callback) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::DispatchAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchAbortPaymentEventCallback callback) {
  response_callback->OnResponseForAbortPayment(true);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  response_callback->OnResponseForCanMakePayment(true);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchPaymentRequestEventCallback callback) {
  response_callback->OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponse::New());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchExtendableMessageEvent(
    blink::mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchExtendableMessageEventWithCustomTimeout(
    blink::mojom::ExtendableMessageEventPtr event,
    base::TimeDelta timeout,
    DispatchExtendableMessageEventWithCustomTimeoutCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchContentDeleteEvent(
    const std::string& id,
    DispatchContentDeleteEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void FakeServiceWorker::SetIdleTimerDelayToZero() {
  is_zero_idle_timer_delay_ = true;
}

void FakeServiceWorker::OnConnectionError() {
  // Destroys |this|.
  helper_->RemoveServiceWorker(this);
}

void FakeServiceWorker::CallOnConnectionError() {
  // Call OnConnectionError(), which subclasses can override.
  OnConnectionError();
}

}  // namespace content
