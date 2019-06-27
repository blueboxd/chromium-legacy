// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_RESPOND_WITH_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_RESPOND_WITH_OBSERVER_H_

#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// Implementation for PaymentRequestEvent.respondWith(), which is used by the
// payment handler to provide a payment response when the payment successfully
// completes.
class MODULES_EXPORT PaymentRequestRespondWithObserver final
    : public RespondWithObserver {
 public:
  PaymentRequestRespondWithObserver(ExecutionContext*,
                                    int event_id,
                                    WaitUntilObserver*);
  ~PaymentRequestRespondWithObserver() override = default;

  static PaymentRequestRespondWithObserver* Create(ExecutionContext*,
                                                   int event_id,
                                                   WaitUntilObserver*);

  void OnResponseRejected(mojom::ServiceWorkerResponseError) override;
  void OnResponseFulfilled(const ScriptValue&,
                           ExceptionState::ContextType,
                           const char* interface_name,
                           const char* property_name) override;
  void OnNoResponse() override;

  void Trace(blink::Visitor*) override;

 private:
  void Respond(const String& method_name,
               const String& stringified_details,
               payments::mojom::blink::PaymentEventResponseType response_type);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_RESPOND_WITH_OBSERVER_H_
