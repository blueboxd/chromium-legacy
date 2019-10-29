// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.ViewAssertion;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Utilities helper class for tab grid/group tests.
 */
public class TabUiTestHelper {
    /**
     * Create {@code tabsCount} tabs for {@code cta} in certain tab model based on {@code
     * isIncognito}.
     * @param cta            A current running activity to create tabs.
     * @param isIncognito    Indicator for whether to create tabs in normal model or incognito
     *         model.
     * @param tabsCount      Number of tabs to be created.
     */
    static void createTabs(ChromeTabbedActivity cta, boolean isIncognito, int tabsCount) {
        for (int i = 0; i < (isIncognito ? tabsCount : tabsCount - 1); i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), cta, isIncognito, true);
        }
    }

    /**
     * Enter tab switcher from a tab page.
     * @param cta  The current running activity.
     */
    static void enterTabSwitcher(ChromeTabbedActivity cta) {
        OverviewModeBehaviorWatcher showWatcher = createOverviewShowWatcher(cta);
        assertFalse(cta.getLayoutManager().overviewVisible());
        onView(withId(R.id.tab_switcher_button)).perform(click());
        showWatcher.waitForBehavior();
    }

    /**
     * Click the first card in grid tab switcher. When group is enabled and the first card is a
     * group, this will open up the dialog; otherwise this will open up the tab page.
     * @param cta  The current running activity.
     */
    static void clickFirstTabFromTabSwitcher(ChromeTabbedActivity cta) {
        assertTrue(cta.getLayoutManager().overviewVisible());
        onView(allOf(withParent(withId(org.chromium.chrome.R.id.compositor_view_holder)),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
    }

    /**
     * Verify that current tab models hold correct number of tabs.
     * @param cta            The current running activity.
     * @param normalTabs     The correct number of normal tabs.
     * @param incognitoTabs  The correct number of incognito tabs.
     */
    static void verifyTabModelTabCount(
            ChromeTabbedActivity cta, int normalTabs, int incognitoTabs) {
        CriteriaHelper.pollUiThread(Criteria.equals(
                normalTabs, () -> cta.getTabModelSelector().getModel(false).getCount()));
        CriteriaHelper.pollUiThread(Criteria.equals(
                incognitoTabs, () -> cta.getTabModelSelector().getModel(true).getCount()));
    }

    /**
     * Verify there are correct number of cards in tab switcher.
     * @param cta       The current running activity.
     * @param count     The correct number of cards in tab switcher.
     */
    static void verifyTabSwitcherCardCount(ChromeTabbedActivity cta, int count) {
        assertTrue(cta.getLayoutManager().overviewVisible());
        onView(allOf(withParent(withId(org.chromium.chrome.R.id.compositor_view_holder)),
                       withId(R.id.tab_list_view)))
                .check(CardCountAssertion.havingTabCount(count));
    }

    /**
     * Create a tab group using {@code tabs}.
     * @param cta             The current running activity.
     * @param isIncognito     Whether the group is in normal model or incognito model.
     * @param tabs            A list of {@link Tab} to create group.
     */
    static void createTabGroup(ChromeTabbedActivity cta, boolean isIncognito, List<Tab> tabs) {
        if (tabs.size() == 0) return;
        assert cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getTabModelFilter(isIncognito);
        Tab rootTab = tabs.get(0);
        for (int i = 1; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            assertEquals(isIncognito, tab.isIncognito());
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> filter.mergeTabsToGroup(tab.getId(), rootTab.getId()));
        }
    }

    /**
     * Create a {@link OverviewModeBehaviorWatcher} to inspect overview show.
     */
    static OverviewModeBehaviorWatcher createOverviewShowWatcher(ChromeTabbedActivity cta) {
        return new OverviewModeBehaviorWatcher(cta.getLayoutManager(), true, false);
    }

    /**
     * Create a {@link OverviewModeBehaviorWatcher} to inspect overview hide.
     */
    static OverviewModeBehaviorWatcher createOverviewHideWatcher(ChromeTabbedActivity cta) {
        return new OverviewModeBehaviorWatcher(cta.getLayoutManager(), false, true);
    }

    /**
     * Implementation of {@link ViewAssertion} to verify the {@link RecyclerView} has correct number
     * of children, and children are showing correctly.
     */
    public static class CardCountAssertion implements ViewAssertion {
        private int mExpectedCount;

        public static CardCountAssertion havingTabCount(int tabCount) {
            return new CardCountAssertion(tabCount);
        }

        public CardCountAssertion(int expectedCount) {
            mExpectedCount = expectedCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = ((RecyclerView) view);
            RecyclerView.Adapter adapter = recyclerView.getAdapter();
            CriteriaHelper.pollUiThread(
                    Criteria.equals(mExpectedCount, () -> adapter.getItemCount()));

            for (int i = 0; i < mExpectedCount; i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) return;
                // This is to check if dialog hiding animation plays properly.
                assertTrue(1f == viewHolder.itemView.getAlpha());
                assertEquals(View.VISIBLE, viewHolder.itemView.getVisibility());
            }
        }
    }
}