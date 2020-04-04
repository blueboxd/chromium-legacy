// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/dns/dns_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kDohServerTemplate[] =
    "https://doh1.test https://doh2.test/query{?dns}";

// Override the reader to mock out the ShouldDisableDohFor...() methods.
class MockedStubResolverConfigReader : public StubResolverConfigReader {
 public:
  explicit MockedStubResolverConfigReader(PrefService* local_state)
      : StubResolverConfigReader(local_state,
                                 false /* set_up_pref_defaults */) {}

  bool ShouldDisableDohForManaged() override { return disable_for_managed_; }

  bool ShouldDisableDohForParentalControls() override {
    parental_controls_checked_ = true;
    return disable_for_parental_controls_;
  }

  void set_disable_for_managed() { disable_for_managed_ = true; }

  void set_disable_for_parental_controls() {
    disable_for_parental_controls_ = true;
  }

  bool parental_controls_checked() { return parental_controls_checked_; }

 private:
  bool disable_for_managed_ = false;
  bool disable_for_parental_controls_ = false;

  bool parental_controls_checked_ = false;
};

class StubResolverConfigReaderTest : public testing::Test {
 public:
  StubResolverConfigReaderTest() {
    StubResolverConfigReader::RegisterPrefs(local_state_.registry());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<MockedStubResolverConfigReader> config_reader_ =
      std::make_unique<MockedStubResolverConfigReader>(&local_state_);
};

TEST_F(StubResolverConfigReaderTest, GetConfiguration) {
  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  config_reader_->GetConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_FALSE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::OFF);
  EXPECT_TRUE(dns_over_https_servers.empty());

  // Parental controls should not be checked when DoH otherwise disabled.
  EXPECT_FALSE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DohEnabled) {
  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  config_reader_->GetConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::AUTOMATIC);
  EXPECT_THAT(dns_over_https_servers,
              testing::ElementsAre(
                  net::DnsOverHttpsServerConfig("https://doh1.test",
                                                true /* use_post */),
                  net::DnsOverHttpsServerConfig("https://doh2.test/query{?dns}",
                                                false /* use_post */)));

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DohEnabled_Secure) {
  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeSecure);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  // |force_check_parental_controls_for_automatic_mode| should have no effect on
  // SECURE mode, so set to false to ensure check is not deferred.
  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::SECURE);
  EXPECT_THAT(dns_over_https_servers,
              testing::ElementsAre(
                  net::DnsOverHttpsServerConfig("https://doh1.test",
                                                true /* use_post */),
                  net::DnsOverHttpsServerConfig("https://doh2.test/query{?dns}",
                                                false /* use_post */)));

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForManaged) {
  config_reader_->set_disable_for_managed();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  config_reader_->GetConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::OFF);
  EXPECT_TRUE(dns_over_https_servers.empty());

  // Parental controls should not be checked when DoH otherwise disabled.
  EXPECT_FALSE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForManaged_Secure) {
  config_reader_->set_disable_for_managed();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeSecure);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::OFF);
  EXPECT_TRUE(dns_over_https_servers.empty());

  // Parental controls should not be checked when DoH otherwise disabled.
  EXPECT_FALSE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForParentalControls) {
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  config_reader_->GetConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::OFF);
  EXPECT_TRUE(dns_over_https_servers.empty());

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForParentalControls_Secure) {
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeSecure);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  // |force_check_parental_controls_for_automatic_mode| should have no effect on
  // SECURE mode, so set to false to ensure check is not deferred.
  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::OFF);
  EXPECT_TRUE(dns_over_https_servers.empty());

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DeferredParentalControlsCheck) {
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohServerTemplate);

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  // Parental controls check initially skipped.
  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::AUTOMATIC);
  EXPECT_THAT(dns_over_https_servers,
              testing::ElementsAre(
                  net::DnsOverHttpsServerConfig("https://doh1.test",
                                                true /* use_post */),
                  net::DnsOverHttpsServerConfig("https://doh2.test/query{?dns}",
                                                false /* use_post */)));
  EXPECT_FALSE(config_reader_->parental_controls_checked());

  task_environment_.AdvanceClock(
      StubResolverConfigReader::kParentalControlsCheckDelay);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(config_reader_->parental_controls_checked());

  dns_over_https_servers.clear();
  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::OFF);
  EXPECT_TRUE(dns_over_https_servers.empty());
}

TEST_F(StubResolverConfigReaderTest, DeferredParentalControlsCheck_Managed) {
  config_reader_->set_disable_for_managed();
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetManagedPref(
      prefs::kDnsOverHttpsMode,
      std::make_unique<base::Value>(
          chrome_browser_net::kDnsOverHttpsModeAutomatic));
  local_state_.SetManagedPref(
      prefs::kDnsOverHttpsTemplates,
      std::make_unique<base::Value>(kDohServerTemplate));

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;

  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  // Parental controls check initially skipped, and managed prefs take
  // precedence over disables.
  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::AUTOMATIC);
  EXPECT_THAT(dns_over_https_servers,
              testing::ElementsAre(
                  net::DnsOverHttpsServerConfig("https://doh1.test",
                                                true /* use_post */),
                  net::DnsOverHttpsServerConfig("https://doh2.test/query{?dns}",
                                                false /* use_post */)));
  EXPECT_FALSE(config_reader_->parental_controls_checked());

  task_environment_.AdvanceClock(
      StubResolverConfigReader::kParentalControlsCheckDelay);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(config_reader_->parental_controls_checked());

  dns_over_https_servers.clear();
  config_reader_->GetConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers);

  // Expect DoH still enabled after parental controls check because managed
  // prefs have precedence.
  EXPECT_TRUE(insecure_stub_resolver_enabled);
  EXPECT_EQ(secure_dns_mode, net::DnsConfig::SecureDnsMode::AUTOMATIC);
  EXPECT_THAT(dns_over_https_servers,
              testing::ElementsAre(
                  net::DnsOverHttpsServerConfig("https://doh1.test",
                                                true /* use_post */),
                  net::DnsOverHttpsServerConfig("https://doh2.test/query{?dns}",
                                                false /* use_post */)));
}

}  // namespace
