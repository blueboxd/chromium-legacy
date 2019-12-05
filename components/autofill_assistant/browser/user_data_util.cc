// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <numeric>
#include "base/i18n/case_conversion.h"
#include "components/autofill/core/browser/autofill_data_util.h"

namespace autofill_assistant {
namespace {

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

int CountCompleteFields(const CollectUserDataOptions& options,
                        const PaymentInstrument& instrument) {
  int complete_fields = 0;
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL).empty()) {
    ++complete_fields;
  }
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty()) {
    ++complete_fields;
  }
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH).empty()) {
    ++complete_fields;
  }
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)
           .empty()) {
    ++complete_fields;
  }
  if (instrument.billing_address != nullptr) {
    ++complete_fields;

    if (options.require_billing_postal_code &&
        !instrument.billing_address->GetRawInfo(autofill::ADDRESS_HOME_ZIP)
             .empty()) {
      ++complete_fields;
    }
  }
  return complete_fields;
}

// Helper function that compares instances of PaymentInstrument by completeness
// in regards to the current options. Full payment instruments should be
// ordered before empty ones and fall back to compare the full name on the
// credit card in case of equality.
bool CompletenessCompare(const CollectUserDataOptions& options,
                         const PaymentInstrument& a,
                         const PaymentInstrument& b) {
  int complete_fields_a = CountCompleteFields(options, a);
  int complete_fields_b = CountCompleteFields(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(
               a.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
               .compare(base::i18n::ToLower(
                   b.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

}  // namespace

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

std::vector<int> SortByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  std::vector<int> payment_instrument_indices(payment_instruments.size());
  std::iota(std::begin(payment_instrument_indices),
            std::end(payment_instrument_indices), 0);
  std::sort(payment_instrument_indices.begin(),
            payment_instrument_indices.end(),
            [&collect_user_data_options, &payment_instruments](int a, int b) {
              return CompletenessCompare(collect_user_data_options,
                                         *payment_instruments[a],
                                         *payment_instruments[b]);
            });
  return payment_instrument_indices;
}

}  // namespace autofill_assistant
