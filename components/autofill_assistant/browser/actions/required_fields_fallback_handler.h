// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REQUIRED_FIELDS_FALLBACK_HANDLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REQUIRED_FIELDS_FALLBACK_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace autofill_assistant {
class ClientStatus;

// A handler for required fields and fallback values, used for example in
// UseAddressAction.
class RequiredFieldsFallbackHandler {
 public:
  enum FieldValueStatus { UNKNOWN, EMPTY, NOT_EMPTY };
  struct RequiredField {
    Selector selector;
    bool simulate_key_presses = false;
    int delay_in_millisecond = 0;
    bool forced = false;
    FieldValueStatus status = UNKNOWN;

    // When filling in credit card, card_field must be set. When filling in
    // address, address_field must be set.
    UseAddressProto::RequiredField::AddressField address_field =
        UseAddressProto::RequiredField::UNDEFINED;
    UseCreditCardProto::RequiredField::CardField card_field =
        UseCreditCardProto::RequiredField::UNDEFINED;

    // Returns true if fallback is required for this field.
    bool ShouldFallback(bool has_fallback_data) const {
      return status == EMPTY || (forced && has_fallback_data);
    }
  };

  // Data necessary for filling in the fallback fields. This is kept in a
  // separate struct to make sure we don't keep it for longer than strictly
  // necessary.
  // TODO(marianfe): Refactor this to use a map instead.
  struct FallbackData {
    FallbackData();
    ~FallbackData() = default;

    // autofill profile.
    const autofill::AutofillProfile* profile;

    // Card information for UseCreditCard fallback.
    std::string cvc;
    std::string card_holder_name;
    std::string card_number;
    int expiration_year = 0;
    int expiration_month = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(FallbackData);
  };

  explicit RequiredFieldsFallbackHandler(
      const std::vector<RequiredField>& required_fields,
      base::RepeatingCallback<std::string(const RequiredField&,
                                          const FallbackData&)>
          field_value_getter,
      base::OnceCallback<void(const ClientStatus&)> status_update_callback,
      ActionDelegate* delegate);
  ~RequiredFieldsFallbackHandler();

  // Check whether all required fields have a non-empty value. If it is the
  // case, update the status to success. If it's not and |fallback_data|
  // is null, update the status to failure. If |fallback_data| is non-null, use
  // it to attempt to fill the failed fields without Autofill.
  void CheckAndFallbackRequiredFields(
      std::unique_ptr<FallbackData> fallback_data);

 private:
  // Triggers the check for a specific field.
  void CheckRequiredFieldsSequentially(
      bool allow_fallback,
      size_t required_fields_index,
      std::unique_ptr<FallbackData> fallback_data);

  // Updates the status of the required field.
  void OnGetRequiredFieldValue(size_t required_fields_index,
                               const ClientStatus& element_status,
                               const std::string& value);

  // Called when all required fields have been checked.
  void OnCheckRequiredFieldsDone(std::unique_ptr<FallbackData> fallback_data);

  // Sets fallback field values for empty fields.
  void SetFallbackFieldValuesSequentially(
      size_t required_fields_index,
      std::unique_ptr<FallbackData> fallback_data);

  // Called after trying to set form values without Autofill in case of fallback
  // after failed validation.
  void OnSetFallbackFieldValue(size_t required_fields_index,
                               std::unique_ptr<FallbackData> fallback_data,
                               const ClientStatus& status);

  std::vector<RequiredField> required_fields_;
  base::RepeatingCallback<std::string(const RequiredField&,
                                      const FallbackData&)>
      field_value_getter_;
  base::OnceCallback<void(const ClientStatus&)> status_update_callback_;
  ActionDelegate* action_delegate_;
  std::unique_ptr<BatchElementChecker> batch_element_checker_;
  base::WeakPtrFactory<RequiredFieldsFallbackHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RequiredFieldsFallbackHandler);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REQUIRED_FIELDS_FALLBACK_HANDLER_H_
