// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "mojo/core/embedder/embedder.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_network_context_client.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

const int kProcessId = 42;
const int kRoutingId = 43;

class RecordingNetworkContextClient : public network::TestNetworkContextClient {
 public:
  struct CookieOp {
    bool get = false;
    GURL url;
    GURL site_for_cookies;
    // The vector is merely to permit use of MatchesCookieLine, there is always
    // one thing.
    std::vector<net::CanonicalCookie> cookie;
    net::CanonicalCookie::CookieInclusionStatus status;
  };

  RecordingNetworkContextClient() {}
  ~RecordingNetworkContextClient() override {}

  const std::vector<CookieOp>& recorded_activity() const {
    return recorded_activity_;
  }

  void OnCookiesChanged(
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id,
      const GURL& url,
      const GURL& site_for_cookies,
      const std::vector<net::CookieWithStatus>& cookie_list) override {
    EXPECT_EQ(false, is_service_worker);
    EXPECT_EQ(kProcessId, process_id);
    EXPECT_EQ(kRoutingId, routing_id);
    for (const auto& cookie_and_status : cookie_list) {
      CookieOp set;
      set.url = url;
      set.site_for_cookies = site_for_cookies;
      set.cookie.push_back(cookie_and_status.cookie);
      set.status = cookie_and_status.status;
      recorded_activity_.push_back(set);
    }
  }

  void OnCookiesRead(
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id,
      const GURL& url,
      const GURL& site_for_cookies,
      const std::vector<net::CookieWithStatus>& cookie_list) override {
    EXPECT_EQ(false, is_service_worker);
    EXPECT_EQ(kProcessId, process_id);
    EXPECT_EQ(kRoutingId, routing_id);
    for (const auto& cookie_and_status : cookie_list) {
      CookieOp get;
      get.get = true;
      get.url = url;
      get.site_for_cookies = site_for_cookies;
      get.cookie.push_back(cookie_and_status.cookie);
      get.status = cookie_and_status.status;
      recorded_activity_.push_back(get);
    }
  }

 private:
  std::vector<CookieOp> recorded_activity_;
};

// Synchronous proxies to a wrapped RestrictedCookieManager's methods.
class RestrictedCookieManagerSync {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit RestrictedCookieManagerSync(
      mojom::RestrictedCookieManager* cookie_service)
      : cookie_service_(cookie_service) {}
  ~RestrictedCookieManagerSync() {}

