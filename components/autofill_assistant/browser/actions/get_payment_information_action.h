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
#include "base/optional.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/payment_request.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"

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
  struct LoginDetails {
    LoginDetails(bool choose_automatically_if_no_other_options,
                 const std::string& payload);
    LoginDetails(bool choose_automatically_if_no_other_options,
                 const std::string& payload,
                 const WebsiteLoginFetcher::Login& login);
    ~LoginDetails();
    bool choose_automatically_if_no_other_options;
    std::string payload;
    // Only for Chrome PWM login details.
    base::Optional<WebsiteLoginFetcher::Login> login;
  };

  void InternalProcessAction(ProcessActionCallback callback) override;
  void EndAction(const ClientStatus& status);

  void OnGetPaymentInformation(
      const GetPaymentInformationProto& get_payment_information,
      std::unique_ptr<PaymentInformation> payment_information);
  void OnAdditionalActionTriggered(int index);
  void OnTermsAndConditionsLinkClicked(int link);

  void OnGetLogins(const LoginDetailsProto::LoginOptionProto& login_option,
                   std::unique_ptr<PaymentRequestOptions> payment_options,
                   std::vector<WebsiteLoginFetcher::Login> logins);
  void ShowToUser(std::unique_ptr<PaymentRequestOptions> payment_options);

  // Creates a new instance of |PaymentRequestOptions| from |proto_|.
  std::unique_ptr<PaymentRequestOptions> CreateOptionsFromProto();

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

  bool shown_to_user_ = false;
  bool initially_prefilled = false;
  bool personal_data_changed_ = false;
  bool action_successful_ = false;
  ProcessActionCallback callback_;

  // Maps login choice identifiers to the corresponding login details.
  std::map<std::string, std::unique_ptr<LoginDetails>> login_details_map_;

  base::WeakPtrFactory<GetPaymentInformationAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetPaymentInformationAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_
