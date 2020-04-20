// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/switches.h"

namespace switches {

const base::Feature kSyncDoNotCommitBookmarksWithoutFavicon = {
    "SyncDoNotCommitBookmarksWithoutFavicon",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables updating a BookmarkNode's GUID by replacing the node itself.
const base::Feature kUpdateBookmarkGUIDWithNodeReplacement{
    "UpdateGUIDWithNodeReplacement", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the GUID-aware merge algorithm.
const base::Feature kMergeBookmarksUsingGUIDs{
    "MergeBookmarksUsingGUIDs", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyncReuploadBookmarkFullTitles{
    "SyncReuploadBookmarkFullTitles", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyncProcessBookmarkRestoreAfterDeletion{
    "SyncProcessBookmarkRestoreAfterDeletion",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches
