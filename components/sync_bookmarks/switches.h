// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

extern const base::Feature kSyncDoNotCommitBookmarksWithoutFavicon;
extern const base::Feature kUpdateBookmarkGUIDWithNodeReplacement;
extern const base::Feature kMergeBookmarksUsingGUIDs;
// TODO(crbug.com/1066962): remove this code when most of bookmarks are
// reuploaded.
extern const base::Feature kSyncReuploadBookmarkFullTitles;
// TODO(crbug.com/1071061): remove after launching.
extern const base::Feature kSyncProcessBookmarkRestoreAfterDeletion;

}  // namespace switches

#endif  // COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
