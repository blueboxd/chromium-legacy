// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_

#include "base/feature_list.h"

namespace omnibox {

// Please do not add more features to this "big blob" list.
// Instead, use the categorized and alphabetized lists below this "big blob".
// You can create a new category if none of the existing ones fit.
extern const base::Feature kHideFileUrlScheme;
extern const base::Feature kHideSteadyStateUrlScheme;
extern const base::Feature kHideSteadyStateUrlTrivialSubdomains;
extern const base::Feature kHideSteadyStateUrlPathQueryAndRef;
extern const base::Feature kOmniboxLocalEntitySuggestions;
extern const base::Feature kOmniboxMaxURLMatches;
extern const base::Feature kOmniboxReverseAnswers;
extern const base::Feature kOmniboxShortBookmarkSuggestions;
extern const base::Feature kOmniboxTailSuggestions;
extern const base::Feature kOmniboxTabSwitchSuggestions;
extern const base::Feature kOmniboxTabSwitchSuggestionsDedicatedRow;
extern const base::Feature kExperimentalKeywordMode;
extern const base::Feature kOmniboxPedalSuggestions;
extern const base::Feature kOmniboxSuggestionTransparencyOptions;
extern const base::Feature kEnableClipboardProviderImageSuggestions;
extern const base::Feature kSearchProviderWarmUpOnFocus;
extern const base::Feature kDisplayTitleForCurrentUrl;
extern const base::Feature kUIExperimentMaxAutocompleteMatches;
extern const base::Feature kQueryInOmnibox;
extern const base::Feature kUIExperimentSwapTitleAndUrl;
extern const base::Feature kSpeculativeServiceWorkerStartOnQueryInput;
extern const base::Feature kDocumentProvider;
extern const base::Feature kAutocompleteTitles;
extern const base::Feature kOmniboxDisableInstantExtendedLimit;
extern const base::Feature kOmniboxSearchEngineLogo;
extern const base::Feature kOmniboxRemoveSuggestionsFromClipboard;
extern const base::Feature kDebounceDocumentProvider;

// Flags that affect the "twiddle" step of AutocompleteResult, i.e. SortAndCull.
// TODO(tommycli): There are more flags above that belong in this category.
extern const base::Feature kOmniboxPreserveDefaultMatchAgainstAsyncUpdate;
extern const base::Feature kOmniboxDemoteByType;

// A special flag, enabled by default, that can be used to disable all new
// search features (e.g. zero suggest).
extern const base::Feature kNewSearchFeatures;

// On-Focus Suggestions a.k.a. ZeroSuggest.
extern const base::Feature kOnFocusSuggestions;
extern const base::Feature kOnFocusSuggestionsContextualWeb;
extern const base::Feature kProactiveZeroSuggestionsOnNTPOmnibox;
extern const base::Feature kProactiveZeroSuggestionsOnNTPRealbox;
extern const base::Feature kZeroSuggestionsOnNTP;
extern const base::Feature kZeroSuggestionsOnNTPRealbox;
extern const base::Feature kZeroSuggestionsOnSERP;

// On Device Head Suggest.
extern const base::Feature kOnDeviceHeadProviderIncognito;
extern const base::Feature kOnDeviceHeadProviderNonIncognito;

// Scoring - these affect how relevance scores are calculated for suggestions.
extern const base::Feature kOmniboxExperimentalSuggestScoring;
extern const base::Feature kHistoryQuickProviderAllowButDoNotScoreMidwordTerms;
extern const base::Feature kHistoryQuickProviderAllowMidwordContinuations;

// Suggestions UI - these affect the UI or function of the suggestions popup.
extern const base::Feature kAdaptiveSuggestionsCount;
extern const base::Feature kCompactSuggestions;
extern const base::Feature kConfirmOmniboxSuggestionRemovals;
extern const base::Feature kDeferredKeyboardPopup;
extern const base::Feature kRichAutocompletion;
extern const base::Feature kOmniboxLooseMaxLimitOnDedicatedRows;
extern const base::Feature kOmniboxSuggestionButtonRow;
extern const base::Feature kOmniboxSuggestionsRecyclerView;
extern const base::Feature kWebUIOmniboxPopup;

// Omnibox UI - these affect the UI or function of the location bar (not the
// popup).
extern const base::Feature kOmniboxAssistantVoiceSearch;
extern const base::Feature kOmniboxContextMenuShowFullUrls;

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
