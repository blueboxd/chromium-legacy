// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_CONSTANTS_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_CONSTANTS_H_

#include "base/files/file_path.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {

// File names (current and previous) for a type of TAB.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kCurrentTabSessionFileName;
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kLastTabSessionFileName;

// File names (current and previous) for a type of SESSION.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kCurrentSessionFileName;
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kLastSessionFileName;

// The maximum number of navigation entries in each direction to persist.
extern const int SESSIONS_EXPORT gMaxPersistNavigationCount;

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_CONSTANTS_H_
