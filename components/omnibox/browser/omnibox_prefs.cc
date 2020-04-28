// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"

#include "base/check.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace omnibox {

const char kToggleSuggestionGroupIdOffHistogram[] =
    "Omnibox.ToggleSuggestionGroupId.Off";
const char kToggleSuggestionGroupIdOnHistogram[] =
    "Omnibox.ToggleSuggestionGroupId.On";

// A client-side toggle for document (Drive) suggestions.
// Also gated by a feature and server-side Admin Panel controls.
const char kDocumentSuggestEnabled[] = "documentsuggest.enabled";

// A list of suggestion group IDs for zero suggest that are not allowed to
// appear in the results.
const char kOmniboxHiddenGroupIds[] = "omnibox.hiddenGroupIds";

// Boolean that specifies whether to always show full URLs in the omnibox.
const char kPreventUrlElisionsInOmnibox[] = "omnibox.prevent_url_elisions";

// A cache of zero suggest results using JSON serialized into a string.
const char kZeroSuggestCachedResults[] = "zerosuggest.cachedresults";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(omnibox::kOmniboxHiddenGroupIds,
                             base::Value(base::Value::Type::LIST));
}

bool IsSuggestionGroupIdHidden(PrefService* prefs, int suggestion_group_id) {
  DCHECK(prefs);
  const base::ListValue* group_id_values =
      prefs->GetList(kOmniboxHiddenGroupIds);
  return std::find(group_id_values->begin(), group_id_values->end(),
                   base::Value(suggestion_group_id)) != group_id_values->end();
}

void ToggleSuggestionGroupIdVisibility(PrefService* prefs,
                                       int suggestion_group_id) {
  DCHECK(prefs);
  ListPrefUpdate update(prefs, kOmniboxHiddenGroupIds);
  const bool is_hidden = IsSuggestionGroupIdHidden(prefs, suggestion_group_id);
  if (is_hidden) {
    update->EraseListValue(base::Value(suggestion_group_id));
  } else {
    update->Append(suggestion_group_id);
  }
  base::SparseHistogram::FactoryGet(
      is_hidden ? kToggleSuggestionGroupIdOnHistogram
                : kToggleSuggestionGroupIdOffHistogram,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(suggestion_group_id);
}

}  // namespace omnibox
