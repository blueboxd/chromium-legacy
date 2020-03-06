// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/time_format.h"

namespace extensions {

namespace {

static const size_t kNumMocks = 3;
static const int kNumCharactersInPassword = 10;
static const char kPlaintextPassword[] = "plaintext";

api::passwords_private::PasswordUiEntry CreateEntry(int id) {
  api::passwords_private::PasswordUiEntry entry;
  entry.urls.shown = "test" + std::to_string(id) + ".com";
  entry.urls.origin = "http://" + entry.urls.shown + "/login";
  entry.urls.link = entry.urls.origin;
  entry.username = "testName" + std::to_string(id);
  entry.num_characters_in_password = kNumCharactersInPassword;
  entry.id = id;
  return entry;
}

api::passwords_private::ExceptionEntry CreateException(int id) {
  api::passwords_private::ExceptionEntry exception;
  exception.urls.shown = "exception" + std::to_string(id) + ".com";
  exception.urls.origin = "http://" + exception.urls.shown + "/login";
  exception.urls.link = exception.urls.origin;
  exception.id = id;
  return exception;
}

// A test PasswordsPrivateDelegate implementation which uses mock data.
// TestDelegate starts out with kNumMocks mocks of each type (saved password
// and password exception) and removes one mock each time RemoveSavedPassword()
// or RemovePasswordException() is called.
class TestDelegate : public PasswordsPrivateDelegate {
 public:
  TestDelegate() : profile_(nullptr) {
    // Create mock data.
    for (size_t i = 0; i < kNumMocks; i++) {
      current_entries_.push_back(CreateEntry(i));
      current_exceptions_.push_back(CreateException(i));
    }
  }
  ~TestDelegate() override {}

  void GetSavedPasswordsList(UiEntriesCallback callback) override {
    std::move(callback).Run(current_entries_);
  }

  void GetPasswordExceptionsList(ExceptionEntriesCallback callback) override {
    std::move(callback).Run(current_exceptions_);
  }

  void ChangeSavedPassword(int id,
                           base::string16 username,
                           base::Optional<base::string16> password) override {
    if (size_t{id} >= current_entries_.size())
      return;

    // PasswordUiEntry does not contain a password. Thus we are only updating
    // the username and the length of the password.
    current_entries_[id].username = base::UTF16ToUTF8(username);
    if (password)
      current_entries_[id].num_characters_in_password = password->size();
    SendSavedPasswordsList();
  }

  void RemoveSavedPassword(int id) override {
    if (current_entries_.empty())
      return;

    // Since this is just mock data, remove the first entry regardless of
    // the data contained.
    last_deleted_entry_ = std::move(current_entries_.front());
    current_entries_.erase(current_entries_.begin());
    SendSavedPasswordsList();
  }

  void RemovePasswordException(int id) override {
    // Since this is just mock data, remove the first entry regardless of
    // the data contained.
    last_deleted_exception_ = std::move(current_exceptions_.front());
    current_exceptions_.erase(current_exceptions_.begin());
    SendPasswordExceptionsList();
  }

  // Simplified version of undo logic, only use for testing.
  void UndoRemoveSavedPasswordOrException() override {
    if (last_deleted_entry_) {
      current_entries_.insert(current_entries_.begin(),
                              std::move(*last_deleted_entry_));
      last_deleted_entry_ = base::nullopt;
      SendSavedPasswordsList();
    } else if (last_deleted_exception_) {
      current_exceptions_.insert(current_exceptions_.begin(),
                                 std::move(*last_deleted_exception_));
      last_deleted_exception_ = base::nullopt;
      SendPasswordExceptionsList();
    }
  }

  void RequestPlaintextPassword(int id,
                                api::passwords_private::PlaintextReason reason,
                                PlaintextPasswordCallback callback,
                                content::WebContents* web_contents) override {
    // Return a mocked password value.
    std::move(callback).Run(plaintext_password_);
  }

  void SetProfile(Profile* profile) { profile_ = profile; }

  void ImportPasswords(content::WebContents* web_contents) override {
    // The testing of password importing itself should be handled via
    // |PasswordManagerPorter|.
    importPasswordsTriggered = true;
  }

  void ExportPasswords(base::OnceCallback<void(const std::string&)> callback,
                       content::WebContents* web_contents) override {
    // The testing of password exporting itself should be handled via
    // |PasswordManagerPorter|.
    exportPasswordsTriggered = true;
    std::move(callback).Run(std::string());
  }

  void CancelExportPasswords() override {
    cancelExportPasswordsTriggered = true;
  }

  api::passwords_private::ExportProgressStatus GetExportProgressStatus()
      override {
    // The testing of password exporting itself should be handled via
    // |PasswordManagerPorter|.
    return api::passwords_private::ExportProgressStatus::
        EXPORT_PROGRESS_STATUS_IN_PROGRESS;
  }

  bool IsOptedInForAccountStorage() override {
    return is_opted_in_for_account_storage_;
  }

  api::passwords_private::CompromisedCredentialsInfo
  GetCompromisedCredentialsInfo() override {
    using ui::TimeFormat;
    api::passwords_private::CompromisedCredential credential;
    credential.username = "alice";
    credential.formatted_origin = "example.com";
    credential.change_password_url =
        std::make_unique<std::string>("https://example.com/change-password");
    credential.compromise_type = api::passwords_private::COMPROMISE_TYPE_LEAKED;
    credential.elapsed_time_since_compromise = base::UTF16ToUTF8(
        TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG,
                           base::TimeDelta::FromDays(3)));
    api::passwords_private::CompromisedCredentialsInfo info;
    info.compromised_credentials.push_back(std::move(credential));
    info.elapsed_time_since_last_check =
        std::make_unique<std::string>(base::UTF16ToUTF8(TimeFormat::Simple(
            TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT,
            base::TimeDelta::FromMinutes(5))));
    return info;
  }

  void GetPlaintextCompromisedPassword(
      api::passwords_private::CompromisedCredential credential,
      api::passwords_private::PlaintextReason reason,
      content::WebContents* web_contents,
      PlaintextCompromisedPasswordCallback callback) override {
    // Return a mocked password value.
    if (!plaintext_password()) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    credential.password =
        std::make_unique<std::string>(base::UTF16ToUTF8(*plaintext_password()));
    std::move(callback).Run(std::move(credential));
  }

  // Fake implementation of ChangeCompromisedCredential. This succeeds if the
  // delegate knows of a compromised credential with the same id.
  bool ChangeCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential,
      base::StringPiece new_password) override {
    return std::any_of(compromised_credentials_.begin(),
                       compromised_credentials_.end(),
                       [&credential](const auto& compromised_credential) {
                         return compromised_credential.id == credential.id;
                       });
  }

  // Fake implementation of RemoveCompromisedCredential. This succeeds if the
  // delegate knows of a compromised credential with the same id.
  bool RemoveCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential)
      override {
    return base::EraseIf(compromised_credentials_,
                         [&credential](const auto& compromised_credential) {
                           return compromised_credential.id == credential.id;
                         }) != 0;
  }

  bool StartPasswordCheck() override {
    start_password_check_triggered_ = true;
    return start_password_check_return_success_;
  }

  void StopPasswordCheck() override { stop_password_check_triggered_ = true; }

  void SetOptedInForAccountStorage(bool opted_in) {
    is_opted_in_for_account_storage_ = opted_in;
  }

  void AddCompromisedCredential(int id) {
    api::passwords_private::CompromisedCredential cred;
    cred.id = id;
    compromised_credentials_.push_back(std::move(cred));
  }

  base::Optional<base::string16>& plaintext_password() {
    return plaintext_password_;
  }

  // Flags for detecting whether import/export operations have been invoked.
  bool importPasswordsTriggered = false;
  bool exportPasswordsTriggered = false;
  bool cancelExportPasswordsTriggered = false;

  // Flags for detecting whether password check operations have been invoked.
  bool start_password_check_triggered_ = false;
  bool stop_password_check_triggered_ = false;
  bool start_password_check_return_success_ = true;

 private:
  void SendSavedPasswordsList() {
    PasswordsPrivateEventRouter* router =
        PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
    if (router)
      router->OnSavedPasswordsListChanged(current_entries_);
  }

  void SendPasswordExceptionsList() {
    PasswordsPrivateEventRouter* router =
        PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
    if (router)
      router->OnPasswordExceptionsListChanged(current_exceptions_);
  }

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
      base::ASCIIToUTF16(kPlaintextPassword);

  // List of compromised credentials.
  std::vector<api::passwords_private::CompromisedCredential>
      compromised_credentials_;
  Profile* profile_;

  bool is_opted_in_for_account_storage_ = false;
};

class PasswordsPrivateApiTest : public ExtensionApiTest {
 public:
  PasswordsPrivateApiTest() {
    if (!s_test_delegate_) {
      s_test_delegate_ = new TestDelegate();
    }
  }
  ~PasswordsPrivateApiTest() override {}

  static std::unique_ptr<KeyedService> GetPasswordsPrivateDelegate(
      content::BrowserContext* profile) {
    CHECK(s_test_delegate_);
    return base::WrapUnique(s_test_delegate_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUp() override { ExtensionApiTest::SetUp(); }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    PasswordsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &PasswordsPrivateApiTest::GetPasswordsPrivateDelegate));
    s_test_delegate_->SetProfile(profile());
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunPasswordsSubtest(const std::string& subtest) {
    return RunExtensionSubtest("passwords_private", "main.html?" + subtest,
                               kFlagNone, kFlagLoadAsComponent);
  }

  bool importPasswordsWasTriggered() {
    return s_test_delegate_->importPasswordsTriggered;
  }

  bool exportPasswordsWasTriggered() {
    return s_test_delegate_->exportPasswordsTriggered;
  }

