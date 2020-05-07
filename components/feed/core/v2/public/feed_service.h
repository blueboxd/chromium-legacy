// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/public/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/web_resource/eula_accepted_notifier.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base
namespace history {
class HistoryService;
class DeletionInfo;
}  // namespace history
namespace feedstore {
class Record;
}  // namespace feedstore
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace signin {
class IdentityManager;
}  // namespace signin

namespace feed {
class RefreshTaskScheduler;
class MetricsReporter;
class FeedNetwork;
class FeedStore;
class FeedStream;

namespace internal {
bool ShouldClearFeed(const history::DeletionInfo& deletion_info);
}  // namespace internal

class FeedService : public KeyedService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns a string which represents the top locale and region of the
    // device.
    virtual std::string GetLanguageTag() = 0;
    // Returns display metrics for the device.
    virtual DisplayMetrics GetDisplayMetrics() = 0;
  };

  // Construct a FeedService given an already constructed FeedStream.
  // Used for testing only.
  explicit FeedService(std::unique_ptr<FeedStream> stream);

  // Construct a new FeedStreamApi along with FeedService.
  FeedService(
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<RefreshTaskScheduler> refresh_task_scheduler,
      PrefService* profile_prefs,
      PrefService* local_state,
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database,
      signin::IdentityManager* identity_manager,
      history::HistoryService* history_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::string& api_key,
      const ChromeInfo& chrome_info);
  ~FeedService() override;
  FeedService(const FeedService&) = delete;
  FeedService& operator=(const FeedService&) = delete;

  FeedStreamApi* GetStream();

  void ClearCachedData();

 private:
  class StreamDelegateImpl;
  class NetworkDelegateImpl;
  class HistoryObserverImpl;

  // These components are owned for construction of |FeedStreamApi|. These will
  // be null if |FeedStreamApi| is created externally.
  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<StreamDelegateImpl> stream_delegate_;
  std::unique_ptr<MetricsReporter> metrics_reporter_;
  std::unique_ptr<NetworkDelegateImpl> network_delegate_;
  std::unique_ptr<FeedNetwork> feed_network_;
  std::unique_ptr<FeedStore> store_;
  std::unique_ptr<RefreshTaskScheduler> refresh_task_scheduler_;
  std::unique_ptr<HistoryObserverImpl> history_observer_;

  std::unique_ptr<FeedStream> stream_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_
