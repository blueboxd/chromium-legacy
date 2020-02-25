// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_constants.h"

namespace sessions {

const base::FilePath::StringPieceType kCurrentTabSessionFileName =
    FILE_PATH_LITERAL("Current Tabs");
const base::FilePath::StringPieceType kLastTabSessionFileName =
    FILE_PATH_LITERAL("Last Tabs");

const base::FilePath::StringPieceType kCurrentSessionFileName =
    FILE_PATH_LITERAL("Current Session");
const base::FilePath::StringPieceType kLastSessionFileName =
    FILE_PATH_LITERAL("Last Session");

const int gMaxPersistNavigationCount = 6;

}  // namespace sessions
