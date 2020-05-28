// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_schedule.h"

#include "base/check.h"

namespace download {

DownloadSchedule::DownloadSchedule(bool only_on_wifi)
    : only_on_wifi_(only_on_wifi) {}

DownloadSchedule::DownloadSchedule(base::Optional<base::Time> start_time)
    : only_on_wifi_(false), start_time_(start_time) {}

DownloadSchedule::~DownloadSchedule() = default;

}  // namespace download
