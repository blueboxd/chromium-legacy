// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace autofill {

struct AutofillOfferData {
  AutofillOfferData();
  ~AutofillOfferData();
  // The description of this offer.
  base::string16 description;
  // The name of this offer.
  base::string16 name;
  // The unique server id of this offer.
  std::string offer_id;
  // The ids of the cards this offer can be applied to.
  std::vector<std::string> eligible_card_id;
  // The merchant URL where this offer can be redeemed.
  std::vector<std::string> merchant_domain;
};

// Manages all Autofill related offers. One per frame; owned by the
// AutofillManager.
class AutofillOfferManager {
 public:
  AutofillOfferManager();
  virtual ~AutofillOfferManager();
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_