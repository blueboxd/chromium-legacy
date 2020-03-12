// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/fuchsia/cast_crash_storage_impl_fuchsia.h"

namespace chromecast {

CastCrashStorageImplFuchsia::CastCrashStorageImplFuchsia() = default;
CastCrashStorageImplFuchsia::~CastCrashStorageImplFuchsia() = default;

void CastCrashStorageImplFuchsia::SetLastLaunchedApp(base::StringPiece app_id) {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::ClearLastLaunchedApp() {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::SetCurrentApp(base::StringPiece app_id) {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::ClearCurrentApp() {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::SetPreviousApp(base::StringPiece app_id) {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::ClearPreviousApp() {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::SetStadiaSessionId(
    base::StringPiece session_id) {
  // TODO(b/67907746): Implement.
}

void CastCrashStorageImplFuchsia::ClearStadiaSessionId() {
  // TODO(b/67907746): Implement.
}

}  // namespace chromecast
