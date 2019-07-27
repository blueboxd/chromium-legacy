// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_x.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_origin_unittest.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::UnorderedPasswordFormElementsAre;
using password_manager::metrics_util::LinuxBackendMigrationStatus;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace {

class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    OnGetPasswordStoreResultsConstRef(results);
  }
};

class FailingBackend : public PasswordStoreX::NativeBackend {
 public:
  bool Init() override { return true; }

  PasswordStoreChangeList AddLogin(const PasswordForm& form) override {
    return PasswordStoreChangeList();
  }
  bool UpdateLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    return false;
  }
  bool RemoveLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    return false;
  }

  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override {
    return false;
  }

  bool DisableAutoSignInForOrigins(
      const base::Callback<bool(const GURL&)>& origin_filter,
      password_manager::PasswordStoreChangeList* changes) override {
    return false;
  }

  // Use this as a landmine to check whether results of failed Get*Logins calls
  // get ignored.
  static std::vector<std::unique_ptr<PasswordForm>> CreateTrashForms() {
    std::vector<std::unique_ptr<PasswordForm>> forms;
    PasswordForm trash;
    trash.username_element = base::ASCIIToUTF16("trash u. element");
    trash.username_value = base::ASCIIToUTF16("trash u. value");
    trash.password_element = base::ASCIIToUTF16("trash p. element");
    trash.password_value = base::ASCIIToUTF16("trash p. value");
    for (size_t i = 0; i < 3; ++i) {
      trash.origin = GURL(base::StringPrintf("http://trash%zu.com", i));
      forms.push_back(std::make_unique<PasswordForm>(trash));
    }
    return forms;
  }

  bool GetLogins(const PasswordStore::FormDigest& form,
                 std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }

  bool GetAutofillableLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }

  bool GetBlacklistLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }

  bool GetAllLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }

  scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() override {
    return nullptr;
  }
};

class MockBackend : public PasswordStoreX::NativeBackend {
 public:
  ~MockBackend() override {
    if (save_on_destruct_) {
      *save_on_destruct_ = std::move(all_forms_);
    }
  }

  bool Init() override { return true; }

  PasswordStoreChangeList AddLogin(const PasswordForm& form) override {
    all_forms_.push_back(form);
    PasswordStoreChange change(PasswordStoreChange::ADD, form);
    return PasswordStoreChangeList(1, change);
  }

  bool UpdateLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (ArePasswordFormUniqueKeysEqual(all_forms_[i], form)) {
        all_forms_[i] = form;
        changes->push_back(
            PasswordStoreChange(PasswordStoreChange::UPDATE, form));
      }
    }
    return true;
  }

  bool RemoveLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (ArePasswordFormUniqueKeysEqual(all_forms_[i], form)) {
        changes->push_back(
            PasswordStoreChange(PasswordStoreChange::REMOVE, form));
        erase(i--);
      }
    }
    return true;
  }

  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_created &&
          (delete_end.is_null() || all_forms_[i].date_created < delete_end))
        erase(i--);
    }
    return true;
  }

  bool DisableAutoSignInForOrigins(
      const base::Callback<bool(const GURL&)>& origin_filter,
      password_manager::PasswordStoreChangeList* changes) override {
    return true;
  }

  bool GetLogins(const PasswordStore::FormDigest& form,
                 std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].signon_realm == form.signon_realm)
        forms->push_back(std::make_unique<PasswordForm>(all_forms_[i]));
    return true;
  }

  bool GetAutofillableLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (!all_forms_[i].blacklisted_by_user)
        forms->push_back(std::make_unique<PasswordForm>(all_forms_[i]));
    return true;
  }

  bool GetBlacklistLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].blacklisted_by_user)
        forms->push_back(std::make_unique<PasswordForm>(all_forms_[i]));
    return true;
  }

  bool GetAllLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      forms->push_back(std::make_unique<PasswordForm>(all_forms_[i]));
    return true;
  }

  scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() override {
    return nullptr;
  }

  void SaveFormsOnDestruct(std::vector<PasswordForm>* forms) {
    save_on_destruct_ = forms;
  }

 private:
  void erase(size_t index) {
    if (index < all_forms_.size() - 1)
      all_forms_[index] = all_forms_.back();
    all_forms_.pop_back();
  }

  std::vector<PasswordForm>* save_on_destruct_ = nullptr;
  std::vector<PasswordForm> all_forms_;
};

class MockLoginDatabaseReturn {
 public:
  MOCK_METHOD1(OnLoginDatabaseQueryDone,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));
};

void LoginDatabaseQueryCallback(password_manager::LoginDatabase* login_db,
                                MockLoginDatabaseReturn* mock_return) {
  password_manager::PrimaryKeyToFormMap key_to_form_map;
  EXPECT_EQ(password_manager::FormRetrievalResult::kSuccess,
            login_db->GetAllLogins(&key_to_form_map));
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.reserve(key_to_form_map.size());
  for (auto& key_to_form : key_to_form_map)
    results.push_back(std::move(key_to_form.second));
  mock_return->OnLoginDatabaseQueryDone(results);
}

// Generate |count| expected logins, either auto-fillable or blacklisted.
void InitExpectedForms(bool autofillable,
                       size_t count,
                       std::vector<std::unique_ptr<PasswordForm>>* forms) {
  const char* domain = autofillable ? "example" : "blacklisted";
  for (size_t i = 0; i < count; ++i) {
    std::string realm = base::StringPrintf("http://%zu.%s.com", i, domain);
    std::string origin =
        base::StringPrintf("http://%zu.%s.com/origin", i, domain);
    std::string action =
        base::StringPrintf("http://%zu.%s.com/action", i, domain);
    password_manager::PasswordFormData data = {
        PasswordForm::Scheme::kHtml,
        realm.c_str(),
        origin.c_str(),
        action.c_str(),
        L"submit_element",
        L"username_element",
        L"password_element",
        autofillable ? L"username_value" : nullptr,
        autofillable ? L"password_value" : nullptr,
        autofillable,
        static_cast<double>(i + 1)};
    forms->push_back(FillPasswordFormWithData(data));
  }
}

PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

enum BackendType { NO_BACKEND, FAILING_BACKEND, WORKING_BACKEND };

std::unique_ptr<PasswordStoreX::NativeBackend> GetBackend(
    BackendType backend_type) {
  switch (backend_type) {
    case FAILING_BACKEND:
      return std::make_unique<FailingBackend>();
    case WORKING_BACKEND:
      return std::make_unique<MockBackend>();
    default:
      return std::unique_ptr<PasswordStoreX::NativeBackend>();
  }
}

class PasswordStoreXTestDelegate {
 public:
  PasswordStoreX* store() { return store_.get(); }

  void FinishAsyncProcessing();

 protected:
  explicit PasswordStoreXTestDelegate(BackendType backend_type);
  ~PasswordStoreXTestDelegate();

 private:
  void SetupTempDir();

  base::FilePath test_login_db_file_path() const;
  base::FilePath test_encrypted_login_db_file_path() const;

  base::test::ScopedTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  BackendType backend_type_;
  scoped_refptr<PasswordStoreX> store_;
  TestingPrefServiceSimple fake_pref_service;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreXTestDelegate);
};

PasswordStoreXTestDelegate::PasswordStoreXTestDelegate(BackendType backend_type)
    : backend_type_(backend_type) {
  SetupTempDir();
  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path());
  fake_pref_service.registry()->RegisterIntegerPref(
      password_manager::prefs::kMigrationToLoginDBStep,
      PasswordStoreX::NOT_ATTEMPTED);
  store_ = new PasswordStoreX(std::move(login_db), test_login_db_file_path(),
                              test_encrypted_login_db_file_path(),
                              GetBackend(backend_type_), &fake_pref_service);
  store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
}

PasswordStoreXTestDelegate::~PasswordStoreXTestDelegate() {
  store_->ShutdownOnUIThread();
}

void PasswordStoreXTestDelegate::FinishAsyncProcessing() {
  task_environment_.RunUntilIdle();
}

void PasswordStoreXTestDelegate::SetupTempDir() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

base::FilePath PasswordStoreXTestDelegate::test_login_db_file_path() const {
  return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
}

base::FilePath PasswordStoreXTestDelegate::test_encrypted_login_db_file_path()
    const {
  return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("encrypted_login_test"));
}

class PasswordStoreXNoBackendTestDelegate : public PasswordStoreXTestDelegate {
 public:
  PasswordStoreXNoBackendTestDelegate()
      : PasswordStoreXTestDelegate(NO_BACKEND) {}
};

class PasswordStoreXWorkingBackendTestDelegate
    : public PasswordStoreXTestDelegate {
 public:
  PasswordStoreXWorkingBackendTestDelegate()
      : PasswordStoreXTestDelegate(WORKING_BACKEND) {
    // Working backends are switched for LoginDatabase with encryption.
    OSCryptMocker::SetUp();
  }
  ~PasswordStoreXWorkingBackendTestDelegate() { OSCryptMocker::TearDown(); }
};

std::vector<std::unique_ptr<PasswordForm>> ReadLoginDB(
    const base::FilePath& path,
    bool encrypted) {
  password_manager::LoginDatabase login_db(path);
  if (!encrypted)
    login_db.disable_encryption();
  EXPECT_TRUE(login_db.Init());
  std::vector<std::unique_ptr<PasswordForm>> stored_forms;
  EXPECT_TRUE(login_db.GetAutofillableLogins(&stored_forms));
  return stored_forms;
}

}  // namespace

namespace password_manager {

INSTANTIATE_TYPED_TEST_SUITE_P(XNoBackend,
                               PasswordStoreOriginTest,
                               PasswordStoreXNoBackendTestDelegate);

INSTANTIATE_TYPED_TEST_SUITE_P(XWorkingBackend,
                               PasswordStoreOriginTest,
                               PasswordStoreXWorkingBackendTestDelegate);
}  // namespace password_manager

class PasswordStoreXTest : public testing::TestWithParam<BackendType> {
 protected:
  PasswordStoreXTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kMigrationToLoginDBStep,
        PasswordStoreX::NOT_ATTEMPTED);
    OSCryptMocker::SetUp();
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  base::FilePath test_encrypted_login_db_file_path() const {
    return temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("encrypted_login_test"));
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

 protected:
  TestingPrefServiceSimple fake_pref_service_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreXTest);
};

TEST_P(PasswordStoreXTest, Notifications) {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      new password_manager::LoginDatabase(test_login_db_file_path()));
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(std::move(login_db), test_login_db_file_path(),
                         test_encrypted_login_db_file_path(),
                         GetBackend(GetParam()), &fake_pref_service_));
  store->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

  password_manager::PasswordFormData form_data = {
      PasswordForm::Scheme::kHtml,
      "http://bar.example.com",
      "http://bar.example.com/origin",
      "http://bar.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"password_value",
      true,
      1};
  std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(form_data);

  password_manager::MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
      PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_add_changes)));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);

  WaitForPasswordStore();

  // Change the password.
  form->password_value = base::ASCIIToUTF16("a different password");

  const PasswordStoreChange expected_update_changes[] = {
      PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_update_changes)));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);

  WaitForPasswordStore();

  const PasswordStoreChange expected_delete_changes[] = {
      PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);

  WaitForPasswordStore();

  store->RemoveObserver(&observer);

  store->ShutdownOnUIThread();
}

