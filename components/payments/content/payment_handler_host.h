// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_HOST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_HOST_H_

#include <stdint.h>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

// Handles the communication from the payment handler renderer process to the
// merchant renderer process.
class PaymentHandlerHost : public mojom::PaymentHandlerHost {
 public:
  // The interface to be implemented by the object that can communicate to the
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
  // is accomplished by the |delegate| owning this object. The |web_contents| is
  // used for developer tools logging and should be from the same browser
  // context as the payment handler.
  PaymentHandlerHost(content::WebContents* web_contents, Delegate* delegate);
  ~PaymentHandlerHost() override;

  // Sets the origin of the payment handler / service worker registration scope.
  // Used for developer tools logging.
  void set_sw_origin_for_logs(const url::Origin& origin) {
    sw_origin_for_logs_ = origin;
  }

  // Sets the registration identifier of the payment handler / service worker.
  // Used for developer tools logging.
  void set_registration_id_for_logs(int64_t id) {
    registration_id_for_logs_ = id;
  }

  // Sets the identifier for the Payment Request object. Used for developer
  // tools logging.
  void set_payment_request_id_for_logs(const std::string& id) {
    payment_request_id_for_logs_ = id;
  }

  // Returns "true" when the payment handler has changed the payment method, but
  // has not received the response from the merchant yet.
  bool is_changing_payment_method() const {
    return !!change_payment_method_callback_;
  }

  // Binds to an IPC endpoint and returns it.
  mojo::PendingRemote<mojom::PaymentHandlerHost> Bind();

  // Notifies the payment handler of the updated details, such as updated total,
  // in response to the change of the payment method.
  void UpdateWith(mojom::PaymentMethodChangeResponsePtr response);

  // Notifies the payment handler that the merchant did not handle the payment
  // method change event, so the payment details are unchanged.
  void NoUpdatedPaymentDetails();

  // Disconnects from the payment handler.
  void Disconnect();

  base::WeakPtr<PaymentHandlerHost> AsWeakPtr();

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
  mojo::Receiver<mojom::PaymentHandlerHost> receiver_{this};

  // The merchant page that invoked the Payment Request API.
  content::WebContents* web_contents_;

  // Not null and outlives this object. Owns this object.
  Delegate* delegate_;

  // The origin of the payment handler / service worker registration scope. Used
  // for developer tools logging.
  url::Origin sw_origin_for_logs_;

  // The registration identifier for the payment handler / service worker. Used
  // for developer tools logging.
  int64_t registration_id_for_logs_ = -1;

  // The identifier for the Payment Request object. Used for developer tools
  // logging.
  std::string payment_request_id_for_logs_;

  base::WeakPtrFactory<PaymentHandlerHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerHost);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_HOST_H_
