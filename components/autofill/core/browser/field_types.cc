// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_types.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

bool IsFillableFieldType(ServerFieldType field_type) {
  switch (field_type) {
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_LINE3:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
    case ADDRESS_HOME_STREET:
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_FLOOR:
    case ADDRESS_HOME_OTHER_SUBUNIT:
      return true;

    // Billing address types that should not be returned by GetStorableType().
    case NAME_BILLING_FIRST:
    case NAME_BILLING_MIDDLE:
    case NAME_BILLING_LAST:
    case NAME_BILLING_MIDDLE_INITIAL:
    case NAME_BILLING_FULL:
    case NAME_BILLING_SUFFIX:
    case PHONE_BILLING_NUMBER:
    case PHONE_BILLING_CITY_CODE:
    case PHONE_BILLING_COUNTRY_CODE:
    case PHONE_BILLING_CITY_AND_NUMBER:
    case PHONE_BILLING_WHOLE_NUMBER:
    case ADDRESS_BILLING_LINE1:
    case ADDRESS_BILLING_LINE2:
    case ADDRESS_BILLING_LINE3:
    case ADDRESS_BILLING_APT_NUM:
    case ADDRESS_BILLING_CITY:
    case ADDRESS_BILLING_STATE:
    case ADDRESS_BILLING_ZIP:
    case ADDRESS_BILLING_COUNTRY:
    case ADDRESS_BILLING_STREET_ADDRESS:
    case ADDRESS_BILLING_SORTING_CODE:
    case ADDRESS_BILLING_DEPENDENT_LOCALITY:
      NOTREACHED();
      return false;

    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
      return true;

    case UPI_VPA:
      return base::FeatureList::IsEnabled(features::kAutofillSaveAndFillVPA);

    case COMPANY_NAME:
      return base::FeatureList::IsEnabled(features::kAutofillEnableCompanyName);

    // Fillable credential fields.
    case USERNAME:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case SINGLE_USERNAME:
      return true;

    // Not fillable credential fields.
    case NOT_PASSWORD:
    case NOT_USERNAME:
      return false;

    // Credential field types that the server should never return as
    // classifications.
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
      return false;

    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case PHONE_FAX_NUMBER:
    case PHONE_FAX_CITY_CODE:
    case PHONE_FAX_COUNTRY_CODE:
    case PHONE_FAX_CITY_AND_NUMBER:
    case PHONE_FAX_WHOLE_NUMBER:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case MERCHANT_PROMO_CODE:
    case PRICE:
    case SEARCH_TERM:
    case UNKNOWN_TYPE:
    case MAX_VALID_FIELD_TYPE:
      return false;
  }
  return false;
}

