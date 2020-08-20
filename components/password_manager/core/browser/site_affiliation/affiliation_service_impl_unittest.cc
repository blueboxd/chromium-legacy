// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/test_affiliation_fetcher_factory.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "components/sync/driver/test_sync_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace password_manager {

namespace {

constexpr char kTestUrlChar1[] = "https://1.example.com";
constexpr char kTestUrlChar2[] = "https://2.example.com";
constexpr char kTestUrlChar3[] = "https://3.example.com";
constexpr char kTestUrlChar4[] = "https://4.example.com";
constexpr char kTestUrlChar5[] = "https://5.example.com";

url::SchemeHostPort ToSchemeHostPort(const std::string& url) {
  return url::SchemeHostPort(GURL(url));
}

std::vector<FacetURI> SchemeHostPortsToFacetsURIs(
    const std::vector<url::SchemeHostPort>& scheme_host_ports) {
  std::vector<FacetURI> facet_URIs;
  for (const auto& scheme_host_port : scheme_host_ports) {
    facet_URIs.push_back(
        FacetURI::FromCanonicalSpec(scheme_host_port.Serialize()));
  }
  return facet_URIs;
}

}  // namespace

class MockAffiliationFetcherFactory : public TestAffiliationFetcherFactory {
 public:
  MockAffiliationFetcherFactory() = default;
  ~MockAffiliationFetcherFactory() override = default;

  MOCK_METHOD(
      AffiliationFetcherInterface*,
      CreateInstance,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       AffiliationFetcherDelegate* delegate),
      (override));
};

class AffiliationServiceImplTest : public testing::Test {
 public:
  AffiliationServiceImplTest()
      : sync_service_(std::make_unique<syncer::TestSyncService>()),
        service_(sync_service_.get(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    AffiliationFetcher::SetFactoryForTesting(mock_fetcher_factory());
  }

  ~AffiliationServiceImplTest() override {
    AffiliationFetcher::SetFactoryForTesting(nullptr);
  }

  syncer::TestSyncService* sync_service() { return sync_service_.get(); }
  AffiliationServiceImpl* service() { return &service_; }
  MockAffiliationFetcherFactory* mock_fetcher_factory() {
    return &mock_fetcher_factory_;
  }

  // Sets TestSyncService flags.
  void SetSyncServiceStates(bool is_setup_completed, bool is_passphrase_set) {
    sync_service()->SetFirstSetupComplete(is_setup_completed);
    sync_service()->SetIsUsingSecondaryPassphrase(is_passphrase_set);
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  AffiliationServiceImpl service_;
  MockAffiliationFetcherFactory mock_fetcher_factory_;
};

TEST_F(AffiliationServiceImplTest, GetChangePasswordURLReturnsEmpty) {
  auto scheme_host_port = ToSchemeHostPort(kTestUrlChar1);

  EXPECT_EQ(GURL(), service()->GetChangePasswordURL(scheme_host_port));
}

TEST_F(AffiliationServiceImplTest, ClearStopsOngoingAffiliationFetcherRequest) {
  auto tuple_origins = {ToSchemeHostPort(kTestUrlChar1),
                        ToSchemeHostPort(kTestUrlChar2)};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));
  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins)));

  service()->PrefetchChangePasswordURLs(tuple_origins);
  EXPECT_NE(nullptr, service()->GetFetcherForTesting());

  service()->Clear();
  EXPECT_EQ(nullptr, service()->GetFetcherForTesting());
}

TEST_F(AffiliationServiceImplTest,
       EachPrefetchCallCreatesNewAffiliationFetcherInstance) {
  auto scheme_host_port1 = ToSchemeHostPort(kTestUrlChar1);
  auto scheme_host_port2 = ToSchemeHostPort(kTestUrlChar2);
  auto scheme_host_port3 = ToSchemeHostPort(kTestUrlChar3);
  auto scheme_host_port4 = ToSchemeHostPort(kTestUrlChar4);
  auto scheme_host_port5 = ToSchemeHostPort(kTestUrlChar5);

  auto tuple_origins_1 = {scheme_host_port1, scheme_host_port2,
                          scheme_host_port3};
  auto tuple_origins_2 = {scheme_host_port3, scheme_host_port4,
                          scheme_host_port5};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();
  MockAffiliationFetcher* new_mock_fetcher = new MockAffiliationFetcher();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins_1)));
  EXPECT_CALL(*new_mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins_2)));
  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher))
      .WillOnce(Return(new_mock_fetcher));

  service()->PrefetchChangePasswordURLs(tuple_origins_1);
  service()->PrefetchChangePasswordURLs(tuple_origins_2);
}

TEST_F(AffiliationServiceImplTest,
       FetchRequiresCompleteSetupAndPassphraseDisabled) {
  auto tuple_origins = {ToSchemeHostPort(kTestUrlChar1),
                        ToSchemeHostPort(kTestUrlChar2)};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();

  // The only scenario when the StartRequest() should be called.
  // Setup is completed and secondary passphrase feature is disabled.
  SetSyncServiceStates(/*is_setup_completed=*/true,
                       /*is_passphrase_set=*/false);

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));
  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins)));

  service()->PrefetchChangePasswordURLs(tuple_origins);
}

TEST_F(AffiliationServiceImplTest, SecondaryPassphraseSetPreventsFetch) {
  auto tuple_origins = {ToSchemeHostPort(kTestUrlChar1),
                        ToSchemeHostPort(kTestUrlChar2)};
  SetSyncServiceStates(/*is_setup_completed=*/true, /*is_passphrase_set=*/true);

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance).Times(0);

  service()->PrefetchChangePasswordURLs(tuple_origins);
}

TEST_F(AffiliationServiceImplTest, SetupNotCompletedPreventsFetch) {
  auto tuple_origins = {ToSchemeHostPort(kTestUrlChar1),
                        ToSchemeHostPort(kTestUrlChar2)};
  SetSyncServiceStates(/*is_setup_completed=*/false,
                       /*is_passphrase_set=*/false);

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance).Times(0);

  service()->PrefetchChangePasswordURLs(tuple_origins);
}

}  // namespace password_manager
