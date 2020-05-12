// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_GENERIC_UI_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_GENERIC_UI_ACTION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/website_login_manager.h"

namespace autofill_assistant {
class UserModel;

// Action to show generic UI in the sheet.
class ShowGenericUiAction : public Action,
                            public autofill::PersonalDataManagerObserver {
 public:
  explicit ShowGenericUiAction(ActionDelegate* delegate,
                               const ActionProto& proto);
  ~ShowGenericUiAction() override;

  ShowGenericUiAction(const ShowGenericUiAction&) = delete;
  ShowGenericUiAction& operator=(const ShowGenericUiAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(bool view_inflation_successful,
                 ProcessedActionStatusProto status,
                 const UserModel* user_model);

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

  void OnGetLogins(const ShowGenericUiProto::RequestLoginOptions& proto,
                   std::vector<WebsiteLoginManager::Login> logins);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<ShowGenericUiAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_GENERIC_UI_ACTION_H_