TEST_P(PasswordStoreXTest, NativeMigration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kMigrateLinuxToLoginDB);

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  InitExpectedForms(true, 5, &expected_forms);
  InitExpectedForms(false, 5, &expected_forms);

  const base::FilePath login_db_file = test_login_db_file_path();
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      new password_manager::LoginDatabase(login_db_file));
  login_db->disable_encryption();
  ASSERT_TRUE(login_db->Init());

  // Get the initial size of the login DB file, before we populate it.
  // This will be used later to make sure it gets back to this size.
  base::File::Info db_file_start_info;
  ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_start_info));

  // Populate the login DB with logins that should be migrated.
  for (const auto& form : expected_forms) {
    EXPECT_EQ(AddChangeForForm(*form), login_db->AddLogin(*form));
  }

  // Get the new size of the login DB file. We expect it to be larger.
  base::File::Info db_file_full_info;
  ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_full_info));
  EXPECT_GT(db_file_full_info.size, db_file_start_info.size);

  // Initializing the PasswordStore shouldn't trigger a native migration (yet).
  login_db.reset(new password_manager::LoginDatabase(login_db_file));
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(std::move(login_db), test_login_db_file_path(),
                         test_encrypted_login_db_file_path(),
                         GetBackend(GetParam()), &fake_pref_service_));
  store->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

  MockPasswordStoreConsumer consumer;

  // All forms should have been migrated to the native backend.
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            UnorderedPasswordFormElementsAre(&expected_forms)));
  store->GetAllLogins(&consumer);
  WaitForPasswordStore();

  MockLoginDatabaseReturn ld_return;

  if (GetParam() == WORKING_BACKEND) {
    // No logins should be left in the login DB.
    EXPECT_CALL(ld_return, OnLoginDatabaseQueryDone(IsEmpty()));
  } else {
    // All logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(
                    UnorderedPasswordFormElementsAre(&expected_forms)));
  }

  LoginDatabaseQueryCallback(store->login_db(), &ld_return);
  WaitForPasswordStore();

  if (GetParam() == WORKING_BACKEND) {
    // If the migration succeeded, then not only should there be no logins left
    // in the login DB, but also the file should have been deleted and then
    // recreated. We approximate checking for this by checking that the file
    // size is equal to the size before we populated it, even though it was
    // larger after populating it.
    base::File::Info db_file_end_info;
    ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_end_info));
    EXPECT_EQ(db_file_start_info.size, db_file_end_info.size);
  }

  store->ShutdownOnUIThread();
}