  std::vector<net::CanonicalCookie> GetAllForUrl(
      const GURL& url,
      const GURL& site_for_cookies,
      mojom::CookieManagerGetOptionsPtr options) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> result;
    cookie_service_->GetAllForUrl(
        url, site_for_cookies, std::move(options),
        base::BindLambdaForTesting(
            [&run_loop,
             &result](const std::vector<net::CanonicalCookie>& backend_result) {
              result = backend_result;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const GURL& site_for_cookies) {
    base::RunLoop run_loop;
    bool result = false;
    cookie_service_->SetCanonicalCookie(
        cookie, url, site_for_cookies,
        base::BindLambdaForTesting([&run_loop, &result](bool backend_result) {
          result = backend_result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void AddChangeListener(const GURL& url,
                         const GURL& site_for_cookies,
                         mojom::CookieChangeListenerPtr listener) {
    base::RunLoop run_loop;
    cookie_service_->AddChangeListener(
        url, site_for_cookies, std::move(listener), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  mojom::RestrictedCookieManager* cookie_service_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedCookieManagerSync);
};

class RestrictedCookieManagerTest : public testing::Test {
 public:
  RestrictedCookieManagerTest()
      : cookie_monster_(nullptr, nullptr /* netlog */),
        service_(std::make_unique<RestrictedCookieManager>(
            &cookie_monster_,
            &cookie_settings_,
            url::Origin::Create(GURL("http://example.com")),
            &recording_client_,
            false /* is_service_worker*/,
            kProcessId,
            kRoutingId)),
        binding_(service_.get(), mojo::MakeRequest(&service_ptr_)) {
    sync_service_ =
        std::make_unique<RestrictedCookieManagerSync>(service_ptr_.get());
  }
  ~RestrictedCookieManagerTest() override {}

  void SetUp() override {
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &RestrictedCookieManagerTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  // Set a canonical cookie directly into the store.
  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          std::string source_scheme,
                          bool can_modify_httponly) {
    net::ResultSavingCookieCallback<net::CanonicalCookie::CookieInclusionStatus>
        callback;
    net::CookieOptions options;
    if (can_modify_httponly)
      options.set_include_httponly();
    cookie_monster_.SetCanonicalCookieAsync(
        std::make_unique<net::CanonicalCookie>(cookie),
        std::move(source_scheme), options,
        base::BindOnce(&net::ResultSavingCookieCallback<
                           net::CanonicalCookie::CookieInclusionStatus>::Run,
                       base::Unretained(&callback)));
    callback.WaitUntilDone();
    return callback.result() ==
           net::CanonicalCookie::CookieInclusionStatus::INCLUDE;
  }

  // Simplified helper for SetCanonicalCookie.
  //
  // Creates a CanonicalCookie that is not secure, not http-only,
  // and not restricted to first parties. Crashes if the creation fails.
  void SetSessionCookie(const char* name,
                        const char* value,
                        const char* domain,
                        const char* path) {
    CHECK(SetCanonicalCookie(
        net::CanonicalCookie(name, value, domain, path, base::Time(),
                             base::Time(), base::Time(), /* secure = */ false,
                             /* httponly = */ false,
                             net::CookieSameSite::NO_RESTRICTION,
                             net::COOKIE_PRIORITY_DEFAULT),
        "https", /* can_modify_httponly = */ true));
  }

  void ExpectBadMessage() { expecting_bad_message_ = true; }

  bool received_bad_message() { return received_bad_message_; }

 protected:
  void OnBadMessage(const std::string& reason) {
    EXPECT_TRUE(expecting_bad_message_) << "Unexpected bad message: " << reason;
    received_bad_message_ = true;
  }

  const std::vector<RecordingNetworkContextClient::CookieOp>&
  recorded_activity() const {
    return recording_client_.recorded_activity();
  }

  base::MessageLoopForIO message_loop_;
  net::CookieMonster cookie_monster_;
  CookieSettings cookie_settings_;
  RecordingNetworkContextClient recording_client_;
  std::unique_ptr<RestrictedCookieManager> service_;
  mojom::RestrictedCookieManagerPtr service_ptr_;
  mojo::Binding<mojom::RestrictedCookieManager> binding_;
  std::unique_ptr<RestrictedCookieManagerSync> sync_service_;
  bool expecting_bad_message_ = false;
  bool received_bad_message_ = false;
};

namespace {

bool CompareCanonicalCookies(const net::CanonicalCookie& c1,
                             const net::CanonicalCookie& c2) {
  return c1.PartialCompare(c2);
}

}  // anonymous namespace

TEST_F(RestrictedCookieManagerTest, GetAllForUrlBlankFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(2));
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("cookie-name", cookies[0].Name());
  EXPECT_EQ("cookie-value", cookies[0].Value());

  EXPECT_EQ("cookie-name-2", cookies[1].Name());
  EXPECT_EQ("cookie-value-2", cookies[1].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlEmptyFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(0));
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlEqualsMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(1));

  EXPECT_EQ("cookie-name", cookies[0].Name());
  EXPECT_EQ("cookie-value", cookies[0].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlStartsWithMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("cookie-name-2b", "cookie-value-2b", "example.com", "/");
  SetSessionCookie("cookie-name-3b", "cookie-value-3b", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name-2";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(2));
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("cookie-name-2", cookies[0].Name());
  EXPECT_EQ("cookie-value-2", cookies[0].Value());

  EXPECT_EQ("cookie-name-2b", cookies[1].Name());
  EXPECT_EQ("cookie-value-2b", cookies[1].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlFromWrongOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  ExpectBadMessage();
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://not-example.com/test/"), GURL("http://not-example.com"),
      std::move(options));
  EXPECT_TRUE(received_bad_message());

  ASSERT_THAT(cookies, testing::SizeIs(0));
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlPolicy) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  // With default policy, should be able to get all cookies, even third-party.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
        GURL("http://example.com/test/"), GURL("http://notexample.com"),
        std::move(options));
    ASSERT_THAT(cookies, testing::SizeIs(1));
    EXPECT_EQ("cookie-name", cookies[0].Name());
    EXPECT_EQ("cookie-value", cookies[0].Value());
  }

  // Disabing getting third-party cookies works correctly.
  cookie_settings_.set_block_third_party_cookies(true);
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
        GURL("http://example.com/test/"), GURL("http://notexample.com"),
        std::move(options));
    ASSERT_THAT(cookies, testing::SizeIs(0));
  }

  ASSERT_EQ(2u, recorded_activity().size());
  EXPECT_EQ(recorded_activity()[0].get, true);
  EXPECT_EQ(recorded_activity()[0].url, "http://example.com/test/");
  EXPECT_EQ(recorded_activity()[0].site_for_cookies, "http://notexample.com/");
  EXPECT_THAT(recorded_activity()[0].cookie,
              net::MatchesCookieLine("cookie-name=cookie-value"));
  EXPECT_EQ(recorded_activity()[0].status,
            net::CanonicalCookie::CookieInclusionStatus::INCLUDE);

  EXPECT_EQ(recorded_activity()[1].get, true);
  EXPECT_EQ(recorded_activity()[1].url, "http://example.com/test/");
  EXPECT_EQ(recorded_activity()[1].site_for_cookies, "http://notexample.com/");
  EXPECT_THAT(recorded_activity()[1].cookie,
              net::MatchesCookieLine("cookie-name=cookie-value"));
  EXPECT_EQ(
      recorded_activity()[1].status,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
}

TEST_F(RestrictedCookieManagerTest, SetCanonicalCookie) {
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      net::CanonicalCookie(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      GURL("http://example.com/test/"), GURL("http://example.com")));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(1));

  EXPECT_EQ("new-name", cookies[0].Name());
  EXPECT_EQ("new-value", cookies[0].Value());
}

TEST_F(RestrictedCookieManagerTest, SetCanonicalCookieFromWrongOrigin) {
  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      net::CanonicalCookie(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      GURL("http://not-example.com/test/"), GURL("http://not-example.com")));
  ASSERT_TRUE(received_bad_message());
}

