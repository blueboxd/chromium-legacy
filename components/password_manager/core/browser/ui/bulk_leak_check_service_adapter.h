// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_BULK_LEAK_CHECK_SERVICE_ADAPTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_BULK_LEAK_CHECK_SERVICE_ADAPTER_H_

#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

// Key used to store an instance of BulkLeakCheckData in the user data map of a
// LeakCheckCredential.
extern const char kBulkLeakCheckDataKey[];

// This struct bundles forms that correspond to the same LeakCheckCredential.
// That is, all of the forms in |leaked_forms| correspond to the same pair of
// canonicalized username and password.
struct BulkLeakCheckData : LeakCheckCredential::Data {
  explicit BulkLeakCheckData(const autofill::PasswordForm& leaked_form);
  explicit BulkLeakCheckData(std::vector<autofill::PasswordForm> leaked_forms);
  ~BulkLeakCheckData() override;

  std::vector<autofill::PasswordForm> leaked_forms;
};

// This class serves as an apdater for the BulkLeakCheckService and exposes an
// API that is intended to be consumed from the settings page.
class BulkLeakCheckServiceAdapter : public SavedPasswordsPresenter::Observer {
 public:
  BulkLeakCheckServiceAdapter(SavedPasswordsPresenter* presenter,
                              BulkLeakCheckService* service);
  ~BulkLeakCheckServiceAdapter() override;

  // Instructs the adapter to start a check. This is a no-op in case a check is
  // already running. Otherwise, this will obtain the list of saved passwords
  // from |presenter_|, perform de-duplication of username and password pairs
  // and then feed it to the |service_| for checking.
  // Returns whether new check was started.
  bool StartBulkLeakCheck();

  // This asks |service_| to stop an ongoing check.
  void StopBulkLeakCheck();

  // Obtains the state of the bulk leak check.
  BulkLeakCheckService::State GetBulkLeakCheckState() const;

  // Gets the list of pending checks.
  size_t GetPendingChecksCount() const;

 private:
  // SavedPasswordsPresenter::Observer:
  void OnEdited(const autofill::PasswordForm& form) override;

  // Weak handles to a presenter and service, respectively. These must be not
  // null and must outlive the adapter.
  SavedPasswordsPresenter* presenter_ = nullptr;
  BulkLeakCheckService* service_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_BULK_LEAK_CHECK_SERVICE_ADAPTER_H_
