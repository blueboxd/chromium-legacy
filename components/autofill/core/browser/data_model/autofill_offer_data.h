// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "url/gurl.h"

namespace autofill {

namespace payments {

// Represents an offer for certain merchants redeemable with certain cards.
struct AutofillOfferData {
 public:
  AutofillOfferData();
  ~AutofillOfferData();
  AutofillOfferData(const AutofillOfferData&);
  AutofillOfferData& operator=(const AutofillOfferData&);

  // The unique server id of this offer.
  std::string offer_id;
  // The name of this offer.
  base::string16 name;
  // The description of this offer.
  base::string16 description;
  // The expiration timestamp of this offer, in the form of seconds since Unix
  // epoch.
  int64_t expiry;
  // The merchant URL where this offer can be redeemed.
  GURL merchant_domain;

  // The ids of the cards this offer can be applied to.
  std::vector<std::string> eligible_instrument_id;
  // The legacy ids of the cards this offer can be applied to.
  std::vector<std::string> eligible_legacy_instrument_id;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