TEST_F(RestrictedCookieManagerTest, SetCanonicalCookiePolicy) {
  {
    // With default settings object, setting a third-party cookie is OK.
    auto cookie =
        net::CanonicalCookie::Create(GURL("http://example.com"), "A=B",
                                     base::Time::Now(), net::CookieOptions());
    EXPECT_TRUE(sync_service_->SetCanonicalCookie(
        *cookie, GURL("http://example.com"), GURL("http://notexample.com")));
  }

  {
    // Not if third-party cookies are disabled, though.
    cookie_settings_.set_block_third_party_cookies(true);
    auto cookie =
        net::CanonicalCookie::Create(GURL("http://example.com"), "A2=B2",
                                     base::Time::Now(), net::CookieOptions());
    EXPECT_FALSE(sync_service_->SetCanonicalCookie(
        *cookie, GURL("http://example.com"), GURL("http://otherexample.com")));
  }

  // Read back, in first-party context
  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "A";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;

  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com/"),
      std::move(options));
  ASSERT_THAT(cookies, testing::SizeIs(1));
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("B", cookies[0].Value());

  ASSERT_EQ(3u, recorded_activity().size());
  EXPECT_EQ(recorded_activity()[0].get, false);
  EXPECT_EQ(recorded_activity()[0].url, "http://example.com/");
  EXPECT_EQ(recorded_activity()[0].site_for_cookies, "http://notexample.com/");
  EXPECT_THAT(recorded_activity()[0].cookie, net::MatchesCookieLine("A=B"));
  EXPECT_EQ(recorded_activity()[0].status,
            net::CanonicalCookie::CookieInclusionStatus::INCLUDE);

  EXPECT_EQ(recorded_activity()[1].get, false);
  EXPECT_EQ(recorded_activity()[1].url, "http://example.com/");
  EXPECT_EQ(recorded_activity()[1].site_for_cookies,
            "http://otherexample.com/");
  EXPECT_THAT(recorded_activity()[1].cookie, net::MatchesCookieLine("A2=B2"));
  EXPECT_EQ(
      recorded_activity()[1].status,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);

  EXPECT_EQ(recorded_activity()[2].get, true);
  EXPECT_EQ(recorded_activity()[2].url, "http://example.com/test/");
  EXPECT_EQ(recorded_activity()[2].site_for_cookies, "http://example.com/");
  EXPECT_THAT(recorded_activity()[2].cookie, net::MatchesCookieLine("A=B"));
  EXPECT_EQ(recorded_activity()[2].status,
            net::CanonicalCookie::CookieInclusionStatus::INCLUDE);
}