TEST_P(PasswordStoreXTest, MigrationToEncryption) {
  IntegerPrefMember migration_step_pref_;
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            &fake_pref_service_);

  EXPECT_EQ(PasswordStoreX::NOT_ATTEMPTED, migration_step_pref_.GetValue());

  // Add existing credentials into the backend.
  std::vector<std::unique_ptr<PasswordForm>> old_credentials;
  InitExpectedForms(true, 6, &old_credentials);
  std::unique_ptr<PasswordStoreX::NativeBackend> backend =
      GetBackend(GetParam());
  std::vector<PasswordForm> native_backend_last_state;
  if (GetParam() == WORKING_BACKEND) {
    static_cast<MockBackend*>(backend.get())
        ->SaveFormsOnDestruct(&native_backend_last_state);
  }
  if (GetParam() != NO_BACKEND) {
    for (int i = 0; i < 3; i++)
      backend->AddLogin(*old_credentials[i]);
  }

  // Add existing credentials into the unencrypted loginDB.
  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path());
  login_db->disable_encryption();
  ASSERT_TRUE(login_db->Init());
  for (int i = 3; i < 6; i++)
    ignore_result(login_db->AddLogin(*old_credentials[i]));

  login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path());
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(std::move(login_db), test_login_db_file_path(),
                         test_encrypted_login_db_file_path(),
                         std::move(backend), &fake_pref_service_));
  store->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

  // Make modifications, so that we trigger the migration and so that we can
  // verify where the store serves from. The migration is triggered
  // opportunistically during access to the store.
  const auto new_form = password_manager::FillPasswordFormWithData(
      {PasswordForm::Scheme::kHtml, "https://www.fakebook.com",
       "https://www.fakebook.com/li", "https://www.fakebook.com/a",
       L"submit_element", L"username_element", L"password_element",
       L"username_value", L"password_value", true, 1.0});
  store->RemoveLogin(*old_credentials[0]);
  store->AddLogin(*new_form);

  MockPasswordStoreConsumer consumer;
  if (GetParam() == WORKING_BACKEND) {
    // The store has the native backend data, the initial unencrypted loginDB
    // data and modifications.
    EXPECT_CALL(
        consumer,
        OnGetPasswordStoreResultsConstRef(UnorderedElementsAre(
            Pointee(*new_form), Pointee(*old_credentials[1]),
            Pointee(*old_credentials[2]), Pointee(*old_credentials[3]),
            Pointee(*old_credentials[4]), Pointee(*old_credentials[5]))));
  } else {
    // The has the initial unencrypted loginDB data and modifications.
    EXPECT_CALL(
        consumer,
        OnGetPasswordStoreResultsConstRef(UnorderedElementsAre(
            Pointee(*new_form), Pointee(*old_credentials[3]),
            Pointee(*old_credentials[4]), Pointee(*old_credentials[5]))));
  }
  store->GetAutofillableLogins(&consumer);

  WaitForPasswordStore();
  store->ShutdownOnUIThread();
  store.reset();
  WaitForPasswordStore();

  // This will report that it was migrated on the next run.
  histogram_tester_.ExpectBucketCount(
      "PasswordManager.LinuxBackendMigration.Adoption",
      LinuxBackendMigrationStatus::kNotAttempted, 1);

  if (GetParam() == WORKING_BACKEND) {
    // Verify that the login database contains all the values, now encrypted.
    std::vector<std::unique_ptr<PasswordForm>> stored_forms =
        ReadLoginDB(test_login_db_file_path(), true);
    EXPECT_EQ(6u, stored_forms.size());
    EXPECT_THAT(
        stored_forms,
        UnorderedElementsAre(
            Pointee(*new_form), Pointee(*old_credentials[1]),
            Pointee(*old_credentials[2]), Pointee(*old_credentials[3]),
            Pointee(*old_credentials[4]), Pointee(*old_credentials[5])));
    EXPECT_TRUE(native_backend_last_state.empty());

    stored_forms = ReadLoginDB(test_encrypted_login_db_file_path(), false);
    EXPECT_TRUE(stored_forms.empty());
    EXPECT_EQ(PasswordStoreX::LOGIN_DB_REPLACED,
              migration_step_pref_.GetValue());

    histogram_tester_.ExpectBucketCount(
        "PasswordManager.LinuxBackendMigration.AttemptResult",
        LinuxBackendMigrationStatus::kLoginDBReplaced, 1);
  } else if (GetParam() == FAILING_BACKEND) {
    // No values should be written if we can't read the backend.
    auto stored_forms = ReadLoginDB(test_encrypted_login_db_file_path(), true);
    EXPECT_TRUE(stored_forms.empty());
    EXPECT_THAT(migration_step_pref_.GetValue(), PasswordStoreX::POSTPONED);

    histogram_tester_.ExpectBucketCount(
        "PasswordManager.LinuxBackendMigration.AttemptResult",
        LinuxBackendMigrationStatus::kPostponed, 1);
  } else {  // NO_BACKEND
    // No values should be moved without a working backend.
    auto stored_forms = ReadLoginDB(test_encrypted_login_db_file_path(), true);
    EXPECT_TRUE(stored_forms.empty());
    EXPECT_THAT(migration_step_pref_.GetValue(), PasswordStoreX::NOT_ATTEMPTED);
  }
}

// Once the migration is performed, don't port anything else into the new
// location.
TEST_P(PasswordStoreXTest, MigrationToEncryption_OnlyOnce) {
  if (GetParam() != WORKING_BACKEND)
    return;

  IntegerPrefMember migration_step_pref_;
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            &fake_pref_service_);
  // Signal that the migration has been completed.
  migration_step_pref_.SetValue(PasswordStoreX::LOGIN_DB_REPLACED);

  // We add new credentials into a backend. They should be completely ignored by
  // the store.
  std::vector<std::unique_ptr<PasswordForm>> old_credentials;
  InitExpectedForms(true, 6, &old_credentials);
  auto backend = GetBackend(GetParam());
  for (int i = 0; i < 3; i++)
    backend->AddLogin(*old_credentials[i]);

  // Add existing credentials into loginDB. They should be the only thing that's
  // available in the store.
  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path());
  ASSERT_TRUE(login_db->Init());
  for (int i = 3; i < 6; i++)
    ignore_result(login_db->AddLogin(*old_credentials[i]));
  login_db.reset();

  // Create the store.
  login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path());
  scoped_refptr<PasswordStoreX> store =
      new PasswordStoreX(std::move(login_db), test_login_db_file_path(),
                         test_encrypted_login_db_file_path(),
                         std::move(backend), &fake_pref_service_);
  store->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

  // Make modifications and check the contents.
  MockPasswordStoreConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(UnorderedElementsAre(
                  Pointee(*old_credentials[3]), Pointee(*old_credentials[4]),
                  Pointee(*old_credentials[5]))));
  store->GetAutofillableLogins(&consumer);
  store->RemoveLogin(*old_credentials[3]);
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(UnorderedElementsAre(
                  Pointee(*old_credentials[4]), Pointee(*old_credentials[5]))));
  store->GetAutofillableLogins(&consumer);

  WaitForPasswordStore();
  store->ShutdownOnUIThread();
  store.reset();
  WaitForPasswordStore();

  // The previous results were served from an encrypted login database.
  std::vector<std::unique_ptr<PasswordForm>> stored_forms =
      ReadLoginDB(test_login_db_file_path(), true);
  EXPECT_EQ(2u, stored_forms.size());
  EXPECT_EQ(PasswordStoreX::LOGIN_DB_REPLACED, migration_step_pref_.GetValue());
  EXPECT_THAT(stored_forms, UnorderedElementsAre(Pointee(*old_credentials[4]),
                                                 Pointee(*old_credentials[5])));

  histogram_tester_.ExpectBucketCount(
      "PasswordManager.LinuxBackendMigration.Adoption",
      LinuxBackendMigrationStatus::kLoginDBReplaced, 1);
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.LinuxBackendMigration.AttemptResult", 0);
}

