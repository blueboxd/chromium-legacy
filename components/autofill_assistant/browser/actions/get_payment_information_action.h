// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/payment_request.h"

namespace autofill_assistant {

// Triggers PaymentRequest to collect user data.
class GetPaymentInformationAction
    : public Action,
      public autofill::PersonalDataManagerObserver {
 public:
  explicit GetPaymentInformationAction(ActionDelegate* delegate,
                                       const ActionProto& proto);
  ~GetPaymentInformationAction() override;

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

 private:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnGetPaymentInformation(
      const GetPaymentInformationProto& get_payment_information,
      std::unique_ptr<PaymentInformation> payment_information);
  void OnAdditionalActionTriggered(int index);
  void OnTermsAndConditionsLinkClicked(int link);

  // Creates a new instance of |PaymentRequestOptions| from |proto_|.
  std::unique_ptr<PaymentRequestOptions> CreateOptionsFromProto() const;

  bool IsInitialAutofillDataComplete(
      autofill::PersonalDataManager* personal_data_manager,
      const PaymentRequestOptions& payment_options) const;
  static bool IsCompleteContact(const autofill::AutofillProfile& profile,
                                const PaymentRequestOptions& payment_options);
  static bool IsCompleteAddress(const autofill::AutofillProfile& profile,
                                const PaymentRequestOptions& payment_options);
  static bool IsCompleteCreditCard(
      const autofill::CreditCard& credit_card,
      const PaymentRequestOptions& payment_options);

  bool presented_to_user_ = false;
  bool initially_prefilled = false;
  bool personal_data_changed_ = false;
  bool action_successful_ = false;
  ProcessActionCallback callback_;
  base::WeakPtrFactory<GetPaymentInformationAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetPaymentInformationAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_
