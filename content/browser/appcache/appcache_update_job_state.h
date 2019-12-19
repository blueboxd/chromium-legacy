// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_STATE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_STATE_H_

namespace content {

// These states are logged in UMA.  Only append, do not reorder.
enum class AppCacheUpdateJobState {
  FETCH_MANIFEST = 0,
  NO_UPDATE = 1,
  DOWNLOADING = 2,

  // Every state after this comment indicates the update is terminating.
  REFETCH_MANIFEST = 3,
  CACHE_FAILURE = 4,
  CANCELLED = 5,
  COMPLETED = 6,

  kMaxValue = COMPLETED,
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_STATE_H_