TEST_P(PasswordStoreXTest, MigrationToEncryption_DropIllegalEntries) {
  if (GetParam() != WORKING_BACKEND)
    return;

  IntegerPrefMember migration_step_pref_;
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            &fake_pref_service_);

  EXPECT_EQ(PasswordStoreX::NOT_ATTEMPTED, migration_step_pref_.GetValue());

  // Add existing credentials into the backend.
  std::vector<std::unique_ptr<PasswordForm>> old_credentials;
  InitExpectedForms(true, 4, &old_credentials);
  // Create illegal entries.
  old_credentials[1]->origin = GURL();
  old_credentials[3]->signon_realm.clear();

  std::unique_ptr<PasswordStoreX::NativeBackend> backend =
      GetBackend(GetParam());
  std::vector<PasswordForm> native_backend_last_state;
  static_cast<MockBackend*>(backend.get())
      ->SaveFormsOnDestruct(&native_backend_last_state);
  for (int i = 0; i < 3; i++)
    backend->AddLogin(*old_credentials[i]);

  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path());
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(std::move(login_db), test_login_db_file_path(),
                         test_encrypted_login_db_file_path(),
                         std::move(backend), &fake_pref_service_));
  store->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

  MockPasswordStoreConsumer consumer;
  // The store has the native backend data, minus the illegal entries.
  // The call to GetAutofillableLogins() both triggers the opportunistic
  // migration and returns the data for the test.
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(UnorderedElementsAre(
                  Pointee(*old_credentials[0]), Pointee(*old_credentials[2]))));
  store->GetAutofillableLogins(&consumer);

  WaitForPasswordStore();
  store->ShutdownOnUIThread();
  store.reset();
  WaitForPasswordStore();

  // Verify that the login database contains all the values, now encrypted.
  std::vector<std::unique_ptr<PasswordForm>> stored_forms =
      ReadLoginDB(test_login_db_file_path(), true);
  EXPECT_EQ(2u, stored_forms.size());
  EXPECT_THAT(stored_forms, UnorderedElementsAre(Pointee(*old_credentials[0]),
                                                 Pointee(*old_credentials[2])));
  EXPECT_EQ(PasswordStoreX::LOGIN_DB_REPLACED, migration_step_pref_.GetValue());

  histogram_tester_.ExpectBucketCount(
      "PasswordManager.LinuxBackendMigration.AttemptResult",
      LinuxBackendMigrationStatus::kLoginDBReplaced, 1);
}

INSTANTIATE_TEST_SUITE_P(NoBackend,
                         PasswordStoreXTest,
                         testing::Values(NO_BACKEND));
INSTANTIATE_TEST_SUITE_P(FailingBackend,
                         PasswordStoreXTest,
                         testing::Values(FAILING_BACKEND));
INSTANTIATE_TEST_SUITE_P(WorkingBackend,
                         PasswordStoreXTest,
                         testing::Values(WORKING_BACKEND));
