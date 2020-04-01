// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy {

namespace {

// Creates a new PolicyLoaderIOS and verifies that it does not load any
// policies.
void VerifyNoPoliciesAreLoaded() {
  PolicyBundle empty;
  scoped_refptr<base::TestSimpleTaskRunner> taskRunner =
      new base::TestSimpleTaskRunner();
  PolicyLoaderIOS loader(taskRunner);
  std::unique_ptr<PolicyBundle> bundle = loader.Load();
  ASSERT_TRUE(bundle);
  EXPECT_TRUE(bundle->Equals(empty));
}

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  ~TestHarness() override;

  void SetUp() override;

  ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

  void InstallEmptyPolicy() override;
  void InstallStringPolicy(const std::string& policy_name,
                           const std::string& policy_value) override;
  void InstallIntegerPolicy(const std::string& policy_name,
                            int policy_value) override;
  void InstallBooleanPolicy(const std::string& policy_name,
                            bool policy_value) override;
  void InstallStringListPolicy(const std::string& policy_name,
                               const base::ListValue* policy_value) override;
  void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) override;

  static PolicyProviderTestHarness* Create();

 private:
  // Merges the policies in |policy| into the current policy dictionary
  // in NSUserDefaults, after making sure that the policy dictionary
  // exists.
  void AddPolicies(NSDictionary* policy);

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY,
                                POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM) {}

TestHarness::~TestHarness() {
  // Cleanup any policies left from the test.
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

void TestHarness::SetUp() {
  // Make sure there is no pre-existing policy present. Ensure that
  // kPolicyLoaderIOSLoadPolicyKey is set, or else the loader won't load any
  // policy data.
  NSDictionary* policy = @{kPolicyLoaderIOSLoadPolicyKey : @YES};
  [[NSUserDefaults standardUserDefaults]
      setObject:policy
         forKey:kPolicyLoaderIOSConfigurationKey];
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return new AsyncPolicyProvider(
      registry, std::make_unique<PolicyLoaderIOS>(task_runner));
}

void TestHarness::InstallEmptyPolicy() {
  AddPolicies(@{});
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  NSString* value = base::SysUTF8ToNSString(policy_value);
  AddPolicies(@{
      key: value
  });
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  AddPolicies(@{
      key: [NSNumber numberWithInt:policy_value]
  });
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  AddPolicies(@{
      key: [NSNumber numberWithBool:policy_value]
  });
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      ValueToProperty(*policy_value));
  AddPolicies(@{key : (__bridge NSArray*)(value.get())});
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      ValueToProperty(*policy_value));
  AddPolicies(@{key : (__bridge NSDictionary*)(value.get())});
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

void TestHarness::AddPolicies(NSDictionary* policy) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSMutableDictionary* chromePolicy = [[NSMutableDictionary alloc] init];

  NSDictionary* previous =
      [defaults dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  if (previous) {
    [chromePolicy addEntriesFromDictionary:previous];
  }

  [chromePolicy addEntriesFromDictionary:policy];
  [[NSUserDefaults standardUserDefaults]
      setObject:chromePolicy
         forKey:kPolicyLoaderIOSConfigurationKey];
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(PolicyProviderIOSChromePolicyTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::Create));

using PolicyLoaderIOSTest = PlatformTest;

// Verifies that policies are not loaded if kPolicyLoaderIOSLoadPolicyKey is not
// present.
TEST_F(PolicyLoaderIOSTest, NoPoliciesLoadedWithoutLoadPolicyKey) {
  NSDictionary* policy = @{
    @"shared": @"wrong",
    @"key1": @"value1",
  };
  [[NSUserDefaults standardUserDefaults]
      setObject:policy
         forKey:kPolicyLoaderIOSConfigurationKey];

  VerifyNoPoliciesAreLoaded();
}

// Verifies that policies are not loaded if kPolicyLoaderIOSLoadPolicyKey is set
// to false.
TEST_F(PolicyLoaderIOSTest, NoPoliciesLoadedWhenEnableChromeKeyIsFalse) {
  NSDictionary* policy = @{
    kPolicyLoaderIOSLoadPolicyKey : @NO,
    @"shared" : @"wrong",
    @"key1" : @"value1",
  };
  [[NSUserDefaults standardUserDefaults]
      setObject:policy
         forKey:kPolicyLoaderIOSConfigurationKey];

  VerifyNoPoliciesAreLoaded();
}

}  // namespace policy
