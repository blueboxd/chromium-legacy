// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.MenuItem;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoNotificationService;
import org.chromium.chrome.browser.toolbar.top.CustomTabToolbar;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Instrumentation tests for {@link CustomTabActivity} launched in incognito mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.FORCE_FIRST_RUN_FLOW_COMPLETE_FOR_TESTING})
public class CustomTabActivityIncognitoTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mJUnitProcessor = new Features.JUnitProcessor();

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void launchesIncognitoWhenEnabled() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab();
        assertTrue(activity.getActivityTab().isIncognito());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void doesntLaunchIncognitoWhenDisabled() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab();
        assertFalse(activity.getActivityTab().isIncognito());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void toolbarHasIncognitoThemeColor() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab();
        assertEquals(getIncognitoThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ignoresCustomizedToolbarColor() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab(
                intent -> intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.RED));
        assertEquals(getIncognitoThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void openInBrowserMenuItemHasCorrectTitle() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab();
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);
        String menuTitle = mCustomTabActivityTestRule.getMenu()
                                   .findItem(R.id.open_in_browser_id)
                                   .getTitle()
                                   .toString();
        assertEquals(activity.getString(R.string.menu_open_in_incognito_chrome), menuTitle);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    @DisabledTest
    // TODO(crbug.com/1023759) : The test crashes in Android Kitkat and is flaky on marshmallow.
    // Need to investigate.
    public void incognitoNotificationClosesIncognitoCustomTab() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab();
        IncognitoNotificationService.getRemoveAllIncognitoTabsIntent(activity)
                .getPendingIntent()
                .send();
        CriteriaHelper.pollUiThread(activity::isFinishing);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void doesNotHaveAddToHomeScreenMenuItem() throws Exception {
        CustomTabActivity activity = launchIncognitoCustomTab();
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        MenuItem item = mCustomTabActivityTestRule.getMenu().findItem(R.id.add_to_homescreen_id);
        assertTrue(item == null || !item.isVisible());
    }

    private static int getIncognitoThemeColor(CustomTabActivity activity)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeColors.getDefaultThemeColor(activity.getResources(), true));
    }

    private static int getToolbarColor(CustomTabActivity activity) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            CustomTabToolbar toolbar = activity.findViewById(R.id.toolbar);
            return toolbar.getBackground().getColor();
        });
    }

    private CustomTabActivity launchIncognitoCustomTab() throws InterruptedException {
        return launchIncognitoCustomTab(null);
    }

    private CustomTabActivity launchIncognitoCustomTab(
            Callback<Intent> additionalIntentModifications) throws InterruptedException {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), "http://www.google.com");

        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        if (additionalIntentModifications != null) {
            additionalIntentModifications.onResult(intent);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        return mCustomTabActivityTestRule.getActivity();
    }
}