// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

namespace sharesheet {

SharesheetService::SharesheetService(Profile* profile)
    : sharesheet_action_cache_(std::make_unique<SharesheetActionCache>()) {}

SharesheetService::~SharesheetService() = default;

}  // namespace sharesheet
