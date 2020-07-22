// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_

#include <vector>

class PrefRegistrySimple;
class PrefService;

namespace omnibox {

// These values are persisted to prefs. They cannot be freely changed.
enum SuggestionGroupVisibility {
  // When DEFAULT is returned, the group's visibility should be controlled by
  // the server-provided hint.
  DEFAULT = 0,

  // HIDDEN means the user has manually hidden the group before, and so this
  // group should be hidden regardless of the server-provided hint.
  HIDDEN = 1,

  // SHOWN means the user has manually shown the group before, and so this
  // group should be shown regardless of the server-provided hint.
  SHOWN = 2,
};

// Histograms being recorded when visibility of suggestion group IDs change.
extern const char kToggleSuggestionGroupIdOffHistogram[];
extern const char kToggleSuggestionGroupIdOnHistogram[];

// Alphabetical list of preference names specific to the omnibox component.
// Keep alphabetized, and document each in the .cc file.
extern const char kDocumentSuggestEnabled[];
extern const char kSuggestionGroupVisibility[];
extern const char kPreventUrlElisionsInOmnibox[];
extern const char kZeroSuggestCachedResults[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns whether the given suggestion group ID is allowed to appear in the
// results.
bool IsSuggestionGroupIdHidden(PrefService* prefs, int suggestion_group_id);

// Sets the group visibility of |suggestion_group_id| to |new_value|.
void SetSuggestionGroupVisibility(PrefService* prefs,
                                  int suggestion_group_id,
                                  SuggestionGroupVisibility new_value);

// Allows suggestions with the given suggestion group ID to appear in the
// results if they currently are not allowed to or prevents them from
// appearing in the results if they are currently permitted to.
//
// DEPRECATED: Use SetSuggestionGroupVisibility instead.
void ToggleSuggestionGroupIdVisibility(PrefService* prefs,
                                       int suggestion_group_id);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
