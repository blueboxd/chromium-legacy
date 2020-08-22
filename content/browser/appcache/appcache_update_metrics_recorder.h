// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_METRICS_RECORDER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_METRICS_RECORDER_H_

#include "base/check_op.h"
#include "content/browser/appcache/appcache_update_job_state.h"
#include "content/common/content_export.h"

namespace content {

// Records UMA metrics for AppCache update jobs.
//
// Update jobs accumulate metric data in an instance of this class calling the
// Record*() methods. When the update job is finished (either by completing
// normally, failing due to error, or being canceled), the instance is deleted.
// The AppCacheUpdateJob destructor then logs the accumulated metrics by calling
// UploadMetrics().
class CONTENT_EXPORT AppCacheUpdateMetricsRecorder {
 public:
  AppCacheUpdateMetricsRecorder();
  ~AppCacheUpdateMetricsRecorder() = default;

  // IncrementExistingCorruptionFixedInUpdate() keeps track of the number of
  // corrupt resources that we've fixed while handling a 304 response.
  void IncrementExistingCorruptionFixedInUpdate();

  // IncrementExistingResourceCheck() keeps track of the number of times
  // we plan to check whether we can reuse existing resources.
  void IncrementExistingResourceCheck();

  // IncrementExistingResourceCorrupt() keeps track of the number of corrupt
  // resources that we've encountered.  Corrupt cache entries that are present
  // and haven't been read or haven't been checked to see if they can be used
  // will not be detected/reported through this metric.
  void IncrementExistingResourceCorrupt();

  // IncrementExistingResourceCorruptionRecovery() keeps track of the number of
  // times we detected a corrupt resource and triggered corruption recovery.
  // This will only occur when the corruption recovery feature is enabled in
  // a given Chromium instance.
  void IncrementExistingResourceCorruptionRecovery();

  // IncrementExistingResourceNotCorrupt() keeps track of the number of non-
  // corrupt resources that we've encountered.  Non-corrupt cache entries that
  // are present and haven't been read or haven't been checked to see if they
  // can be used will not be detected/reported through this metric.
  void IncrementExistingResourceNotCorrupt();

  // IncrementExistingResourceReused() keeps track of the number of times
  // we've determined we can reuse an existing resource.
  void IncrementExistingResourceReused();

  // RecordCanceled() logs whether the update job was canceled somehow.
  void RecordCanceled();

  // RecordFinalInternalState() logs the final state the update job ended in.
  // This is logged after the destructor is called and before the destructor
  // cancels any pending work.  As a result, if any part of the state machine
  // triggers cancel before calling deleting the update job, the state may
  // be CANCELED, while in other cases, the state may not be COMPLETED and would
  // later in the destructor have been marked CANCELED.
  void RecordFinalInternalState(AppCacheUpdateJobState);

  // Called after the update job has completed and is being destructed.
  //
  // Must be called exactly once. No other Record*() method may be called after
  // this method is called.
  void UploadMetrics();

 private:
  int existing_corruption_fixed_in_update_ = 0;
  int existing_resource_check_ = 0;
  int existing_resource_corrupt_ = 0;
  int existing_resource_corruption_recovery_ = 0;
  int existing_resource_not_corrupt_ = 0;
  int existing_resource_reused_ = 0;
  bool canceled_ = false;
  AppCacheUpdateJobState final_internal_state_;

#if DCHECK_IS_ON()
  // True after UploadMetrics() was called.
  bool finalized_ = false;
#endif  // DCHECK_IS_ON()
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_METRICS_RECORDER_H_
