// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_OMNIBOX_FOCUS_TYPE_H_
#define COMPONENTS_SEARCH_ENGINES_OMNIBOX_FOCUS_TYPE_H_

// For search requests, this enum specifies how the user last interacted with
// the UI control. This is used for both the omnibox and NTP realbox.
// It's called OmniboxFocusType so this enum matches the "oft" GET param.
//
// These values are used as HTTP GET parameter values. Entries should not be
// renumbered and numeric values should never be reused.
enum class OmniboxFocusType {
  // The default value. This is used for any search requests without any
  // special interaction annotation, including: normal omnibox searches,
  // as-you-type omnibox suggestions, as well as non-omnibox searches.
  DEFAULT = 0,

  // This search request is triggered by the user focusing the omnibox.
  ON_FOCUS = 1,

  // This search request is triggered by the user deleting all of the
  // omnibox permanent text at once, i.e. user is on "https://example.com",
  // does Ctrl+L which selects the whole URL, then presses Backspace.
  //
  // This value does not apply in these circumstances:
  //  - User deletes their own typed text.
  //  - User deletes the permanent text one character at a time.
  //  - User uses Cut (Ctrl+X) to delete the permanent text.
  DELETED_PERMANENT_TEXT = 2,
};

#endif  // COMPONENTS_SEARCH_ENGINES_OMNIBOX_FOCUS_TYPE_H_
