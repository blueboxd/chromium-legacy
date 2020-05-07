// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/android/feed/v2/feed_service_bridge.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_version.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"

namespace feed {
namespace {
const char kFeedv2Folder[] = "feedv2";

class FeedServiceDelegateImpl : public FeedService::Delegate {
 public:
  ~FeedServiceDelegateImpl() override = default;
  std::string GetLanguageTag() override {
    return FeedServiceBridge::GetLanguageTag();
  }
  DisplayMetrics GetDisplayMetrics() override {
    return FeedServiceBridge::GetDisplayMetrics();
  }
};

}  // namespace

// static
FeedService* FeedServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FeedService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
FeedServiceFactory* FeedServiceFactory::GetInstance() {
  return base::Singleton<FeedServiceFactory>::get();
}

FeedServiceFactory::FeedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeedService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

FeedServiceFactory::~FeedServiceFactory() = default;

KeyedService* FeedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(context);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  base::FilePath feed_dir(profile->GetPath().Append(kFeedv2Folder));

  feed::ChromeInfo chrome_info;
  chrome_info.version = base::Version({CHROME_VERSION});
  chrome_info.channel = chrome::GetChannel();

  return new FeedService(
      std::make_unique<FeedServiceDelegateImpl>(),
      std::unique_ptr<RefreshTaskScheduler>(),  // TODO(harringtond): implement
                                                // one of these.
      profile->GetPrefs(), g_browser_process->local_state(),
      storage_partition->GetProtoDatabaseProvider()->GetDB<feedstore::Record>(
          leveldb_proto::ProtoDbType::FEED_STREAM_DATABASE,
          feed_dir.AppendASCII("streamdb"), background_task_runner),
      identity_manager,
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS),
      storage_partition->GetURLLoaderFactoryForBrowserProcess(),
      background_task_runner, api_key, chrome_info);
}

content::BrowserContext* FeedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context->IsOffTheRecord() ? nullptr : context;
}

bool FeedServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feed
