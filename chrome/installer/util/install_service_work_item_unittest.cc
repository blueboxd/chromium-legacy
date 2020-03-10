// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_service_work_item_impl.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

constexpr base::char16 kServiceName[] = L"InstallServiceWorkItemService";
constexpr base::char16 kServiceDisplayName[] = L"InstallServiceWorkItemService";
constexpr base::FilePath::CharType kServiceProgramPath[] =
    FILE_PATH_LITERAL("c:\\windows\\system32\\cmd.exe");

}  // namespace

class InstallServiceWorkItemTest : public ::testing::Test {
 protected:
  static InstallServiceWorkItemImpl* GetImpl(InstallServiceWorkItem* item) {
    DCHECK(item);
    return item->impl_.get();
  }
  static bool IsServiceCorrectlyConfigured(InstallServiceWorkItem* item) {
    DCHECK(item);
    InstallServiceWorkItemImpl::ServiceConfig config;
    if (!GetImpl(item)->GetServiceConfig(&config))
      return false;

    return GetImpl(item)->IsServiceCorrectlyConfigured(config);
  }

  void SetUp() override {
    DWORD disposition = 0;
    ASSERT_EQ(
        base::win::RegKey().CreateWithDisposition(
            HKEY_LOCAL_MACHINE, install_static::GetClientStateKeyPath().c_str(),
            &disposition, KEY_READ | KEY_WOW64_32KEY),
        ERROR_SUCCESS);
    preexisting_clientstate_key_ = (disposition == REG_OPENED_EXISTING_KEY);
  }

  void TearDown() override {
    // Delete the ClientState key created by this test if it is empty. While it
    // would be ideal to only delete if !preexisting_clientstate_key_, older
    // variants of this test failed to delete their key during TearDown.
    auto result =
        base::win::RegKey(HKEY_LOCAL_MACHINE, L"", KEY_READ | KEY_WOW64_32KEY)
            .DeleteEmptyKey(install_static::GetClientStateKeyPath().c_str());
    // Deletion should have succeeded if the key didn't exist to start with. If
    // the key existed before the test ran, the delete may have succeeded
    // (because the key was empty to start with) or may have failed because the
    // key actually has data that should not be removed.
    if (!preexisting_clientstate_key_)
      EXPECT_EQ(result, ERROR_SUCCESS);
    else if (result != ERROR_SUCCESS)
      EXPECT_EQ(result, ERROR_DIR_NOT_EMPTY);
  }

  // Set up InstallDetails for a system-level install.
  const install_static::ScopedInstallDetails install_details_{true};
  bool preexisting_clientstate_key_ = false;
};

TEST_F(InstallServiceWorkItemTest, Do_MultiSzToVector) {
  constexpr base::char16 kZeroMultiSz[] = L"";
  std::vector<base::char16> vec =
      InstallServiceWorkItemImpl::MultiSzToVector(kZeroMultiSz);
  EXPECT_TRUE(!memcmp(vec.data(), &kZeroMultiSz, sizeof(kZeroMultiSz)));
  EXPECT_EQ(vec.size(), base::size(kZeroMultiSz));

  vec = InstallServiceWorkItemImpl::MultiSzToVector(nullptr);
  EXPECT_TRUE(vec.empty());

  constexpr base::char16 kRpcMultiSz[] = L"RPCSS\0";
  vec = InstallServiceWorkItemImpl::MultiSzToVector(kRpcMultiSz);
  EXPECT_TRUE(!memcmp(vec.data(), &kRpcMultiSz, sizeof(kRpcMultiSz)));
  EXPECT_EQ(vec.size(), base::size(kRpcMultiSz));

  constexpr base::char16 kMultiSz[] = L"RPCSS\0LSASS\0";
  vec = InstallServiceWorkItemImpl::MultiSzToVector(kMultiSz);
  EXPECT_TRUE(!memcmp(vec.data(), &kMultiSz, sizeof(kMultiSz)));
  EXPECT_EQ(vec.size(), base::size(kMultiSz));
}

TEST_F(InstallServiceWorkItemTest, Do_FreshInstall) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)));

  ASSERT_TRUE(item->Do());
  EXPECT_TRUE(GetImpl(item.get())->OpenService());
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  item->Rollback();
  EXPECT_FALSE(GetImpl(item.get())->OpenService());
}

TEST_F(InstallServiceWorkItemTest, Do_FreshInstallThenDeleteService) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)));

  ASSERT_TRUE(item->Do());
  EXPECT_TRUE(GetImpl(item.get())->OpenService());
  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  EXPECT_TRUE(InstallServiceWorkItem::DeleteService(kServiceName));
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeNoChanges) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)));
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  // Same command line:
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)));
  EXPECT_TRUE(item_upgrade->Do());

  item_upgrade->Rollback();
  EXPECT_TRUE(GetImpl(item_upgrade.get())->OpenService());

  EXPECT_TRUE(GetImpl(item_upgrade.get())->DeleteCurrentService());
}

TEST_F(InstallServiceWorkItemTest, Do_UpgradeChangedCmdLine) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)));
  ASSERT_TRUE(item->Do());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));

  // New command line.
  auto item_upgrade = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine::FromString(L"NewCmd.exe arg1 arg2"));
  EXPECT_TRUE(item_upgrade->Do());

  item_upgrade->Rollback();
  EXPECT_TRUE(GetImpl(item_upgrade.get())->OpenService());

  EXPECT_TRUE(IsServiceCorrectlyConfigured(item.get()));
  EXPECT_FALSE(IsServiceCorrectlyConfigured(item_upgrade.get()));

  EXPECT_TRUE(GetImpl(item_upgrade.get())->DeleteCurrentService());
}

TEST_F(InstallServiceWorkItemTest, Do_ServiceName) {
  auto item = std::make_unique<InstallServiceWorkItem>(
      kServiceName, kServiceDisplayName,
      base::CommandLine(base::FilePath(kServiceProgramPath)));

  EXPECT_STREQ(kServiceName,
               GetImpl(item.get())->GetCurrentServiceName().c_str());
  EXPECT_STREQ(
      base::StringPrintf(L"%ls (%ls)", kServiceDisplayName,
                         GetImpl(item.get())->GetCurrentServiceName().c_str())
          .c_str(),
      GetImpl(item.get())->GetCurrentServiceDisplayName().c_str());

  EXPECT_TRUE(GetImpl(item.get())->CreateAndSetServiceName());
  EXPECT_STRNE(kServiceName,
               GetImpl(item.get())->GetCurrentServiceName().c_str());
  EXPECT_EQ(0UL,
            GetImpl(item.get())->GetCurrentServiceName().find(kServiceName));
  EXPECT_STREQ(
      base::StringPrintf(L"%ls (%ls)", kServiceDisplayName,
                         GetImpl(item.get())->GetCurrentServiceName().c_str())
          .c_str(),
      GetImpl(item.get())->GetCurrentServiceDisplayName().c_str());

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE,
                     install_static::GetClientStateKeyPath().c_str(),
                     KEY_WRITE | KEY_WOW64_32KEY));
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kServiceName));
}

}  // namespace installer
