// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
class GURL;

namespace safe_search_api {
enum class Classification;
class URLChecker;
}  // namespace safe_search_api

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace media_feeds {

namespace {
class CookieChangeListener;
}  // namespace

class MediaFeedsService : public KeyedService {
 public:
  static const char kSafeSearchResultHistogramName[];

  explicit MediaFeedsService(Profile* profile);
  ~MediaFeedsService() override;
  MediaFeedsService(const MediaFeedsService& t) = delete;
  MediaFeedsService& operator=(const MediaFeedsService&) = delete;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static MediaFeedsService* Get(Profile* profile);

  // Register profile prefs in the pref registry.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Checks the list of pending items against the Safe Search API and stores
  // the result.
  void CheckItemsAgainstSafeSearch(
      media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList list);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

  // Stores a callback to be called once we have completed all inflight checks.
  void SetSafeSearchCompletionCallbackForTest(base::OnceClosure callback);

  // Fetches a media feed with the given ID and then store it in the
  // feeds table in media history. Runs the given callback after storing. The
  // fetch will be skipped if another fetch is currently ongoing.
  void FetchMediaFeed(int64_t feed_id,
                      base::OnceClosure callback);

  // Stores a callback to be called once we have completed all inflight checks.
  void SetCookieChangeCallbackForTest(base::OnceClosure callback);

  bool HasCookieObserverForTest() const;

 private:
  friend class MediaFeedsServiceTest;

  bool AddInflightSafeSearchCheck(const int64_t id, const std::set<GURL>& urls);

  void CheckForSafeSearch(const int64_t id, const GURL& url);

  void OnCheckURLDone(const int64_t id,
                      const GURL& original_url,
                      const GURL& url,
                      safe_search_api::Classification classification,
                      bool uncertain);

  void MaybeCallCompletionCallback();

  bool IsSafeSearchCheckingEnabled() const;

  void OnGotFetchDetails(
      const int64_t feed_id,
      base::Optional<
          media_history::MediaHistoryKeyedService::MediaFeedFetchDetails>
          details);

  void OnFetchResponse(int64_t feed_id,
                       base::Optional<base::UnguessableToken> reset_token,
                       const schema_org::improved::mojom::EntityPtr& response,
                       MediaFeedsFetcher::Status status,
                       bool was_fetched_via_cache);

  void OnCompleteFetch(const int64_t feed_id, const bool has_items);

  void OnSafeSearchPrefChanged();

  void OnResetOriginFromCookie(const url::Origin& origin,
                               const bool include_subdomains);

  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  scoped_refptr<::network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForFetcher();

  PrefChangeRegistrar pref_change_registrar_;

  struct InflightFeedFetch {
    InflightFeedFetch(std::unique_ptr<MediaFeedsFetcher> fetcher,
                      base::OnceClosure callback);
    ~InflightFeedFetch();
    InflightFeedFetch(InflightFeedFetch&& t);
    InflightFeedFetch(const InflightFeedFetch&) = delete;
    InflightFeedFetch& operator=(const InflightFeedFetch&) = delete;

    std::vector<base::OnceClosure> callbacks;

    std::unique_ptr<MediaFeedsFetcher> fetcher;
  };

  std::map<int64_t, InflightFeedFetch> fetches_;

  struct InflightSafeSearchCheck {
    explicit InflightSafeSearchCheck(const std::set<GURL>& urls);
    ~InflightSafeSearchCheck();
    InflightSafeSearchCheck(const InflightSafeSearchCheck&) = delete;
    InflightSafeSearchCheck& operator=(const InflightSafeSearchCheck&) = delete;

    std::set<GURL> pending;

    bool is_safe = false;
    bool is_unsafe = false;
    bool is_uncertain = false;
  };

  std::map<int64_t, std::unique_ptr<InflightSafeSearchCheck>>
      inflight_safe_search_checks_;

  base::OnceClosure safe_search_completion_callback_;

  scoped_refptr<::network::SharedURLLoaderFactory>
      test_url_loader_factory_for_fetcher_;

  std::unique_ptr<CookieChangeListener> cookie_change_listener_;

  base::OnceClosure cookie_change_callback_;

  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;
  Profile* const profile_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<MediaFeedsService> weak_factory_{this};
};

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
