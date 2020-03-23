// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {
// A test PasswordsPrivateDelegate implementation which uses mock data.
// TestDelegate starts out with kNumMocks mocks of each type (saved password
// and password exception) and removes one mock each time RemoveSavedPassword()
// or RemovePasswordException() is called.
class TestPasswordsPrivateDelegate : public PasswordsPrivateDelegate {
 public:
  TestPasswordsPrivateDelegate();
  ~TestPasswordsPrivateDelegate() override;

  // PasswordsPrivateDelegate implementation.
  void GetSavedPasswordsList(UiEntriesCallback callback) override;
  void GetPasswordExceptionsList(ExceptionEntriesCallback callback) override;
  void ChangeSavedPassword(int id,
                           base::string16 username,
                           base::Optional<base::string16> password) override;
  void RemoveSavedPassword(int id) override;
  void RemovePasswordException(int id) override;
  // Simplified version of undo logic, only use for testing.
  void UndoRemoveSavedPasswordOrException() override;
  void RequestPlaintextPassword(int id,
                                api::passwords_private::PlaintextReason reason,
                                PlaintextPasswordCallback callback,
                                content::WebContents* web_contents) override;
  void ImportPasswords(content::WebContents* web_contents) override;
  void ExportPasswords(base::OnceCallback<void(const std::string&)> callback,
                       content::WebContents* web_contents) override;
  void CancelExportPasswords() override;
  api::passwords_private::ExportProgressStatus GetExportProgressStatus()
      override;
  bool IsOptedInForAccountStorage() override;
  void SetAccountStorageOptIn(bool opt_in,
                              content::WebContents* web_contents) override;
  std::vector<api::passwords_private::CompromisedCredential>
  GetCompromisedCredentials() override;
  void GetPlaintextCompromisedPassword(
      api::passwords_private::CompromisedCredential credential,
      api::passwords_private::PlaintextReason reason,
      content::WebContents* web_contents,
      PlaintextCompromisedPasswordCallback callback) override;
  // Fake implementation of ChangeCompromisedCredential. This succeeds if the
  // delegate knows of a compromised credential with the same id.
  bool ChangeCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential,
      base::StringPiece new_password) override;
  // Fake implementation of RemoveCompromisedCredential. This succeeds if the
  // delegate knows of a compromised credential with the same id.
  bool RemoveCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential) override;
  void StartPasswordCheck(StartPasswordCheckCallback callback) override;
  void StopPasswordCheck() override;
  api::passwords_private::PasswordCheckStatus GetPasswordCheckStatus() override;

  void SetProfile(Profile* profile);
  void SetOptedInForAccountStorage(bool opted_in);
  void AddCompromisedCredential(int id);

  void ClearSavedPasswordsList() { current_entries_.clear(); }
  void ResetPlaintextPassword() { plaintext_password_.reset(); }
  bool ImportPasswordsTriggered() const { return import_passwords_triggered_; }
  bool ExportPasswordsTriggered() const { return export_passwords_triggered_; }
  bool CancelExportPasswordsTriggered() const {
    return cancel_export_passwords_triggered_;
  }
  bool StartPasswordCheckTriggered() const {
    return start_password_check_triggered_;
  }
  bool StopPasswordCheckTriggered() const {
    return stop_password_check_triggered_;
  }
  void SetStartPasswordCheckState(
      password_manager::BulkLeakCheckService::State state) {
    start_password_check_state_ = state;
  }

 private:
  void SendSavedPasswordsList();
  void SendPasswordExceptionsList();

  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  std::vector<api::passwords_private::PasswordUiEntry> current_entries_;
  std::vector<api::passwords_private::ExceptionEntry> current_exceptions_;
  // Simplified version of a undo manager that only allows undoing and redoing
  // the very last deletion.
  base::Optional<api::passwords_private::PasswordUiEntry> last_deleted_entry_;
  base::Optional<api::passwords_private::ExceptionEntry>
      last_deleted_exception_;
  base::Optional<base::string16> plaintext_password_ =
      base::ASCIIToUTF16("plaintext");

  // List of compromised credentials.
  std::vector<api::passwords_private::CompromisedCredential>
      compromised_credentials_;
  Profile* profile_ = nullptr;

  bool is_opted_in_for_account_storage_ = false;

  // Flags for detecting whether import/export operations have been invoked.
  bool import_passwords_triggered_ = false;
  bool export_passwords_triggered_ = false;
  bool cancel_export_passwords_triggered_ = false;

  // Flags for detecting whether password check operations have been invoked.
  bool start_password_check_triggered_ = false;
  bool stop_password_check_triggered_ = false;
  password_manager::BulkLeakCheckService::State start_password_check_state_ =
      password_manager::BulkLeakCheckService::State::kRunning;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_
