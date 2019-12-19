// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <numeric>
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"

namespace autofill_assistant {
namespace {

// TODO: Share this helper function with use_address_action.
base::string16 GetProfileFullName(const autofill::AutofillProfile& profile) {
  return autofill::data_util::JoinNameParts(
      profile.GetRawInfo(autofill::NAME_FIRST),
      profile.GetRawInfo(autofill::NAME_MIDDLE),
      profile.GetRawInfo(autofill::NAME_LAST));
}

int CountCompleteContactFields(const CollectUserDataOptions& options,
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
bool CompletenessCompareContacts(const CollectUserDataOptions& options,
                                 const autofill::AutofillProfile& a,
                                 const autofill::AutofillProfile& b) {
  int complete_fields_a = CountCompleteContactFields(options, a);
  int complete_fields_b = CountCompleteContactFields(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(GetProfileFullName(a))
               .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

int GetAddressCompletenessRating(const CollectUserDataOptions& options,
                                 const autofill::AutofillProfile& profile) {
  auto address_data =
      autofill::i18n::CreateAddressDataFromAutofillProfile(profile, "en-US");
  std::multimap<i18n::addressinput::AddressField,
                i18n::addressinput::AddressProblem>
      problems;
  autofill::addressinput::ValidateRequiredFields(
      *address_data, /* filter= */ nullptr, &problems);
  return -problems.size();
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareAddresses(const CollectUserDataOptions& options,
                                  const autofill::AutofillProfile& a,
                                  const autofill::AutofillProfile& b) {
  int complete_fields_a = GetAddressCompletenessRating(options, a);
  int complete_fields_b = GetAddressCompletenessRating(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(GetProfileFullName(a))
               .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

int CountCompletePaymentInstrumentFields(const CollectUserDataOptions& options,
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
bool CompletenessComparePaymentInstruments(
    const CollectUserDataOptions& options,
    const PaymentInstrument& a,
    const PaymentInstrument& b) {
  int complete_fields_a = CountCompletePaymentInstrumentFields(options, a);
  int complete_fields_b = CountCompletePaymentInstrumentFields(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(
               a.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
               .compare(base::i18n::ToLower(
                   b.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

}  // namespace

std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::sort(profile_indices.begin(), profile_indices.end(),
            [&collect_user_data_options, &profiles](int i, int j) {
              return CompletenessCompareContacts(collect_user_data_options,
                                                 *profiles[i], *profiles[j]);
            });
  return profile_indices;
}

int GetDefaultContactProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortContactsByCompleteness(collect_user_data_options, profiles);
  if (!collect_user_data_options.default_email.empty()) {
    for (int index : sorted_indices) {
      if (base::UTF16ToUTF8(
              profiles[index]->GetRawInfo(autofill::EMAIL_ADDRESS)) ==
          collect_user_data_options.default_email) {
        return index;
      }
    }
  }
  return sorted_indices[0];
}

std::vector<int> SortAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::sort(profile_indices.begin(), profile_indices.end(),
            [&collect_user_data_options, &profiles](int i, int j) {
              return CompletenessCompareAddresses(collect_user_data_options,
                                                  *profiles[i], *profiles[j]);
            });
  return profile_indices;
}

int GetDefaultAddressProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortContactsByCompleteness(collect_user_data_options, profiles);
  return sorted_indices[0];
}

std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  std::vector<int> payment_instrument_indices(payment_instruments.size());
  std::iota(std::begin(payment_instrument_indices),
            std::end(payment_instrument_indices), 0);
  std::sort(payment_instrument_indices.begin(),
            payment_instrument_indices.end(),
            [&collect_user_data_options, &payment_instruments](int a, int b) {
              return CompletenessComparePaymentInstruments(
                  collect_user_data_options, *payment_instruments[a],
                  *payment_instruments[b]);
            });
  return payment_instrument_indices;
}

int GetDefaultPaymentInstrument(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  if (payment_instruments.empty()) {
    return -1;
  }
  auto sorted_indices = SortPaymentInstrumentsByCompleteness(
      collect_user_data_options, payment_instruments);
  return sorted_indices[0];
}

bool CompareContactDetails(
    const CollectUserDataOptions& collect_user_data_options,
    const autofill::AutofillProfile* a,
    const autofill::AutofillProfile* b) {
  std::vector<autofill::ServerFieldType> types;
  if (collect_user_data_options.request_payer_name) {
    types.emplace_back(autofill::NAME_FULL);
    types.emplace_back(autofill::NAME_FIRST);
    types.emplace_back(autofill::NAME_MIDDLE);
    types.emplace_back(autofill::NAME_LAST);
  }
  if (collect_user_data_options.request_payer_phone) {
    types.emplace_back(autofill::PHONE_HOME_WHOLE_NUMBER);
  }
  if (collect_user_data_options.request_payer_email) {
    types.emplace_back(autofill::EMAIL_ADDRESS);
  }
  if (types.empty()) {
    return a->guid() == b->guid();
  }

  for (auto type : types) {
    int comparison = a->GetRawInfo(type).compare(b->GetRawInfo(type));
    if (comparison != 0) {
      return false;
    }
  }

  return true;
}

}  // namespace autofill_assistant
