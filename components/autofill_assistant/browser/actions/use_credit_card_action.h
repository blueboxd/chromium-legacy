// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CREDIT_CARD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CREDIT_CARD_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {
class ClientStatus;

// An action to autofill a form using a credit card.
class UseCreditCardAction : public Action {
 public:
  explicit UseCreditCardAction(ActionDelegate* delegate,
                               const ActionProto& proto);
  ~UseCreditCardAction() override;

 private:
  enum FieldValueStatus { UNKNOWN, EMPTY, NOT_EMPTY };
  struct RequiredField {
    Selector selector;
    bool simulate_key_presses = false;
    int delay_in_millisecond = 0;
    bool forced = false;
    FieldValueStatus status = UNKNOWN;

    // When filling in credit card, card_field must be set.
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
  struct FallbackData {
    FallbackData();
    ~FallbackData() = default;

    // Card information for UseCreditCard fallback.
    std::string cvc;
    std::string card_holder_name;
    std::string card_number;
    int expiration_year = 0;
    int expiration_month = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(FallbackData);
  };

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(ProcessedActionStatusProto status);
  void EndAction(const ClientStatus& status);

  // Fill the form using data in client memory. Return whether filling succeeded
  // or not through OnFormFilled.
  void FillFormWithData();
  void OnWaitForElement(const ClientStatus& element_status);

  // Called after getting full credit card with its cvc.
  void OnGetFullCard(std::unique_ptr<autofill::CreditCard> card,
                     const base::string16& cvc);

  // Called when the form credit card has been filled.
  void OnFormFilled(std::unique_ptr<FallbackData> fallback_data,
                    const ClientStatus& status);

  // Check whether all required fields have a non-empty value. If it is the
  // case, finish the action successfully. If it's not and |fallback_data|
  // is null, fail the action. If |fallback_data| is non-null, use it to attempt
  // to fill the failed fields without Autofill.
  void CheckRequiredFields(std::unique_ptr<FallbackData> fallback_data);

  // Triggers the check for a specific field.
  void CheckRequiredFieldsSequentially(
      bool allow_fallback,
      size_t required_fields_index,
      std::unique_ptr<FallbackData> fallback_data);

  // Updates |required_fields_value_status_|.
  void OnGetRequiredFieldValue(size_t required_fields_index,
                               const ClientStatus& element_status,
                               const std::string& value);

  // Called when all required fields have been checked.
  void OnCheckRequiredFieldsDone(std::unique_ptr<FallbackData> fallback_data);

  // Gets the fallback value.
  std::string GetFallbackValue(const RequiredField& required_field,
                               const FallbackData& fallback_data);

  // Gets the value of |field| from |fallback_data|, if available. Returns an
  // empty string otherwise.
  std::string GetCreditCardFieldValue(
      UseCreditCardProto::RequiredField::CardField field,
      const FallbackData& fallback_data);

  // Sets fallback field values for empty fields from
  // |required_fields_value_status_|.
  void SetFallbackFieldValuesSequentially(
      size_t required_fields_index,
      std::unique_ptr<FallbackData> fallback_data);

  // Called after trying to set form values without Autofill in case of fallback
  // after failed validation.
  void OnSetFallbackFieldValue(size_t required_fields_index,
                               std::unique_ptr<FallbackData> fallback_data,
                               const ClientStatus& status);

  std::string prompt_;
  Selector selector_;

  std::vector<RequiredField> required_fields_;

  std::unique_ptr<BatchElementChecker> batch_element_checker_;

  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<UseCreditCardAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UseCreditCardAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
