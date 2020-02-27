// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_can_make_payment_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_utils.h"
#include "third_party/blink/renderer/modules/payments/payments_validators.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {

CanMakePaymentRespondWithObserver::CanMakePaymentRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

void CanMakePaymentRespondWithObserver::OnResponseRejected(
    blink::mojom::ServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "CanMakePaymentEvent", error);
  RespondCanMakePayment(false);
}

void CanMakePaymentRespondWithObserver::OnResponseFulfilled(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(script_state->GetIsolate(), context_type,
                                 interface_name, property_name);
  if (is_minimal_ui_) {
    OnResponseFulfilledForMinimalUI(script_state, value, exception_state);
    return;
  }

  bool can_make_payment =
      ToBoolean(script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    RespondCanMakePayment(false);
    return;
  }

  RespondCanMakePayment(can_make_payment);
}

void CanMakePaymentRespondWithObserver::OnNoResponse() {
  ConsoleWarning(
      "To control whether your payment handler can be used, handle the "
      "'canmakepayment' event explicitly. Otherwise, it is assumed implicitly "
      "that your payment handler can always be used.");
  RespondCanMakePayment(true);
}

void CanMakePaymentRespondWithObserver::Trace(Visitor* visitor) {
  RespondWithObserver::Trace(visitor);
}

void CanMakePaymentRespondWithObserver::RespondToCanMakePaymentEvent(
    ScriptState* script_state,
    ScriptPromise promise,
    ExceptionState& exception_state,
    bool is_minimal_ui) {
  is_minimal_ui_ = is_minimal_ui;
  RespondWith(script_state, promise, exception_state);
}

void CanMakePaymentRespondWithObserver::OnResponseFulfilledForMinimalUI(
    ScriptState* script_state,
    const ScriptValue& value,
    ExceptionState& exception_state) {
  CanMakePaymentResponse* response =
      NativeValueTraits<CanMakePaymentResponse>::NativeValue(
          script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    RespondCanMakePayment(false);
    return;
  }

  if (!response->hasCanMakePayment()) {
    ConsoleWarning(
        "To use minimal UI, specify the value of 'canMakePayment' explicitly. "
        "Otherwise, the value of 'false' is assumed implicitly.");
    RespondCanMakePayment(false);
    return;
  }

  if (!response->hasReadyForMinimalUI()) {
    ConsoleWarning(
        "To use minimal UI, specify the value of 'readyForMinimalUI' "
        "explicitly. Otherwise, the value of 'false' is assumed implicitly.");
    RespondCanMakePayment(response->canMakePayment());
    return;
  }

  if (!response->hasAccountBalance() || response->accountBalance().IsEmpty()) {
    ConsoleWarning(
        "To use minimal UI, specify 'accountBalance' value, e.g., '1.00'.");
    RespondCanMakePayment(response->canMakePayment());
    return;
  }

  String error_message;
  if (!PaymentsValidators::IsValidAmountFormat(
          response->accountBalance(), "account balance", &error_message)) {
    ConsoleWarning(error_message +
                   ". To use minimal UI, format 'accountBalance' as, for "
                   "example, '1.00'.");
    RespondCanMakePayment(response->canMakePayment());
    return;
  }

  RespondCanMakePayment(response->canMakePayment());
}

void CanMakePaymentRespondWithObserver::ConsoleWarning(const String& message) {
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

void CanMakePaymentRespondWithObserver::RespondCanMakePayment(
    bool can_make_payment) {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToCanMakePaymentEvent(event_id_, can_make_payment);
}

}  // namespace blink
