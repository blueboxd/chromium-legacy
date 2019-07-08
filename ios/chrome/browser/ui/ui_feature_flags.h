// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to automatically switch to the regular tabs panel in tab grid after
// closing the last incognito tab.
extern const base::Feature kClosingLastIncognitoTab;

// Feature to retain the contentView in the browser container.
extern const base::Feature kBrowserContainerKeepsContentView;

// Feature to show most visited sites and collection shortcuts in the omnibox
// popup instead of ZeroSuggest.
extern const base::Feature kOmniboxPopupShortcutIconsInZeroState;

// Feature to take snapshots using |-drawViewHierarchy:|.
extern const base::Feature kSnapshotDrawView;

// Feature to rework handling of copied content (url/string/image) in the ui.
// This feature is used in extensions. If you modify it significantly, you may
// want to update the version in |app_group_field_trial_version|.
extern const base::Feature kCopiedContentBehavior;

// Feature to apply UI Refresh theme to the settings.
extern const base::Feature kSettingsRefresh;

// Feature to display the new omnibox popup design with favicons, search engine
// favicon in the omnibox, rich entities support, new layout.
extern const base::Feature kNewOmniboxPopupLayout;

// Feature to display the omnibox with default search engine favicon
// in the omnibox.
extern const base::Feature kOmniboxUseDefaultSearchEngineFavicon;

// Feature flag for the language settings page.
extern const base::Feature kLanguageSettings;

// Feature flag for the optional article thumbnail.
extern const base::Feature kOptionalArticleThumbnail;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
