// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_service.h"

#include <utility>

#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/feed/core/v2/feed_network_impl.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace feed {
namespace {

class EulaObserver : public web_resource::EulaAcceptedNotifier::Observer {
 public:
  explicit EulaObserver(FeedStream* feed_stream) : feed_stream_(feed_stream) {}
  EulaObserver(EulaObserver&) = delete;
  EulaObserver& operator=(const EulaObserver&) = delete;

  // web_resource::EulaAcceptedNotifier::Observer.
  void OnEulaAccepted() override { feed_stream_->OnEulaAccepted(); }

 private:
  FeedStream* feed_stream_;
};

}  // namespace

namespace internal {
bool ShouldClearFeed(const history::DeletionInfo& deletion_info) {
  // We ignore expirations since they're not user-initiated.
  if (deletion_info.is_from_expiration())
    return false;

  // If a user deletes a single URL, we don't consider this a clear user
  // intent to clear our data.
  return deletion_info.IsAllHistory() ||
         deletion_info.deleted_rows().size() > 1;
}
}  // namespace internal

class FeedService::HistoryObserverImpl
    : public history::HistoryServiceObserver {
 public:
  HistoryObserverImpl(history::HistoryService* history_service,
                      FeedStream* feed_stream)
      : feed_stream_(feed_stream) {
    history_service->AddObserver(this);
  }
  HistoryObserverImpl(const HistoryObserverImpl&) = delete;
  HistoryObserverImpl& operator=(const HistoryObserverImpl&) = delete;

  // history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override {
    if (internal::ShouldClearFeed(deletion_info))
      feed_stream_->OnHistoryDeleted();
  }

 private:
  FeedStream* feed_stream_;
};

class FeedService::NetworkDelegateImpl : public FeedNetworkImpl::Delegate {
 public:
  explicit NetworkDelegateImpl(FeedService::Delegate* service_delegate)
      : service_delegate_(service_delegate) {}
  NetworkDelegateImpl(const NetworkDelegateImpl&) = delete;
  NetworkDelegateImpl& operator=(const NetworkDelegateImpl&) = delete;

  // FeedNetworkImpl::Delegate.
  std::string GetLanguageTag() override {
    return service_delegate_->GetLanguageTag();
  }

 private:
  FeedService::Delegate* service_delegate_;
};

class FeedService::StreamDelegateImpl : public FeedStream::Delegate {
 public:
  StreamDelegateImpl(PrefService* local_state,
                     FeedService::Delegate* service_delegate)
      : service_delegate_(service_delegate), eula_notifier_(local_state) {}
  StreamDelegateImpl(const StreamDelegateImpl&) = delete;
  StreamDelegateImpl& operator=(const StreamDelegateImpl&) = delete;

  void Initialize(FeedStream* feed_stream) {
    eula_observer_ = std::make_unique<EulaObserver>(feed_stream);
    eula_notifier_.Init(eula_observer_.get());
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override { return eula_notifier_.IsEulaAccepted(); }
  bool IsOffline() override { return net::NetworkChangeNotifier::IsOffline(); }
  DisplayMetrics GetDisplayMetrics() override {
    return service_delegate_->GetDisplayMetrics();
  }
  std::string GetLanguageTag() override {
    return service_delegate_->GetLanguageTag();
  }

 private:
  FeedService::Delegate* service_delegate_;
  web_resource::EulaAcceptedNotifier eula_notifier_;
  std::unique_ptr<EulaObserver> eula_observer_;
  std::unique_ptr<HistoryObserverImpl> history_observer_;
};

FeedService::FeedService(std::unique_ptr<FeedStream> stream)
    : stream_(std::move(stream)) {}

FeedService::FeedService(
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
    const ChromeInfo& chrome_info)
    : delegate_(std::move(delegate)),
      refresh_task_scheduler_(std::move(refresh_task_scheduler)) {
  stream_delegate_ =
      std::make_unique<StreamDelegateImpl>(local_state, delegate.get());
  network_delegate_ = std::make_unique<NetworkDelegateImpl>(delegate_.get());
  metrics_reporter_ =
      std::make_unique<MetricsReporter>(base::DefaultTickClock::GetInstance());
  feed_network_ = std::make_unique<FeedNetworkImpl>(
      network_delegate_.get(), identity_manager, api_key, url_loader_factory,
      base::DefaultTickClock::GetInstance(), profile_prefs);
  store_ = std::make_unique<FeedStore>(std::move(database));

  stream_ = std::make_unique<FeedStream>(
      refresh_task_scheduler_.get(), metrics_reporter_.get(),
      stream_delegate_.get(), profile_prefs, feed_network_.get(), store_.get(),
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      chrome_info);

  history_observer_ = std::make_unique<HistoryObserverImpl>(
      history_service, static_cast<FeedStream*>(stream_.get()));
  stream_delegate_->Initialize(static_cast<FeedStream*>(stream_.get()));

  // TODO(harringtond): Call FeedStream::OnSignedIn()
  // TODO(harringtond): Call FeedStream::OnSignedOut()
  // TODO(harringtond): Call FeedStream::OnEnterForeground()
}

FeedService::~FeedService() = default;

FeedStreamApi* FeedService::GetStream() {
  return stream_.get();
}

void FeedService::ClearCachedData() {
  stream_->OnCacheDataCleared();
}

}  // namespace feed
