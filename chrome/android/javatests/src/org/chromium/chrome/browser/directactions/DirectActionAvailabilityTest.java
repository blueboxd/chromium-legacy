// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Tests the availability of core direct actions in different activities.
 *
 * <p>This tests both {@link DirectActionInitializer} and its integration with {@link
 * ChromeActivity} and its different subclasses.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_DIRECT_ACTIONS)
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
@TargetApi(24) // For java.util.function.Consumer.
public class DirectActionAvailabilityTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public WebappActivityTestRule mWebAppActivityTestRule = new WebappActivityTestRule();

    @Rule
    public DirectActionTestRule mDirectActionRule = new DirectActionTestRule();

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCoreDirectActionInTabbedActivity() throws Exception {
        mTabbedActivityTestRule.startMainActivityOnBlankPage();

        assertThat(setupActivityAndGetDirectAction(mTabbedActivityTestRule),
                Matchers.containsInAnyOrder("go_back", "reload", "go_forward", "bookmark_this_page",
                        "downloads", "preferences", "open_history", "help", "new_tab", "close_tab",
                        "close_all_tabs"));
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCoreDirectActionInCustomTabActivity() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));

        assertThat(setupActivityAndGetDirectAction(mCustomTabActivityTestRule),
                Matchers.containsInAnyOrder(
                        "go_back", "reload", "go_forward", "bookmark_this_page", "preferences"));
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCoreDirectActionInWebappActivity() throws Exception {
        mWebAppActivityTestRule.startWebappActivity();

        assertThat(setupActivityAndGetDirectAction(mWebAppActivityTestRule),
                Matchers.containsInAnyOrder("go_back", "reload", "go_forward"));
    }

    private List<String> setupActivityAndGetDirectAction(ChromeActivityTestRule<?> rule)
            throws Exception {
        allowGoForward(rule);
        return DirectActionTestUtils.callOnGetDirectActions(rule.getActivity());
    }

    /**
     * Forces availability of the "go_forward" direct action on the current tab by loading another
     * URL then navigating back to the current one.
     *
     * <p>The activity of the given rule must have been started and have loaded a page.
     */
    public static void allowGoForward(ChromeActivityTestRule<?> rule) throws Exception {
        ChromeActivity activity = rule.getActivity();
        String initialUrl = TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getCurrentWebContents().getLastCommittedUrl());

        // Any built-in page that is not about:blank and is reasonably cheap to render will do,
        // here.
        String visitedUrl = "chrome://version/";
        assertThat(initialUrl, Matchers.not(Matchers.equalTo(visitedUrl)));
        rule.loadUrl(visitedUrl);

        Tab tab = activity.getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getCurrentWebContents().getNavigationController().goBack());
        ChromeTabUtils.waitForTabPageLoaded(tab, initialUrl);
        assertTrue(tab.canGoForward());
    }
}
