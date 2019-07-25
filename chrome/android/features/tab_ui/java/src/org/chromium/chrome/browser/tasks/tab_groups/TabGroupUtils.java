// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.support.annotation.StringRes;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;

/**
 * Helper class to handle tab groups related utilities.
 */
public class TabGroupUtils {
    private static TabModelSelectorTabObserver sTabModelSelectorTabObserver;

    public static void maybeShowIPH(@FeatureConstants String featureName, View view) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)) return;

        @StringRes
        int textId;
        @StringRes
        int accessibilityTextId;
        switch (featureName) {
            case FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE:
                textId = R.string.iph_tab_groups_quickly_compare_pages_text;
                accessibilityTextId = textId;
                break;
            case FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE:
                textId = R.string.iph_tab_groups_tap_to_see_another_tab_text;
                accessibilityTextId =
                        R.string.iph_tab_groups_tap_to_see_another_tab_accessibility_text;
                break;
            case FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE:
                textId = R.string.iph_tab_groups_your_tabs_together_text;
                accessibilityTextId = textId;
                break;
            default:
                assert false;
                return;
        }

        final Tracker tracker = TrackerFactory.getTrackerForProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(view);

        TextBubble textBubble = new TextBubble(
                view.getContext(), view, textId, accessibilityTextId, true, rectProvider);
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.addOnDismissListener(() -> tracker.dismissed(featureName));
        textBubble.show();
    }

    /**
     * Start a TabModelSelectorTabObserver to show IPH for TabGroups.
     * @param selector The selector that owns the Tabs that should be observed.
     */
    public static void startObservingForTabGroupsIPH(TabModelSelector selector) {
        if (sTabModelSelectorTabObserver != null) return;
        sTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (!navigationHandle.isInMainFrame() || navigationHandle.pageTransition() == null)
                    return;
                if (tab.isIncognito()) return;
                if (navigationHandle.pageTransition() == null) return;

                int coreTransitionType =
                        navigationHandle.pageTransition() & PageTransition.CORE_MASK;
                // Searching from omnibox results in PageTransition.GENERATED.
                if (navigationHandle.isValidSearchFormUrl()
                        || coreTransitionType == PageTransition.GENERATED) {
                    maybeShowIPH(FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
                            tab.getView());
                    sTabModelSelectorTabObserver.destroy();
                }
            }
        };
    }

    /**
     * This method gets the selected tab of the group where {@code tab} is in.
     * @param selector   The selector that owns the {@code tab}.
     * @param tab        {@link Tab}
     * @return The selected tab of the group which contains the {@code tab}
     */
    public static Tab getSelectedTabInGroupForTab(TabModelSelector selector, Tab tab) {
        TabGroupModelFilter filter = (TabGroupModelFilter) selector.getTabModelFilterProvider()
                                             .getCurrentTabModelFilter();
        return filter.getTabAt(filter.indexOf(tab));
    }

    /**
     * This method gets the index in TabModel of the first tab in {@code tabs}.
     * @param tabModel   The tabModel that owns the {@code tab}.
     * @param tabs       The list of tabs among which we need to find the first tab index.
     * @return The index in TabModel of the first tab in {@code tabs}
     */
    public static int getFirstTabModelIndexForList(TabModel tabModel, List<Tab> tabs) {
        assert tabs != null && tabs.size() != 0;

        return tabModel.indexOf(tabs.get(0));
    }

    /**
     * This method gets the index in TabModel of the last tab in {@code tabs}.
     * @param tabModel   The tabModel that owns the {@code tab}.
     * @param tabs       The list of tabs among which we need to find the last tab index.
     * @return The index in TabModel of the last tab in {@code tabs}
     */
    public static int getLastTabModelIndexForList(TabModel tabModel, List<Tab> tabs) {
        assert tabs != null && tabs.size() != 0;

        return tabModel.indexOf(tabs.get(tabs.size() - 1));
    }
}
