// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/win/sandbox_win.h"

#include <algorithm>
#include <vector>

#include <windows.h>

#include <sddl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/win/src/app_container_profile_base.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sid.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

constexpr wchar_t kBaseSecurityDescriptor[] = L"D:(A;;GA;;;WD)";
constexpr char kAppContainerId[] = "SandboxWinTest";
constexpr wchar_t kPackageSid[] =
    L"S-1-15-2-2739114418-4250112400-4176314265-1208402406-1880724913-"
    L"3756377648-2708578895";
constexpr wchar_t kChromeInstallFiles[] = L"chromeInstallFiles";
constexpr wchar_t kLpacChromeInstallFiles[] = L"lpacChromeInstallFiles";
constexpr wchar_t kRegistryRead[] = L"registryRead";
constexpr wchar_t klpacPnpNotifications[] = L"lpacPnpNotifications";

class TestTargetPolicy : public sandbox::TargetPolicy {
 public:
  void AddRef() override {}
  void Release() override {}
  sandbox::ResultCode SetTokenLevel(sandbox::TokenLevel initial,
                                    sandbox::TokenLevel lockdown) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::TokenLevel GetInitialTokenLevel() const override {
    return sandbox::TokenLevel{};
  }
  sandbox::TokenLevel GetLockdownTokenLevel() const override {
    return sandbox::TokenLevel{};
  }
  sandbox::ResultCode SetJobLevel(sandbox::JobLevel job_level,
                                  uint32_t ui_exceptions) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::JobLevel GetJobLevel() const override { return sandbox::JobLevel{}; }
  sandbox::ResultCode SetJobMemoryLimit(size_t memory_limit) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetAlternateDesktop(bool alternate_winstation) override {
    return sandbox::SBOX_ALL_OK;
  }
  base::string16 GetAlternateDesktop() const override {
    return base::string16();
  }
  sandbox::ResultCode CreateAlternateDesktop(
      bool alternate_winstation) override {
    return sandbox::SBOX_ALL_OK;
  }
  void DestroyAlternateDesktop() override {}
  sandbox::ResultCode SetIntegrityLevel(
      sandbox::IntegrityLevel level) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::IntegrityLevel GetIntegrityLevel() const override {
    return sandbox::IntegrityLevel{};
  }
  sandbox::ResultCode SetDelayedIntegrityLevel(
      sandbox::IntegrityLevel level) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetLowBox(const wchar_t* sid) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetProcessMitigations(
      sandbox::MitigationFlags flags) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::MitigationFlags GetProcessMitigations() override {
    return sandbox::MitigationFlags{};
  }
  sandbox::ResultCode SetDelayedProcessMitigations(
      sandbox::MitigationFlags flags) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::MitigationFlags GetDelayedProcessMitigations() const override {
    return sandbox::MitigationFlags{};
  }
  sandbox::ResultCode SetDisconnectCsrss() override {
    return sandbox::SBOX_ALL_OK;
  }
  void SetStrictInterceptions() override {}
  sandbox::ResultCode SetStdoutHandle(HANDLE handle) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetStderrHandle(HANDLE handle) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode AddRule(SubSystem subsystem,
                              Semantics semantics,
                              const wchar_t* pattern) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode AddDllToUnload(const wchar_t* dll_name) override {
    blocklisted_dlls_.push_back(dll_name);
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode AddKernelObjectToClose(
      const wchar_t* handle_type,
      const wchar_t* handle_name) override {
    return sandbox::SBOX_ALL_OK;
  }
  void AddHandleToShare(HANDLE handle) override {}
  void SetLockdownDefaultDacl() override {}
  void AddRestrictingRandomSid() override {}
  void SetEnableOPMRedirection() override {}
  bool GetEnableOPMRedirection() override { return false; }

  sandbox::ResultCode AddAppContainerProfile(const wchar_t* package_name,
                                             bool create_profile) override {
    if (create_profile) {
      app_container_profile_ = sandbox::AppContainerProfileBase::Create(
          package_name, L"Sandbox", L"Sandbox");
    } else {
      app_container_profile_ =
          sandbox::AppContainerProfileBase::Open(package_name);
    }
    if (!app_container_profile_)
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE;
    return sandbox::SBOX_ALL_OK;
  }

  scoped_refptr<sandbox::AppContainerProfile> GetAppContainerProfile()
      override {
    return app_container_profile_;
  }

  scoped_refptr<sandbox::AppContainerProfileBase> GetAppContainerProfileBase() {
    return app_container_profile_;
  }

  void SetEffectiveToken(HANDLE token) override {}
  size_t GetPolicyGlobalSize() const override { return 0; }

  const std::vector<std::wstring>& blocklisted_dlls() const {
    return blocklisted_dlls_;
  }

 private:
  std::vector<std::wstring> blocklisted_dlls_;
  scoped_refptr<sandbox::AppContainerProfileBase> app_container_profile_;
};

std::vector<sandbox::Sid> GetCapabilitySids(
    const std::initializer_list<base::string16>& capabilities) {
  std::vector<sandbox::Sid> sids;
  for (const auto& capability : capabilities) {
    sids.emplace_back(sandbox::Sid::FromNamedCapability(capability.c_str()));
  }
  return sids;
}

base::string16 GetAccessAllowedForCapabilities(
    const std::initializer_list<base::string16>& capabilities) {
  base::string16 sddl = kBaseSecurityDescriptor;
  for (const auto& capability : GetCapabilitySids(capabilities)) {
    base::string16 sid_string;
    CHECK(capability.ToSddlString(&sid_string));
    base::StrAppend(&sddl, {L"(A;;GRGX;;;", sid_string, L")"});
  }
  return sddl;
}

// Drops a temporary file granting RX access to a list of capabilities.
bool DropTempFileWithSecurity(
    const base::ScopedTempDir& temp_dir,
    const std::initializer_list<base::string16>& capabilities,
    base::FilePath* path) {
  if (!base::CreateTemporaryFileInDir(temp_dir.GetPath(), path))
    return false;
  auto sddl = GetAccessAllowedForCapabilities(capabilities);
  PSECURITY_DESCRIPTOR security_descriptor = nullptr;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl.c_str(), SDDL_REVISION_1, &security_descriptor, nullptr)) {
    return false;
  }
  BOOL result = ::SetFileSecurityW(
      path->value().c_str(), DACL_SECURITY_INFORMATION, security_descriptor);
  ::LocalFree(security_descriptor);
  return !!result;
}

