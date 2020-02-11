// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Baseline method for identifying Tab candidates which the user may want
 * to close due to lack of engagement. It returns all Tabs. This is
 * used for testing code upstream from the provider - not the provider
 * itself. This provider is very deterministic and creates very stable
 * instrumentation tests and tests for {@link TabSuggestionsOrchestrator}
 * and {@link TabSuggestionsClientFetcher}
 */
public class BaselineStaleTabSuggestionProvider implements TabSuggestionProvider {
    @Override
    public List<TabSuggestion> suggest(TabContext tabContext) {
        if (tabContext == null || tabContext.getUngroupedTabs() == null
                || tabContext.getUngroupedTabs().size() < 1) {
            return null;
        }
        List<TabContext.TabInfo> staleTabs = new ArrayList<>();
        staleTabs.addAll(tabContext.getUngroupedTabs());
        return Arrays.asList(new TabSuggestion(staleTabs, TabSuggestion.TabSuggestionAction.CLOSE,
                TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER));
    }
}