base::StringPiece FieldTypeToStringPiece(HtmlFieldType type) {
  switch (type) {
    case HTML_TYPE_UNSPECIFIED:
      return "HTML_TYPE_UNSPECIFIED";
    case HTML_TYPE_NAME:
      return "HTML_TYPE_NAME";
    case HTML_TYPE_HONORIFIC_PREFIX:
      return "HTML_TYPE_HONORIFIC_PREFIX";
    case HTML_TYPE_GIVEN_NAME:
      return "HTML_TYPE_GIVEN_NAME";
    case HTML_TYPE_ADDITIONAL_NAME:
      return "HTML_TYPE_ADDITIONAL_NAME";
    case HTML_TYPE_FAMILY_NAME:
      return "HTML_TYPE_FAMILY_NAME";
    case HTML_TYPE_ORGANIZATION:
      return "HTML_TYPE_ORGANIZATION";
    case HTML_TYPE_STREET_ADDRESS:
      return "HTML_TYPE_STREET_ADDRESS";
    case HTML_TYPE_ADDRESS_LINE1:
      return "HTML_TYPE_ADDRESS_LINE1";
    case HTML_TYPE_ADDRESS_LINE2:
      return "HTML_TYPE_ADDRESS_LINE2";
    case HTML_TYPE_ADDRESS_LINE3:
      return "HTML_TYPE_ADDRESS_LINE3";
    case HTML_TYPE_ADDRESS_LEVEL1:
      return "HTML_TYPE_ADDRESS_LEVEL1";
    case HTML_TYPE_ADDRESS_LEVEL2:
      return "HTML_TYPE_ADDRESS_LEVEL2";
    case HTML_TYPE_ADDRESS_LEVEL3:
      return "HTML_TYPE_ADDRESS_LEVEL3";
    case HTML_TYPE_COUNTRY_CODE:
      return "HTML_TYPE_COUNTRY_CODE";
    case HTML_TYPE_COUNTRY_NAME:
      return "HTML_TYPE_COUNTRY_NAME";
    case HTML_TYPE_POSTAL_CODE:
      return "HTML_TYPE_POSTAL_CODE";
    case HTML_TYPE_FULL_ADDRESS:
      return "HTML_TYPE_FULL_ADDRESS";
    case HTML_TYPE_CREDIT_CARD_NAME_FULL:
      return "HTML_TYPE_CREDIT_CARD_NAME_FULL";
    case HTML_TYPE_CREDIT_CARD_NAME_FIRST:
      return "HTML_TYPE_CREDIT_CARD_NAME_FIRST";
    case HTML_TYPE_CREDIT_CARD_NAME_LAST:
      return "HTML_TYPE_CREDIT_CARD_NAME_LAST";
    case HTML_TYPE_CREDIT_CARD_NUMBER:
      return "HTML_TYPE_CREDIT_CARD_NUMBER";
    case HTML_TYPE_CREDIT_CARD_EXP:
      return "HTML_TYPE_CREDIT_CARD_EXP";
    case HTML_TYPE_CREDIT_CARD_EXP_MONTH:
      return "HTML_TYPE_CREDIT_CARD_EXP_MONTH";
    case HTML_TYPE_CREDIT_CARD_EXP_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_YEAR";
    case HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE:
      return "HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE";
    case HTML_TYPE_CREDIT_CARD_TYPE:
      return "HTML_TYPE_CREDIT_CARD_TYPE";
    case HTML_TYPE_TEL:
      return "HTML_TYPE_TEL";
    case HTML_TYPE_TEL_COUNTRY_CODE:
      return "HTML_TYPE_TEL_COUNTRY_CODE";
    case HTML_TYPE_TEL_NATIONAL:
      return "HTML_TYPE_TEL_NATIONAL";
    case HTML_TYPE_TEL_AREA_CODE:
      return "HTML_TYPE_TEL_AREA_CODE";
    case HTML_TYPE_TEL_LOCAL:
      return "HTML_TYPE_TEL_LOCAL";
    case HTML_TYPE_TEL_LOCAL_PREFIX:
      return "HTML_TYPE_TEL_LOCAL_PREFIX";
    case HTML_TYPE_TEL_LOCAL_SUFFIX:
      return "HTML_TYPE_TEL_LOCAL_SUFFIX";
    case HTML_TYPE_TEL_EXTENSION:
      return "HTML_TYPE_TEL_EXTENSION";
    case HTML_TYPE_EMAIL:
      return "HTML_TYPE_EMAIL";
    case HTML_TYPE_TRANSACTION_AMOUNT:
      return "HTML_TYPE_TRANSACTION_AMOUNT";
    case HTML_TYPE_TRANSACTION_CURRENCY:
      return "HTML_TYPE_TRANSACTION_CURRENCY";
    case HTML_TYPE_ADDITIONAL_NAME_INITIAL:
      return "HTML_TYPE_ADDITIONAL_NAME_INITIAL";
    case HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR";
    case HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR";
    case HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR";
    case HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR";
    case HTML_TYPE_UPI_VPA:
      return "HTML_TYPE_UPI_VPA";
    case HTML_TYPE_UNRECOGNIZED:
      return "HTML_TYPE_UNRECOGNIZED";
  }
  NOTREACHED();
  return "";
}

}  // namespace autofill