void EqualSidList(const std::vector<sandbox::Sid>& left,
                  const std::vector<sandbox::Sid>& right) {
  EXPECT_EQ(left.size(), right.size());
  auto result = std::mismatch(left.cbegin(), left.cend(), right.cbegin(),
                              [](const auto& left_sid, const auto& right_sid) {
                                return !!::EqualSid(left_sid.GetPSID(),
                                                    right_sid.GetPSID());
                              });
  EXPECT_EQ(result.first, left.cend());
}

void CheckCapabilities(
    sandbox::AppContainerProfileBase* profile,
    const std::initializer_list<base::string16>& additional_capabilities) {
  auto additional_caps = GetCapabilitySids(additional_capabilities);
  auto impersonation_caps =
      GetCapabilitySids({kChromeInstallFiles, klpacPnpNotifications,
                         kLpacChromeInstallFiles, kRegistryRead});
  auto base_caps = GetCapabilitySids(
      {klpacPnpNotifications, kLpacChromeInstallFiles, kRegistryRead});

  impersonation_caps.insert(impersonation_caps.end(), additional_caps.begin(),
                            additional_caps.end());
  base_caps.insert(base_caps.end(), additional_caps.begin(),
                   additional_caps.end());

  EqualSidList(impersonation_caps, profile->GetImpersonationCapabilities());
  EqualSidList(base_caps, profile->GetCapabilities());
}

