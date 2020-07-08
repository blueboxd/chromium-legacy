// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_

#include <memory>
#include <string>

#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/query_tiles/internal/log_source.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/tile_service_prefs.h"
#include "net/base/backoff_entry_serializer.h"

class PrefService;

namespace query_tiles {

// Coordinates with native background task scheduler to schedule or cancel a
// TileBackgroundTask.
class TileServiceScheduler {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    // Returns the tile group instance holds in memory.
    virtual TileGroup* GetTileGroup() = 0;
  };

  // Set delegate object for the scheduler.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Called when fetching task starts.
  virtual void OnFetchStarted() = 0;

  // Called on fetch task completed, schedule another task with or without
  // backoff based on the status. Success status will lead a regular schedule
  // after around 14 - 18 hours. Failure status will lead a backoff, the release
  // duration is related to count of failures. Suspend status will directly set
  // the release time until 24 hours later.
  virtual void OnFetchCompleted(TileInfoRequestStatus status) = 0;

  // Called on tile manager initialization completed, schedule another task with
  // or without backoff based on the status. NoTiles status will lead a regular
  // schedule after around 14 - 18 hours. DbOperationFailure status will
  // directly set the release time until 24 hours later.
  virtual void OnTileManagerInitialized(TileGroupStatus status) = 0;

  // Called when database is purged. Reset the flow and update the status.
  virtual void OnDbPurged(TileGroupStatus status) = 0;

  // Called when parsed group data are saved.
  virtual void OnGroupDataSaved(TileGroupStatus status) = 0;

  // Cancel current existing task, and reset scheduler.
  virtual void CancelTask() = 0;

  virtual ~TileServiceScheduler() = default;

  TileServiceScheduler(const TileServiceScheduler& other) = delete;
  TileServiceScheduler& operator=(const TileServiceScheduler& other) = delete;

 protected:
  TileServiceScheduler() = default;
};

// An implementation of TileServiceScheduler interface and LogSource interface.
class TileServiceSchedulerImpl : public TileServiceScheduler, public LogSource {
 public:
  TileServiceSchedulerImpl(
      background_task::BackgroundTaskScheduler* scheduler,
      PrefService* prefs,
      base::Clock* clock,
      const base::TickClock* tick_clock,
      std::unique_ptr<net::BackoffEntry::Policy> backoff_policy,
      LogSink* log_sink);

  ~TileServiceSchedulerImpl() override;

 private:
  // TileServiceScheduler implementation.
  void CancelTask() override;
  void OnFetchStarted() override;
  void OnFetchCompleted(TileInfoRequestStatus status) override;
  void OnTileManagerInitialized(TileGroupStatus status) override;
  void OnDbPurged(TileGroupStatus status) override;
  void OnGroupDataSaved(TileGroupStatus status) override;
  void SetDelegate(Delegate* delegate) override;

  // LogSource implementation.
  TileInfoRequestStatus GetFetcherStatus() override;
  TileGroupStatus GetGroupStatus() override;
  TileGroup* GetTileGroup() override;

  void ScheduleTask(bool is_init_schedule);
  std::unique_ptr<net::BackoffEntry> GetBackoff();
  void AddBackoff();
  void ResetBackoff();
  void MaximizeBackoff();
  int64_t GetDelaysFromBackoff();
  void GetInstantTaskWindow(int64_t* start_time_ms,
                            int64_t* end_time_ms,
                            bool is_init_schedule);
  void GetTaskWindow(int64_t* start_time_ms,
                     int64_t* end_time_ms,
                     bool is_init_schedule);
  void UpdateBackoff(net::BackoffEntry* backoff);
  void MarkFirstRunScheduled();
  void MarkFirstRunFinished();

  // Returns true if the initial task has been scheduled because no tiles in
  // db(kickoff condition), but still waiting to be completed at the designated
  // window. Returns false either the first task is not scheduled yet or it is
  // already finished.
  bool IsDuringFirstFlow();

  // Ping the log sink to update.
  void PingLogSink();

  // Native Background Scheduler instance.
  background_task::BackgroundTaskScheduler* scheduler_;

  // PrefService.
  PrefService* prefs_;

  // Clock object to get current time.
  base::Clock* clock_;

  // TickClock used for building backoff entry.
  const base::TickClock* tick_clock_;

  // Backoff policy used for reschdule.
  std::unique_ptr<net::BackoffEntry::Policy> backoff_policy_;

  // Flag to indicate if currently have a suspend status to avoid overwriting if
  // already scheduled a suspend task during this lifecycle.
  bool is_suspend_;

  // Delegate object.
  Delegate* delegate_;

  // Internal fetcher status.
  TileInfoRequestStatus fetcher_status_;

  // Internal group status.
  TileGroupStatus group_status_;

  // Log Sink object.
  LogSink* log_sink_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_
