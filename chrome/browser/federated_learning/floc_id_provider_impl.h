// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/sync/driver/sync_service_observer.h"

namespace content_settings {
class CookieSettings;
}

namespace syncer {
class UserEventService;
}

namespace federated_learning {

class FlocRemotePermissionService;

// A service that regularly computes the floc id and logs it in a user event.
//
// A floc session starts when sync & sync-history is first enabled. We validate
// the following conditions in sequence before computing the floc id: user
// doesn’t block 3rd party cookies; all 3 of swaa & nac & account_type are
// enabled; the user has visited at least 1 domain with publicly routable ip in
// the past 7 days. Every 24 hours, it'll go over the conditions above again
// (including the sync & sync-history check), and reset or recompute the id
// accordingly.
class FlocIdProviderImpl : public FlocIdProvider,
                           public syncer::SyncServiceObserver {
 public:
  using CanComputeFlocIdCallback = base::OnceCallback<void(bool)>;
  using GetRecentlyVisitedURLsCallback =
      history::HistoryService::QueryHistoryCallback;

  enum class EventLoggingAction {
    kAllow,
    kDisallow,
  };

  FlocIdProviderImpl(
      syncer::SyncService* sync_service,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      FlocRemotePermissionService* floc_remote_permission_service,
      history::HistoryService* history_service,
      syncer::UserEventService* user_event_service);
  ~FlocIdProviderImpl() override;
  FlocIdProviderImpl(const FlocIdProviderImpl&) = delete;
  FlocIdProviderImpl& operator=(const FlocIdProviderImpl&) = delete;

 protected:
  // protected virtual for testing.
  virtual void NotifyFlocIdUpdated(EventLoggingAction);
  virtual bool IsSyncHistoryEnabled();
  virtual bool AreThirdPartyCookiesAllowed();
  virtual void IsSwaaNacAccountEnabled(CanComputeFlocIdCallback callback);

 private:
  friend class FlocIdProviderUnitTest;
  friend class FlocIdProviderBrowserTest;

  // KeyedService:
  void Shutdown() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;

  void CalculateFloc();

  void CheckCanComputeFlocId(CanComputeFlocIdCallback callback);
  void OnCheckCanComputeFlocIdCompleted(bool can_compute_floc);

  void GetRecentlyVisitedURLs(GetRecentlyVisitedURLsCallback callback);
  void OnGetRecentlyVisitedURLsCompleted(size_t floc_session_count,
                                         history::QueryResults results);

  FlocId floc_id_;
  size_t floc_session_count_ = 0;

  syncer::SyncService* sync_service_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  FlocRemotePermissionService* floc_remote_permission_service_;
  history::HistoryService* history_service_;
  syncer::UserEventService* user_event_service_;

  // Used for the async tasks querying the HistoryService.
  base::CancelableTaskTracker history_task_tracker_;

  // The timer used to start the floc session at regular intervals.
  base::RepeatingTimer floc_session_start_timer_;

  base::WeakPtrFactory<FlocIdProviderImpl> weak_ptr_factory_{this};
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_IMPL_H_
