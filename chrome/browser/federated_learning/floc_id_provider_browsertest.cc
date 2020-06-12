// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"

namespace federated_learning {

class FlocIdProviderBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  FlocIdProvider* floc_id_provider() {
    return FlocIdProviderFactory::GetForProfile(browser()->profile());
  }

  FlocId GetFlocId() {
    return static_cast<FlocIdProviderImpl*>(floc_id_provider())->floc_id_;
  }

  std::string test_host() const { return "a.test"; }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderBrowserTest, NoProviderInIncognitoMode) {
  FlocIdProvider* original_provider = floc_id_provider();
  ASSERT_TRUE(original_provider);

  GURL url = https_server_.GetURL(test_host(), "/title1.html");
  ui_test_utils::NavigateToURL(CreateIncognitoBrowser(), url);

  ASSERT_TRUE(browser()->profile()->HasOffTheRecordProfile());

  Profile* off_the_record_profile =
      browser()->profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(off_the_record_profile);

  FlocIdProvider* incognito_floc_id_provider =
      FlocIdProviderFactory::GetForProfile(off_the_record_profile);
  ASSERT_FALSE(incognito_floc_id_provider);
}

class MockFlocIdProvider : public FlocIdProviderImpl {
 public:
  using FlocIdProviderImpl::FlocIdProviderImpl;

  bool IsSwaaNacAccountEnabled() override { return true; }
};

class FlocIdProviderWithCustomizedServicesBrowserTest
    : public FlocIdProviderBrowserTest {
 public:
  FlocIdProviderWithCustomizedServicesBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFlocIdComputedEventLogging}, {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(
                    &FlocIdProviderWithCustomizedServicesBrowserTest::
                        OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  std::vector<GURL> GetHistoryUrls() {
    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    return enumerator.urls();
  }

  void FinishOutstandingHistoryQueries() {
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    HistoryServiceFactory::GetForProfile(browser()->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->QueryHistory(
            base::string16(), history::QueryOptions(),
            base::BindLambdaForTesting(
                [&](history::QueryResults results) { run_loop.Quit(); }),
            &tracker);
    run_loop.Run();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(browser()->profile()));
  }

  syncer::FakeUserEventService* user_event_service() {
    return static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetForProfile(
            browser()->profile()));
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &FlocIdProviderWithCustomizedServicesBrowserTest::CreateSyncService,
            base::Unretained(this)));

    browser_sync::UserEventServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderWithCustomizedServicesBrowserTest::
                                CreateUserEventService,
                            base::Unretained(this)));

    FlocIdProviderFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderWithCustomizedServicesBrowserTest::
                                CreateFlocIdProvider,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateSyncService(
      content::BrowserContext* context) {
    auto sync_service = std::make_unique<syncer::TestSyncService>();

    syncer::ModelTypeSet types = syncer::ModelTypeSet::All();
    types.Remove(syncer::HISTORY_DELETE_DIRECTIVES);
    sync_service->SetActiveDataTypes(types);

    return std::move(sync_service);
  }

  std::unique_ptr<KeyedService> CreateUserEventService(
      content::BrowserContext* context) {
    return std::make_unique<syncer::FakeUserEventService>();
  }

  std::unique_ptr<KeyedService> CreateFlocIdProvider(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);

    auto floc_id_provider = std::make_unique<MockFlocIdProvider>(
        ProfileSyncServiceFactory::GetForProfile(profile),
        CookieSettingsFactory::GetForProfile(profile),
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS),
        browser_sync::UserEventServiceFactory::GetForProfile(profile));
    return std::move(floc_id_provider);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      subscription_;
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       FlocIdValue_OneNavigation) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/title1.html"));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_EQ(GetFlocId().ToDebugHeaderValue(), FlocId().ToDebugHeaderValue());

  // Turn on sync-history to trigger the start of the 1st floc session.
  sync_service()->SetActiveDataTypes(syncer::ModelTypeSet::All());
  sync_service()->FireStateChanged();

  FinishOutstandingHistoryQueries();

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::NEW,
            event.event_trigger());
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToUint64(),
            event.floc_id());
}

}  // namespace federated_learning
