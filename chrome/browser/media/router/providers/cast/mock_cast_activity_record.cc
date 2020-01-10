// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mock_cast_activity_record.h"

namespace media_router {

MockCastActivityRecord::MockCastActivityRecord(const MediaRoute& route,
                                               const std::string& app_id)
    : CastActivityRecord(route, app_id, nullptr, nullptr) {}

MockCastActivityRecord::~MockCastActivityRecord() = default;

}  // namespace media_router
