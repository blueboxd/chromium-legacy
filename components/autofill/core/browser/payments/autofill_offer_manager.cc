// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "url/gurl.h"

namespace autofill {

namespace payments {

AutofillOfferManager::AutofillOfferManager() = default;

AutofillOfferManager::~AutofillOfferManager() = default;

void AutofillOfferManager::Init(AutofillClient* client,
                                const std::string& app_locale) {
  if (base::TimeDelta(AutofillClock::Now() - last_updated_timestamp_) >=
          base::TimeDelta::FromMicroseconds(kOfferDataExpiryTimeInMicros) &&
      !request_is_active_) {
    client->GetPaymentsClient()->GetOfferData(
        app_locale, base::BindOnce(&AutofillOfferManager::OnDidGetOfferData,
                                   weak_ptr_factory_.GetWeakPtr()));
    request_is_active_ = true;
    request_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMicroseconds(kRequestExpiryTimeInMicros), this,
        &AutofillOfferManager::OnRequestTimeout);
  }
}

void AutofillOfferManager::OnRequestTimeout() {
  request_is_active_ = false;
}

void AutofillOfferManager::OnDidGetOfferData(
    AutofillClient::PaymentsRpcResult result,
    const std::vector<AutofillOfferData>& offers) {
  request_timer_.Stop();
  request_is_active_ = false;
  if (result == AutofillClient::SUCCESS) {
    // TODO(crbug/1093057): Parse and store returned offer data.
    last_updated_timestamp_ = AutofillClock::Now();
  }
}

}  // namespace payments

}  // namespace autofill
