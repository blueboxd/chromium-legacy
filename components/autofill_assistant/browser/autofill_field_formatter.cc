// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_field_formatter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace {
// Regex to find placeholders of the form ${N}.
const char kPlaceholderExtractor[] = R"re(\$\{([^}]+)\})re";

template <class T>
base::Optional<std::string> GetFieldValue(const T& autofill_data_model,
                                          int field,
                                          const std::string& locale) {
  if (field >= autofill::MAX_VALID_FIELD_TYPE || field < 0) {
    return base::nullopt;
  }
  std::string value = base::UTF16ToUTF8(autofill_data_model.GetInfo(
      autofill::AutofillType(static_cast<autofill::ServerFieldType>(field)),
      locale));
  if (value.empty()) {
    return base::nullopt;
  }
  return value;
}

// Template specialization for credit cards.
template <>
base::Optional<std::string> GetFieldValue<autofill::CreditCard>(
    const autofill::CreditCard& credit_card,
    int field,
    const std::string& locale) {
  if (field >= 0) {
    return GetFieldValue<autofill::AutofillDataModel>(credit_card, field,
                                                      locale);
  }

  switch (field) {
    case autofill_assistant::AutofillFormatProto::CREDIT_CARD_NETWORK:
      return autofill::data_util::GetPaymentRequestData(credit_card.network())
          .basic_card_issuer_network;
    case autofill_assistant::AutofillFormatProto::
        CREDIT_CARD_NETWORK_FOR_DISPLAY:
      return base::UTF16ToUTF8(credit_card.NetworkForDisplay());
    case autofill_assistant::AutofillFormatProto::
        CREDIT_CARD_NUMBER_LAST_FOUR_DIGITS:
      return base::UTF16ToUTF8(credit_card.LastFourDigits());
    default:
      return base::nullopt;
  }
}

template <class T>
base::Optional<std::string> FormatDataModel(const T& autofill_data_model,
                                            const std::string& pattern,
                                            const std::string& locale) {
  if (pattern.empty()) {
    return std::string();
  }

  // Special case: if the input is a single number, interpret as ${N}.
  int field_type;
  if (base::StringToInt(pattern, &field_type)) {
    return GetFieldValue(autofill_data_model, field_type, locale);
  }

  std::string out = pattern;
  re2::StringPiece input(pattern);
  while (re2::RE2::FindAndConsume(&input, kPlaceholderExtractor, &field_type)) {
    auto rewrite_value = GetFieldValue(autofill_data_model, field_type, locale);
    if (!rewrite_value.has_value()) {
      VLOG(2) << "No value for " << field_type << " in " << pattern;
      return base::nullopt;
    }

    re2::RE2::Replace(&out, kPlaceholderExtractor,
                      re2::StringPiece(rewrite_value.value()));
  }

  return out;
}

}  // namespace

namespace autofill_assistant {
namespace autofill_field_formatter {

template <>
base::Optional<std::string> FormatString<autofill::AutofillProfile>(
    const autofill::AutofillProfile& autofill_data_model,
    const std::string& pattern,
    const std::string& locale) {
  return FormatDataModel(autofill_data_model, pattern, locale);
}

template <>
base::Optional<std::string> FormatString<autofill::CreditCard>(
    const autofill::CreditCard& credit_card,
    const std::string& pattern,
    const std::string& locale) {
  return FormatDataModel(credit_card, pattern, locale);
}

}  // namespace autofill_field_formatter
}  // namespace autofill_assistant
