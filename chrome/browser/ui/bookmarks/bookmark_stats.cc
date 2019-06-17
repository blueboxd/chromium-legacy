// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_stats.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkNode;

namespace {

void RecordNodeDepth(const BookmarkNode* node) {
  if (!node)
    return;

  // In the cases where a bookmark node is provided, record the depth of the
  // bookmark in the tree.
  int depth = 0;
  for (const BookmarkNode* iter = node; iter != NULL; iter = iter->parent()) {
    depth++;
  }
  // Record |depth - 2| to offset the invisible root node and permanent nodes
  // (Bookmark Bar, Mobile Bookmarks or Other Bookmarks)
  UMA_HISTOGRAM_COUNTS_1M("Bookmarks.LaunchDepth", depth - 2);
}

bool IsBookmarkBarLocation(BookmarkLaunchLocation location) {
  return location == BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR ||
         location == BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR ||
         location == BOOKMARK_LAUNCH_LOCATION_BAR_SUBFOLDER;
}

}  // namespace

void RecordBookmarkLaunch(const BookmarkNode* node,
                          BookmarkLaunchLocation location,
                          ProfileMetrics::BrowserProfileType profile_type) {
  if (IsBookmarkBarLocation(location)) {
    base::RecordAction(base::UserMetricsAction("ClickedBookmarkBarURLButton"));
  } else if (location == BOOKMARK_LAUNCH_LOCATION_APP_MENU) {
    base::RecordAction(
        base::UserMetricsAction("WrenchMenu_Bookmarks_LaunchURL"));
  } else if (location == BOOKMARK_LAUNCH_LOCATION_TOP_MENU) {
    base::RecordAction(base::UserMetricsAction("TopMenu_Bookmarks_LaunchURL"));
  }

  UMA_HISTOGRAM_ENUMERATION("Bookmarks.LaunchLocation", location,
                            BOOKMARK_LAUNCH_LOCATION_LIMIT);

  UMA_HISTOGRAM_ENUMERATION("Bookmarks.UsageCountPerProfileType", profile_type);
  RecordNodeDepth(node);
}

void RecordBookmarkFolderLaunch(const BookmarkNode* node,
                                BookmarkLaunchLocation location) {
  if (IsBookmarkBarLocation(location))
    base::RecordAction(
        base::UserMetricsAction("MiddleClickedBookmarkBarFolder"));

  RecordNodeDepth(node);
}

void RecordBookmarkFolderOpen(BookmarkLaunchLocation location) {
  if (IsBookmarkBarLocation(location))
    base::RecordAction(base::UserMetricsAction("ClickedBookmarkBarFolder"));
}

void RecordBookmarkAppsPageOpen(BookmarkLaunchLocation location) {
  if (IsBookmarkBarLocation(location)) {
    base::RecordAction(
        base::UserMetricsAction("ClickedBookmarkBarAppsShortcutButton"));
  }
}
