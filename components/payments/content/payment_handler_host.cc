// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_handler_host.h"

#include <utility>

#include "base/callback.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "content/public/browser/browser_thread.h"

namespace payments {

PaymentHandlerHost::PaymentHandlerHost(Delegate* delegate)
    : binding_(this), delegate_(delegate) {
  DCHECK(delegate_);
}

PaymentHandlerHost::~PaymentHandlerHost() {}

mojom::PaymentHandlerHostPtrInfo PaymentHandlerHost::Bind() {
  mojom::PaymentHandlerHostPtrInfo host_ptr_info;
  binding_.Close();
  binding_.Bind(mojo::MakeRequest(&host_ptr_info));

  // Connection error handler can be set only after the Bind() call.
  binding_.set_connection_error_handler(base::BindOnce(
      &PaymentHandlerHost::Disconnect, weak_ptr_factory_.GetWeakPtr()));

  return host_ptr_info;
}

void PaymentHandlerHost::UpdateWith(
    mojom::PaymentMethodChangeResponsePtr response) {
  if (!change_payment_method_callback_)
    return;

  std::move(change_payment_method_callback_).Run(std::move(response));
}

void PaymentHandlerHost::NoUpdatedPaymentDetails() {
  if (!change_payment_method_callback_)
    return;

  std::move(change_payment_method_callback_)
      .Run(mojom::PaymentMethodChangeResponse::New());
}

void PaymentHandlerHost::Disconnect() {
  binding_.Close();
}

void PaymentHandlerHost::ChangePaymentMethod(
    mojom::PaymentHandlerMethodDataPtr method_data,
    mojom::PaymentHandlerHost::ChangePaymentMethodCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!method_data) {
    mojom::PaymentMethodChangeResponsePtr response =
        mojom::PaymentMethodChangeResponse::New();
    response->error = errors::kMethodDataRequired;
    std::move(callback).Run(std::move(response));
    return;
  }

  if (method_data->method_name.empty()) {
    mojom::PaymentMethodChangeResponsePtr response =
        mojom::PaymentMethodChangeResponse::New();
    response->error = errors::kMethodNameRequired;
    std::move(callback).Run(std::move(response));
    return;
  }

  if (!delegate_->ChangePaymentMethod(method_data->method_name,
                                      method_data->stringified_data)) {
    mojom::PaymentMethodChangeResponsePtr response =
        mojom::PaymentMethodChangeResponse::New();
    response->error = errors::kInvalidState;
    std::move(callback).Run(std::move(response));
    return;
  }

  change_payment_method_callback_ = std::move(callback);
}

}  // namespace payments
