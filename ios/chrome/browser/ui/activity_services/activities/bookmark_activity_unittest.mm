// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the BookmarkActivity class.
class BookmarkActivityTest : public PlatformTest {
 protected:
  BookmarkActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_dispatcher_ = OCMProtocolMock(@protocol(BrowserCommands));

    RegisterPrefs();
  }

  void RegisterPrefs() {
    testing_pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, true);
  }

  void SetCanEditBookmarkPref(bool canEdit) {
    testing_pref_service_.SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                     canEdit);
  }

  BookmarkActivity* CreateActivity(const GURL& URL, bool bookmarked) {
    return [[BookmarkActivity alloc] initWithURL:URL
                                      bookmarked:bookmarked
                                      dispatcher:mocked_dispatcher_
                                     prefService:&testing_pref_service_];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple testing_pref_service_;
  id mocked_dispatcher_;
};

// Tests that the activity can always be performed when the kEditBookmarksIOS
// feature flag is disabled.
TEST_F(BookmarkActivityTest, FlagOff_ActivityAlwaysAvailable) {
  BookmarkActivity* activity = CreateActivity(GURL(), true);
  scoped_feature_list_.InitAndDisableFeature(kEditBookmarksIOS);

  // Flag Off, Editable bookmark pref true.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  SetCanEditBookmarkPref(false);

  // Flag off, Editable bookmark pref false.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that, when the kEditBookmarksIOS is enabled, the activity can only be
// performed if the preferences indicate that bookmarks can be edited.
TEST_F(BookmarkActivityTest, FlagOn_ActivityHiddenByPref) {
  BookmarkActivity* activity = CreateActivity(GURL(), true);
  scoped_feature_list_.InitAndEnableFeature(kEditBookmarksIOS);

  // Flag On, Editable bookmark pref true.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  SetCanEditBookmarkPref(false);

  // Flag On, Editable bookmark pref false.
  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}
