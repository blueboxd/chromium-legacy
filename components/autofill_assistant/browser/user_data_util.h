// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_

#include <vector>
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

// Sorts the given autofill profiles based on completeness, and returns a
// vector of profile indices in sorted order. Full profiles will be ordered
// before empty ones, and for equally complete profiles, this falls back to
// sorting based on the profile names.
std::vector<int> SortByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Sorts the given payment instruments by completeness, and returns a vector
// of payment instrument indices in sorted order. Full payment instruments will
// be ordered before empty ones, and for equally complete payment instruments,
// this falls back to sorting based on the full name on the credit card.
std::vector<int> SortByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
