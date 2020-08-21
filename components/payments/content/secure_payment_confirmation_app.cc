// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app.h"

#include <utility>

#include "base/notreached.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payer_data.h"

namespace payments {

SecurePaymentConfirmationApp::SecurePaymentConfirmationApp(
    std::unique_ptr<SkBitmap> icon,
    const base::string16& label,
    const url::Origin& merchant_origin,
    const mojom::PaymentCurrencyAmountPtr& total,
    const mojom::SecurePaymentConfirmationRequestPtr& request)
    : PaymentApp(/*icon_resource_id=*/0, PaymentApp::Type::INTERNAL),
      icon_(std::move(icon)),
      label_(label),
      merchant_origin_(merchant_origin),
      total_(total.Clone()),
      request_(request.Clone()) {
  app_method_names_.insert(methods::kSecurePaymentConfirmation);
}

SecurePaymentConfirmationApp::~SecurePaymentConfirmationApp() = default;

void SecurePaymentConfirmationApp::InvokePaymentApp(Delegate* delegate) {
  // TODO(https://crbug.com/1110324): Combine |merchant_origin_|, |total_|, and
  // |request_| into a challenge to invoke WebAuthn.
}

bool SecurePaymentConfirmationApp::IsCompleteForPayment() const {
  return true;
}

uint32_t SecurePaymentConfirmationApp::GetCompletenessScore() const {
  // This value is used for sorting multiple apps, but this app always appears
  // on its own.
  return 0;
}

bool SecurePaymentConfirmationApp::CanPreselect() const {
  return true;
}

base::string16 SecurePaymentConfirmationApp::GetMissingInfoLabel() const {
  NOTREACHED();
  return base::string16();
}

bool SecurePaymentConfirmationApp::HasEnrolledInstrument() const {
  // If there's no platform authenticator, then the factory should not create
  // this app. Therefore, this function can always return true.
  return true;
}

void SecurePaymentConfirmationApp::RecordUse() {
  NOTIMPLEMENTED();
}

bool SecurePaymentConfirmationApp::NeedsInstallation() const {
  return false;
}

std::string SecurePaymentConfirmationApp::GetId() const {
  return request_->instrument_id;
}

base::string16 SecurePaymentConfirmationApp::GetLabel() const {
  return label_;
}

base::string16 SecurePaymentConfirmationApp::GetSublabel() const {
  return base::string16();
}

const SkBitmap* SecurePaymentConfirmationApp::icon_bitmap() const {
  return icon_.get();
}

bool SecurePaymentConfirmationApp::IsValidForModifier(
    const std::string& method,
    bool supported_networks_specified,
    const std::set<std::string>& supported_networks) const {
  bool is_valid = false;
  IsValidForPaymentMethodIdentifier(method, &is_valid);
  return is_valid;
}

base::WeakPtr<PaymentApp> SecurePaymentConfirmationApp::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool SecurePaymentConfirmationApp::HandlesShippingAddress() const {
  return false;
}

bool SecurePaymentConfirmationApp::HandlesPayerName() const {
  return false;
}

bool SecurePaymentConfirmationApp::HandlesPayerEmail() const {
  return false;
}

bool SecurePaymentConfirmationApp::HandlesPayerPhone() const {
  return false;
}

bool SecurePaymentConfirmationApp::IsWaitingForPaymentDetailsUpdate() const {
  return false;
}

void SecurePaymentConfirmationApp::UpdateWith(
    mojom::PaymentRequestDetailsUpdatePtr details_update) {
  NOTREACHED();
}

void SecurePaymentConfirmationApp::OnPaymentDetailsNotUpdated() {
  NOTREACHED();
}

void SecurePaymentConfirmationApp::AbortPaymentApp(
    base::OnceCallback<void(bool)> abort_callback) {
  std::move(abort_callback).Run(/*abort_success=*/true);
}

}  // namespace payments
