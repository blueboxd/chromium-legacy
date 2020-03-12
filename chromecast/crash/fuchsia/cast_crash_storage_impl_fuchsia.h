// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_FUCHSIA_CAST_CRASH_STORAGE_IMPL_FUCHSIA_H_
#define CHROMECAST_CRASH_FUCHSIA_CAST_CRASH_STORAGE_IMPL_FUCHSIA_H_

#include "chromecast/crash/cast_crash_storage.h"

namespace chromecast {

class CastCrashStorageImplFuchsia : public CastCrashStorage {
 public:
  CastCrashStorageImplFuchsia();
  ~CastCrashStorageImplFuchsia() final;
  CastCrashStorageImplFuchsia& operator=(const CastCrashStorageImplFuchsia&) =
      delete;
  CastCrashStorageImplFuchsia(const CastCrashStorageImplFuchsia&) = delete;

  // CastCrashStorage implementation:
  void SetLastLaunchedApp(base::StringPiece app_id) final;
  void ClearLastLaunchedApp() final;
  void SetCurrentApp(base::StringPiece app_id) final;
  void ClearCurrentApp() final;
  void SetPreviousApp(base::StringPiece app_id) final;
  void ClearPreviousApp() final;
  void SetStadiaSessionId(base::StringPiece session_id) final;
  void ClearStadiaSessionId() final;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_FUCHSIA_CAST_CRASH_STORAGE_IMPL_FUCHSIA_H_
