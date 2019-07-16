// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_HOST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_HOST_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"

namespace payments {

// Handles the communication from the payment handler renderer process to the
// merchant renderer process.
class PaymentHandlerHost : public mojom::PaymentHandlerHost {
 public:
  // The interfce to be implemented by the object that can communicate to the
  // merchant's renderer process.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notifies the merchant that the payment method has changed. Returns
    // "false" if the state is invalid.
    virtual bool ChangePaymentMethod(const std::string& method_name,
                                     const std::string& stringified_data) = 0;
  };

  // The |delegate| cannot be null and must outlive this object. Typically this
  // is accomplished by the |delegate| owning this object.
  explicit PaymentHandlerHost(Delegate* delegate);
  ~PaymentHandlerHost() override;

  // Returns "true" when the payment handler has changed the payment method, but
  // has not received the response from the merchant yet.
  bool is_changing_payment_method() const {
    return !!change_payment_method_callback_;
  }

  // Binds to an IPC endpoint and returns it.
  mojom::PaymentHandlerHostPtrInfo Bind();

  // Notifies the payment handler of the updated details, such as updated total,
  // in response to the change of the payment method.
  void UpdateWith(mojom::PaymentMethodChangeResponsePtr response);

  // Notifies the payment handler that the merchant did not handle the payment
  // method change event, so the payment details are unchanged.
  void NoUpdatedPaymentDetails();

  // Disconnects from the payment handler.
  void Disconnect();

 private:
  // mojom::PaymentHandlerHost
  void ChangePaymentMethod(
      mojom::PaymentHandlerMethodDataPtr method_data,
      mojom::PaymentHandlerHost::ChangePaymentMethodCallback callback) override;

  // Payment handler's callback to invoke after merchant responds to the
  // "payment method change" event.
  mojom::PaymentHandlerHost::ChangePaymentMethodCallback
      change_payment_method_callback_;

  // The end-point for the payment handler renderer process to call into the
  // browser process.
  mojo::Binding<mojom::PaymentHandlerHost> binding_;

  // Not null and outlives this object. Owns this object.
  Delegate* delegate_;

  base::WeakPtrFactory<PaymentHandlerHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerHost);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_HOST_H_
