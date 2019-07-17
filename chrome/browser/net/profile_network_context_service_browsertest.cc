// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/policy/policy_constants.h"
#include "net/base/features.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// Most tests for this class are in NetworkContextConfigurationBrowserTest.
class ProfileNetworkContextServiceBrowsertest : public InProcessBrowserTest {
 public:
  ProfileNetworkContextServiceBrowsertest() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  ~ProfileNetworkContextServiceBrowsertest() override {}

  void SetUpOnMainThread() override {
    loader_factory_ = content::BrowserContext::GetDefaultStoragePartition(
                          browser()->profile())
                          ->GetURLLoaderFactoryForBrowserProcess()
                          .get();
  }

  network::mojom::URLLoaderFactory* loader_factory() const {
    return loader_factory_;
  }

 private:
  network::mojom::URLLoaderFactory* loader_factory_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceBrowsertest,
                       DiskCacheLocation) {
  // Run a request that caches the response, to give the network service time to
  // create a cache directory.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/cachetime");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  base::FilePath expected_cache_path;
  chrome::GetUserCacheDirectory(browser()->profile()->GetPath(),
                                &expected_cache_path);
  expected_cache_path = expected_cache_path.Append(chrome::kCacheDirname);
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(expected_cache_path));
}

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceBrowsertest, BrotliEnabled) {
  // Brotli is only used over encrypted connections.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = https_server.GetURL("/echoheader?accept-encoding");

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());
  std::vector<std::string> encodings =
      base::SplitString(*simple_loader_helper.response_body(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  EXPECT_TRUE(base::Contains(encodings, "br"));
}

// Test subclass that adds switches::kDiskCacheDir to the command line, to make
// sure it's respected.
class ProfileNetworkContextServiceDiskCacheDirBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextServiceDiskCacheDirBrowsertest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ~ProfileNetworkContextServiceDiskCacheDirBrowsertest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchPath(switches::kDiskCacheDir,
                                   temp_dir_.GetPath());
  }

  const base::FilePath& TempPath() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Makes sure switches::kDiskCacheDir is hooked up correctly.
IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceDiskCacheDirBrowsertest,
                       DiskCacheLocation) {
  // Make sure command line switch is hooked up to the pref.
  ASSERT_EQ(TempPath(), browser()->profile()->GetPrefs()->GetFilePath(
                            prefs::kDiskCacheDir));

  // Run a request that caches the response, to give the network service time to
  // create a cache directory.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/cachetime");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  // Cache directory should now exist.
  base::FilePath expected_cache_path =
      TempPath()
          .Append(browser()->profile()->GetPath().BaseName())
          .Append(chrome::kCacheDirname);
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(expected_cache_path));
}

#if defined(OS_CHROMEOS)
// Base class for verifying which certificate verifier is being used on Chrome
// OS depending on feature state and policies.
class ProfileNetworkContextServiceCertVerifierBrowsertestBase
    : public policy::LoginPolicyTestBase {
 public:
  ProfileNetworkContextServiceCertVerifierBrowsertestBase() = default;
  ~ProfileNetworkContextServiceCertVerifierBrowsertestBase() override = default;

 protected:
  void SetPolicyValue(base::StringPiece policy_key, base::Value value) {
    policy_values_.SetKey(policy_key, std::move(value));
    user_policy_helper()->SetPolicy(policy_values_,
                                    base::Value(base::Value::Type::DICTIONARY));
  }

  bool IsSigninProfileUsingBuiltinCertVerifier() {
    Profile* const profile = chromeos::ProfileHelper::GetSigninProfile();
    ProfileNetworkContextService* const service =
        ProfileNetworkContextServiceFactory::GetForContext(profile);
    return service->using_builtin_cert_verifier();
  }

  bool IsActiveProfileUsingBuiltinCertVerifier() {
    Profile* const profile = GetProfileForActiveUser();
    ProfileNetworkContextService* const service =
        ProfileNetworkContextServiceFactory::GetForContext(profile);
    return service->using_builtin_cert_verifier();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::Value policy_values_{base::Value::Type::DICTIONARY};

  DISALLOW_COPY_AND_ASSIGN(
      ProfileNetworkContextServiceCertVerifierBrowsertestBase);
};

// When using this class, the built-in certificate verifier has been enabled
// using the UseBuiltinCertVerifier feature.
class ProfileNetworkContextServiceCertVerifierBuiltinEnabledBrowsertest
    : public ProfileNetworkContextServiceCertVerifierBrowsertestBase {
 public:
  ProfileNetworkContextServiceCertVerifierBuiltinEnabledBrowsertest() = default;
  ~ProfileNetworkContextServiceCertVerifierBuiltinEnabledBrowsertest()
      override = default;

  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kCertVerifierBuiltinFeature);
    ProfileNetworkContextServiceCertVerifierBrowsertestBase::
        SetUpInProcessBrowserTestFixture();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ProfileNetworkContextServiceCertVerifierBuiltinEnabledBrowsertest);
};