  bool cancelExportPasswordsWasTriggered() {
    return s_test_delegate_->cancelExportPasswordsTriggered;
  }

  bool start_password_check_triggered() {
    return s_test_delegate_->start_password_check_triggered_;
  }

  bool stop_password_check_triggered() {
    return s_test_delegate_->stop_password_check_triggered_;
  }

  void set_start_password_check_result(bool result) {
    s_test_delegate_->start_password_check_return_success_ = result;
  }

  void SetOptedInForAccountStorage(bool opted_in) {
    s_test_delegate_->SetOptedInForAccountStorage(opted_in);
  }

  base::Optional<base::string16>& plaintext_password() {
    return s_test_delegate_->plaintext_password();
  }

  void AddCompromisedCredential(int id) {
    s_test_delegate_->AddCompromisedCredential(id);
  }

 private:
  static TestDelegate* s_test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateApiTest);
};

// static
TestDelegate* PasswordsPrivateApiTest::s_test_delegate_ = nullptr;

}  // namespace

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ChangeSavedPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("changeSavedPassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveAndUndoRemoveSavedPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("removeAndUndoRemoveSavedPassword"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveAndUndoRemovePasswordException) {
  EXPECT_TRUE(RunPasswordsSubtest("removeAndUndoRemovePasswordException"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestPlaintextPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("requestPlaintextPassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestPlaintextPasswordFails) {
  plaintext_password().reset();
  EXPECT_TRUE(RunPasswordsSubtest("requestPlaintextPasswordFails")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetSavedPasswordList) {
  EXPECT_TRUE(RunPasswordsSubtest("getSavedPasswordList")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetPasswordExceptionList) {
  EXPECT_TRUE(RunPasswordsSubtest("getPasswordExceptionList")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ImportPasswords) {
  EXPECT_FALSE(importPasswordsWasTriggered());
  EXPECT_TRUE(RunPasswordsSubtest("importPasswords")) << message_;
  EXPECT_TRUE(importPasswordsWasTriggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ExportPasswords) {
  EXPECT_FALSE(exportPasswordsWasTriggered());
  EXPECT_TRUE(RunPasswordsSubtest("exportPasswords")) << message_;
  EXPECT_TRUE(exportPasswordsWasTriggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, CancelExportPasswords) {
  EXPECT_FALSE(cancelExportPasswordsWasTriggered());
  EXPECT_TRUE(RunPasswordsSubtest("cancelExportPasswords")) << message_;
  EXPECT_TRUE(cancelExportPasswordsWasTriggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestExportProgressStatus) {
  EXPECT_TRUE(RunPasswordsSubtest("requestExportProgressStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, IsNotOptedInForAccountStorage) {
  EXPECT_TRUE(RunPasswordsSubtest("isNotOptedInForAccountStorage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, IsOptedInForAccountStorage) {
  SetOptedInForAccountStorage(true);
  EXPECT_TRUE(RunPasswordsSubtest("isOptedInForAccountStorage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetCompromisedCredentialsInfo) {
  EXPECT_TRUE(RunPasswordsSubtest("getCompromisedCredentialsInfo")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       GetPlaintextCompromisedPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("getPlaintextCompromisedPassword"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       GetPlaintextCompromisedPasswordFails) {
  plaintext_password().reset();
  EXPECT_TRUE(RunPasswordsSubtest("getPlaintextCompromisedPasswordFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeCompromisedCredentialFails) {
  EXPECT_TRUE(RunPasswordsSubtest("changeCompromisedCredentialFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeCompromisedCredentialSucceeds) {
  AddCompromisedCredential(0);
  EXPECT_TRUE(RunPasswordsSubtest("changeCompromisedCredentialSucceeds"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveCompromisedCredentialFails) {
  EXPECT_TRUE(RunPasswordsSubtest("removeCompromisedCredentialFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveCompromisedCredentialSucceeds) {
  AddCompromisedCredential(0);
  EXPECT_TRUE(RunPasswordsSubtest("removeCompromisedCredentialSucceeds"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, StartPasswordCheck) {
  set_start_password_check_result(true);
  EXPECT_FALSE(start_password_check_triggered());
  EXPECT_TRUE(RunPasswordsSubtest("startPasswordCheck")) << message_;
  EXPECT_TRUE(start_password_check_triggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, StartPasswordCheckFailed) {
  set_start_password_check_result(false);
  EXPECT_FALSE(start_password_check_triggered());
  EXPECT_TRUE(RunPasswordsSubtest("startPasswordCheckFailed")) << message_;
  EXPECT_TRUE(start_password_check_triggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, StopPasswordCheck) {
  EXPECT_FALSE(stop_password_check_triggered());
  EXPECT_TRUE(RunPasswordsSubtest("stopPasswordCheck")) << message_;
  EXPECT_TRUE(stop_password_check_triggered());
}

}  // namespace extensions
