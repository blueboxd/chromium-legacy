// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/keyed_service/core/keyed_service.h"

// Constant used to set intervals between calls for new offer data.
static const int64_t kOfferDataExpiryTimeInMicros =
    int64_t{1000000 * 60 * 60 * 24};  // 24 hours

// Constant used to set expiry time for a single call before the next call can
// be made.
static const int64_t kRequestExpiryTimeInMicros =
    int64_t{1000000 * 60};  // 1 minute

namespace autofill {

class AutofillClient;

namespace payments {

// Manages all Autofill related offers. One per frame; owned by the
// AutofillManager.
class AutofillOfferManager : public KeyedService {
 public:
  AutofillOfferManager();
  ~AutofillOfferManager() override;
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;

  void Init(AutofillClient* client, const std::string& app_locale);

 private:
  // Helper function used as callback when using |request_timer_|
  void OnRequestTimeout();

  // Callback function after successfully retrieving offer data.
  void OnDidGetOfferData(AutofillClient::PaymentsRpcResult result,
                         const std::vector<AutofillOfferData>& offers);

  // The time the offer data was last retrieved from Payments.
  base::Time last_updated_timestamp_;

  // Bool used to track if a request has been sent.
  bool request_is_active_ = false;

  // Timer used to wait for sent requests to come back before sending another.
  base::OneShotTimer request_timer_;

  base::WeakPtrFactory<AutofillOfferManager> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest, InitFirstCallSucceeds);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest,
                           InitBeforeOfferDataExpiry_OneSecond);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest,
                           InitAfterOfferDataExpiry_OneSecond);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest,
                           InitBeforeTimerExpiry_OneSecond);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest,
                           InitAfterTimerExpiry_OneSecond);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest,
                           InitAfterOfferDataExpiryButRequestActive);
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