// If the built-in cert verifier is enabled and no policy is present, it should
// be enabled on the sign-in screen and in the user profile.
IN_PROC_BROWSER_TEST_F(
    ProfileNetworkContextServiceCertVerifierBuiltinEnabledBrowsertest,
    TurnedOnByFeature) {
  SkipToLoginScreen();
  EXPECT_TRUE(IsSigninProfileUsingBuiltinCertVerifier());

  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  EXPECT_TRUE(IsActiveProfileUsingBuiltinCertVerifier());
}

// If the built-in cert verifier is enabled, but user policy says to disable it,
// it should be disabled in the user profile.
IN_PROC_BROWSER_TEST_F(
    ProfileNetworkContextServiceCertVerifierBuiltinEnabledBrowsertest,
    TurnedOffByLegacyPolicy) {
  SkipToLoginScreen();

  SetPolicyValue(policy::key::kBuiltinCertificateVerifierEnabled,
                 base::Value(false));
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  EXPECT_FALSE(IsActiveProfileUsingBuiltinCertVerifier());
}

// When using this class, the built-in certificate verifier has been disabled
// using the UseBuiltinCertVerifier feature.
class ProfileNetworkContextServiceCertVerifierBuiltinDisabledBrowsertest
    : public ProfileNetworkContextServiceCertVerifierBrowsertestBase {
 public:
  ProfileNetworkContextServiceCertVerifierBuiltinDisabledBrowsertest() =
      default;
  ~ProfileNetworkContextServiceCertVerifierBuiltinDisabledBrowsertest()
      override = default;

  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kCertVerifierBuiltinFeature);
    ProfileNetworkContextServiceCertVerifierBrowsertestBase::
        SetUpInProcessBrowserTestFixture();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ProfileNetworkContextServiceCertVerifierBuiltinDisabledBrowsertest);
};

// If the built-in cert verifier is disabled, it should be disabled everywhere.
IN_PROC_BROWSER_TEST_F(
    ProfileNetworkContextServiceCertVerifierBuiltinDisabledBrowsertest,
    TurnedOffByFeature) {
  SkipToLoginScreen();
  EXPECT_FALSE(IsSigninProfileUsingBuiltinCertVerifier());

  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  EXPECT_FALSE(IsActiveProfileUsingBuiltinCertVerifier());
}

// If the built-in cert verifier is disabled, but policy force-enables it for a
// profile, it should be enabled in the profile.
IN_PROC_BROWSER_TEST_F(
    ProfileNetworkContextServiceCertVerifierBuiltinDisabledBrowsertest,
    TurnedOffByFeatureOverrideByPolicy) {
  SkipToLoginScreen();
  EXPECT_FALSE(IsSigninProfileUsingBuiltinCertVerifier());

  SetPolicyValue(policy::key::kBuiltinCertificateVerifierEnabled,
                 base::Value(true));
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  EXPECT_TRUE(IsActiveProfileUsingBuiltinCertVerifier());
}

#endif  // defined(OS_CHROMEOS)

}  // namespace
