// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <numeric>
#include "base/i18n/case_conversion.h"
#include "components/autofill/core/browser/autofill_data_util.h"

namespace autofill_assistant {
// TODO: Share this helper function with use_address_action.
base::string16 GetProfileFullName(const autofill::AutofillProfile& profile) {
  return autofill::data_util::JoinNameParts(
      profile.GetRawInfo(autofill::NAME_FIRST),
      profile.GetRawInfo(autofill::NAME_MIDDLE),
      profile.GetRawInfo(autofill::NAME_LAST));
}

int CountCompleteFields(const CollectUserDataOptions& options,
                        const autofill::AutofillProfile& profile) {
  int completed_fields = 0;
  if (options.request_payer_name && !GetProfileFullName(profile).empty()) {
    ++completed_fields;
  }
  if (options.request_shipping &&
      !profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS).empty()) {
    ++completed_fields;
  }
  if (options.request_payer_email &&
      !profile.GetRawInfo(autofill::EMAIL_ADDRESS).empty()) {
    ++completed_fields;
  }
  if (options.request_payer_phone &&
      !profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty()) {
    ++completed_fields;
  }
  return completed_fields;
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompare(const CollectUserDataOptions& options,
                         const autofill::AutofillProfile& a,
                         const autofill::AutofillProfile& b) {
  int complete_fields_a = CountCompleteFields(options, a);
  int complete_fields_b = CountCompleteFields(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(GetProfileFullName(a))
               .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

std::vector<int> SortByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::sort(profile_indices.begin(), profile_indices.end(),
            [&collect_user_data_options, &profiles](int i, int j) {
              return CompletenessCompare(collect_user_data_options,
                                         *profiles[i], *profiles[j]);
            });
  return profile_indices;
}
}  // namespace autofill_assistant