namespace {

// Stashes the cookie changes it receives, for testing.
class TestCookieChangeListener : public network::mojom::CookieChangeListener {
 public:
  // Records a cookie change received from RestrictedCookieManager.
  struct Change {
    Change(const net::CanonicalCookie& cookie,
           network::mojom::CookieChangeCause change_cause)
        : cookie(cookie), change_cause(change_cause) {}

    net::CanonicalCookie cookie;
    network::mojom::CookieChangeCause change_cause;
  };

  TestCookieChangeListener(network::mojom::CookieChangeListenerRequest request)
      : binding_(this, std::move(request)) {}
  ~TestCookieChangeListener() override = default;

  // Spin in a run loop until a change is received.
  void WaitForChange() {
    base::RunLoop loop;
    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

  // Changes received by this listener.
  const std::vector<Change>& observed_changes() const {
    return observed_changes_;
  }

  // network::mojom::CookieChangeListener
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      network::mojom::CookieChangeCause change_cause) override {
    observed_changes_.emplace_back(cookie, change_cause);

    if (run_loop_)  // Set in WaitForChange().
      run_loop_->Quit();
  }

 private:
  std::vector<Change> observed_changes_;
  mojo::Binding<network::mojom::CookieChangeListener> binding_;

  // If not null, will be stopped when a cookie change notification is received.
  base::RunLoop* run_loop_ = nullptr;
};

}  // anonymous namespace

TEST_F(RestrictedCookieManagerTest, ChangeDispatch) {
  network::mojom::CookieChangeListenerPtr listener_ptr;
  network::mojom::CookieChangeListenerRequest request =
      mojo::MakeRequest(&listener_ptr);
  sync_service_->AddChangeListener(GURL("http://example.com/test/"),
                                   GURL("http://example.com"),
                                   std::move(listener_ptr));
  TestCookieChangeListener listener(std::move(request));

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));

  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  listener.WaitForChange();

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(network::mojom::CookieChangeCause::INSERTED,
            listener.observed_changes()[0].change_cause);
  EXPECT_EQ("cookie-name", listener.observed_changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value", listener.observed_changes()[0].cookie.Value());
}

TEST_F(RestrictedCookieManagerTest, ChangeSettings) {
  network::mojom::CookieChangeListenerPtr listener_ptr;
  network::mojom::CookieChangeListenerRequest request =
      mojo::MakeRequest(&listener_ptr);
  sync_service_->AddChangeListener(GURL("http://example.com/test/"),
                                   GURL("http://notexample.com"),
                                   std::move(listener_ptr));
  TestCookieChangeListener listener(std::move(request));

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));

  cookie_settings_.set_block_third_party_cookies(true);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));
}

TEST_F(RestrictedCookieManagerTest, AddChangeListenerFromWrongOrigin) {
  network::mojom::CookieChangeListenerPtr bad_listener_ptr;
  network::mojom::CookieChangeListenerRequest bad_request =
      mojo::MakeRequest(&bad_listener_ptr);
  ExpectBadMessage();
  sync_service_->AddChangeListener(GURL("http://not-example.com/test/"),
                                   GURL("http://not-example.com"),
                                   std::move(bad_listener_ptr));
  EXPECT_TRUE(received_bad_message());
  TestCookieChangeListener bad_listener(std::move(bad_request));

  network::mojom::CookieChangeListenerPtr good_listener_ptr;
  network::mojom::CookieChangeListenerRequest good_request =
      mojo::MakeRequest(&good_listener_ptr);
  sync_service_->AddChangeListener(GURL("http://example.com/test/"),
                                   GURL("http://example.com"),
                                   std::move(good_listener_ptr));
  TestCookieChangeListener good_listener(std::move(good_request));

  ASSERT_THAT(bad_listener.observed_changes(), testing::SizeIs(0));
  ASSERT_THAT(good_listener.observed_changes(), testing::SizeIs(0));

  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  good_listener.WaitForChange();

  EXPECT_THAT(bad_listener.observed_changes(), testing::SizeIs(0));

  ASSERT_THAT(good_listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(network::mojom::CookieChangeCause::INSERTED,
            good_listener.observed_changes()[0].change_cause);
  EXPECT_EQ("cookie-name", good_listener.observed_changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value", good_listener.observed_changes()[0].cookie.Value());
}

}  // namespace network
