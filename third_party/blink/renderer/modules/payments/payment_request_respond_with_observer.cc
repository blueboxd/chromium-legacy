// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_respond_with_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_handler_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_response.h"
#include "third_party/blink/renderer/modules/payments/payment_handler_utils.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

using payments::mojom::blink::PaymentEventResponseType;

}  // namespace

PaymentRequestRespondWithObserver* PaymentRequestRespondWithObserver::Create(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer) {
  return MakeGarbageCollected<PaymentRequestRespondWithObserver>(
      context, event_id, observer);
}

void PaymentRequestRespondWithObserver::OnResponseRejected(
    mojom::ServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "PaymentRequestEvent", error);
  Respond("", "",
          error == mojom::ServiceWorkerResponseError::kPromiseRejected
              ? PaymentEventResponseType::PAYMENT_EVENT_REJECT
              : PaymentEventResponseType::PAYMENT_EVENT_INTERNAL_ERROR);
}

void PaymentRequestRespondWithObserver::OnResponseFulfilled(
    const ScriptValue& value,
    ExceptionState::ContextType context_type,
    const char* interface_name,
    const char* property_name) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(value.GetIsolate(), context_type,
                                 interface_name, property_name);
  PaymentHandlerResponse* response =
      NativeValueTraits<PaymentHandlerResponse>::NativeValue(
          value.GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    OnResponseRejected(mojom::ServiceWorkerResponseError::kNoV8Instance);
    return;
  }

  // Check payment response validity.
  if (!response->hasMethodName() || response->methodName().IsEmpty() ||
      !response->hasDetails() || response->details().IsNull() ||
      !response->details().IsObject()) {
    GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kError,
                               "'PaymentHandlerResponse.methodName' and "
                               "'PaymentHandlerResponse.details' must not "
                               "be empty in payment response."));
  }

  if (!response->hasMethodName() || response->methodName().IsEmpty()) {
    Respond("", "", PaymentEventResponseType::PAYMENT_METHOD_NAME_EMPTY);
    return;
  }

  if (!response->hasDetails()) {
    Respond("", "", PaymentEventResponseType::PAYMENT_DETAILS_ABSENT);
    return;
  }

  if (response->details().IsNull() || !response->details().IsObject() ||
      response->details().IsEmpty()) {
    Respond("", "", PaymentEventResponseType::PAYMENT_DETAILS_NOT_OBJECT);
    return;
  }

  v8::Local<v8::String> details_value;
  if (!v8::JSON::Stringify(response->details().GetContext(),
                           response->details().V8Value().As<v8::Object>())
           .ToLocal(&details_value)) {
    GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Failed to stringify PaymentHandlerResponse.details in payment "
        "response."));
    Respond("", "", PaymentEventResponseType::PAYMENT_DETAILS_STRINGIFY_ERROR);
    return;
  }

  String details = ToCoreString(details_value);
  DCHECK(!details.IsEmpty());

  Respond(response->methodName(), details,
          PaymentEventResponseType::PAYMENT_EVENT_SUCCESS);
}

void PaymentRequestRespondWithObserver::OnNoResponse() {
  Respond("", "", PaymentEventResponseType::PAYMENT_EVENT_NO_RESPONSE);
}

PaymentRequestRespondWithObserver::PaymentRequestRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

void PaymentRequestRespondWithObserver::Trace(blink::Visitor* visitor) {
  RespondWithObserver::Trace(visitor);
}

void PaymentRequestRespondWithObserver::Respond(
    const String& method_name,
    const String& stringified_details,
    PaymentEventResponseType response_type) {
  DCHECK(GetExecutionContext());
  To<ServiceWorkerGlobalScope>(GetExecutionContext())
      ->RespondToPaymentRequestEvent(
          event_id_, payments::mojom::blink::PaymentHandlerResponse::New(
                         method_name, stringified_details, response_type));
}

}  // namespace blink
