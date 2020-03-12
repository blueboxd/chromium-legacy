// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_storage.h"

#include "base/no_destructor.h"
#include "chromecast/crash/fuchsia/cast_crash_storage_impl_fuchsia.h"

namespace chromecast {

CastCrashStorage* CastCrashStorage::GetInstance() {
  static base::NoDestructor<CastCrashStorageImplFuchsia> storage;
  return storage.get();
}

}  // namespace chromecast
