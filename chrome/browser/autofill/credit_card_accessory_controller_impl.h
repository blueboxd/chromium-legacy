// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_

#include "chrome/browser/autofill/credit_card_accessory_controller.h"

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/web_contents_user_data.h"

class ManualFillingController;

namespace autofill {

// Use either CreditCardAccessoryController::GetOrCreate or
// CreditCardAccessoryController::GetIfExisting to obtain instances of this
// class.
class CreditCardAccessoryControllerImpl
    : public CreditCardAccessoryController,
      public content::WebContentsUserData<CreditCardAccessoryControllerImpl> {
 public:
  ~CreditCardAccessoryControllerImpl() override;

  // AccessoryController:
  void OnFillingTriggered(const UserInfo::Field& selection) override;
  void OnOptionSelected(AccessoryAction selected_action) override;

  // CreditCardAccessoryController:
  void RefreshSuggestions() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      autofill::PersonalDataManager* personal_data_manager);

 private:
  friend class content::WebContentsUserData<CreditCardAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit CreditCardAccessoryControllerImpl(content::WebContents* contents);

  // Used by CreateForWebContentsForTesting:
  CreditCardAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      PersonalDataManager* personal_data_manager);

  void FetchSuggestionsFromPersonalDataManager();
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // Pointers to cards owned by PersonalDataManager.
  std::vector<CreditCard*> cards_cache_;
  content::WebContents* web_contents_;
  base::WeakPtr<ManualFillingController> mf_controller_;
  PersonalDataManager* const personal_data_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_