class SandboxWinTest : public ::testing::Test {
 public:
  SandboxWinTest() {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {}

 protected:
  void CreateProgramFile(std::initializer_list<base::string16> capabilities,
                         base::CommandLine* command_line) {
    base::FilePath path;
    ASSERT_TRUE(DropTempFileWithSecurity(temp_dir_, capabilities, &path));
    command_line->SetProgram(path);
  }

  sandbox::ResultCode CreateAppContainerProfile(
      const base::CommandLine& base_command_line,
      bool access_check_fail,
      service_manager::SandboxType sandbox_type,
      scoped_refptr<sandbox::AppContainerProfileBase>* profile) {
    base::FilePath path;
    base::CommandLine command_line(base_command_line);

    if (access_check_fail) {
      CreateProgramFile({}, &command_line);
    } else {
      CreateProgramFile({kChromeInstallFiles, kLpacChromeInstallFiles},
                        &command_line);
    }

    TestTargetPolicy policy;
    sandbox::ResultCode result =
        service_manager::SandboxWin::AddAppContainerProfileToPolicy(
            command_line, sandbox_type, kAppContainerId, &policy);
    if (result == sandbox::SBOX_ALL_OK)
      *profile = policy.GetAppContainerProfileBase();
    return result;
  }

  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(SandboxWinTest, IsGpuAppContainerEnabled) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SandboxType::kGpu));
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(service_manager::features::kGpuAppContainer);
  EXPECT_TRUE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SandboxType::kGpu));
  EXPECT_FALSE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SandboxType::kNoSandbox));
}

TEST_F(SandboxWinTest, AppContainerAccessCheckFail) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, true, SandboxType::kGpu, &profile);
  EXPECT_EQ(sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_ACCESS_CHECK,
            result);
  EXPECT_EQ(nullptr, profile);
}

TEST_F(SandboxWinTest, AppContainerCheckProfile) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, false, SandboxType::kGpu, &profile);
  ASSERT_EQ(sandbox::SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  auto package_sid = sandbox::Sid::FromSddlString(kPackageSid);
  ASSERT_TRUE(package_sid.IsValid());
  EXPECT_TRUE(
      ::EqualSid(package_sid.GetPSID(), profile->GetPackageSid().GetPSID()));
  EXPECT_TRUE(profile->GetEnableLowPrivilegeAppContainer());
  CheckCapabilities(profile.get(), {});
}

TEST_F(SandboxWinTest, AppContainerCheckProfileDisableLpac) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(service_manager::features::kGpuLPAC);
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, false, SandboxType::kGpu, &profile);
  ASSERT_EQ(sandbox::SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  EXPECT_FALSE(profile->GetEnableLowPrivilegeAppContainer());
}

TEST_F(SandboxWinTest, AppContainerCheckProfileAddCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAddGpuAppContainerCaps,
                                 "  cap1   ,   cap2   ,");
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, false, SandboxType::kGpu, &profile);
  ASSERT_EQ(sandbox::SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  CheckCapabilities(profile.get(), {L"cap1", L"cap2"});
}

TEST_F(SandboxWinTest, BlocklistAddOneDllCheckInBrowser) {
  {  // Block loaded module.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"kernel32.dll", true, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({L"kernel32.dll"}));
  }

  {  // Block module which is not loaded.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"notloaded.dll", true, &policy);
    EXPECT_TRUE(policy.blocklisted_dlls().empty());
  }

  {  // Block module loaded by short name.
#if defined(ARCH_CPU_X86)
    const std::wstring short_dll_name = L"pe_ima~1.dll";
    const std::wstring full_dll_name = L"pe_image_test_32.dll";
#elif defined(ARCH_CPU_X86_64)
    const std::wstring short_dll_name = L"pe_ima~2.dll";
    const std::wstring full_dll_name = L"pe_image_test_64.dll";
#elif defined(ARCH_CPU_ARM64)
    const std::wstring short_dll_name = L"pe_ima~3.dll";
    const std::wstring full_dll_name = L"pe_image_test_arm64.dll";
#endif

    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_TEST_DATA, &test_data_dir);
    auto dll_path =
        test_data_dir.AppendASCII("pe_image").Append(short_dll_name);

    base::ScopedNativeLibrary library(dll_path);
    EXPECT_TRUE(library.is_valid());

    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(full_dll_name.c_str(), true, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({short_dll_name, full_dll_name}));
  }
}

TEST_F(SandboxWinTest, BlocklistAddOneDllDontCheckInBrowser) {
  {  // Block module with short name.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"short.dll", false, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({L"short.dll"}));
  }

  {  // Block module with long name.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"thelongname.dll", false, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({L"thelongname.dll", L"thelon~1.dll",
                                         L"thelon~2.dll", L"thelon~3.dll"}));
  }
}

}  // namespace service_manager
