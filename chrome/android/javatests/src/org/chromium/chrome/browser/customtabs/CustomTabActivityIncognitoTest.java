// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.MenuItem;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.incognito.IncognitoNotificationService;
import org.chromium.chrome.browser.toolbar.top.CustomTabToolbar;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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

    private CustomTabActivity mActivity;

    // TODO(https://crbug.com/1023759): Update all test to cover both cases
    // where CCT_INCOGNITO is enabled and disabled.
    public boolean isEnabled() {
        return ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_INCOGNITO);
    }

    @Test
    @SmallTest
    public void tabIsIncognito() throws Exception {
        if (!isEnabled()) return;
        launchIncognitoCustomTab();

        Assert.assertTrue(mActivity.getActivityTab().isIncognito());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void toolbarHasIncognitoThemeColor() throws Exception {
        if (!isEnabled()) return;
        launchIncognitoCustomTab();

        Assert.assertEquals(getIncognitoThemeColor(), getToolbarColor());
    }

    @Test
    @SmallTest
    public void ignoresCustomizedToolbarColor() throws Exception {
        if (!isEnabled()) return;
        launchIncognitoCustomTab(
                intent -> intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.RED));

        Assert.assertEquals(getIncognitoThemeColor(), getToolbarColor());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void openInBrowserMenuItemHasCorrectTitle() throws Exception {
        if (!isEnabled()) return;
        launchIncognitoCustomTab();

        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mActivity);

        String menuTitle = mCustomTabActivityTestRule.getMenu()
                                   .findItem(R.id.open_in_browser_id)
                                   .getTitle()
                                   .toString();
        Assert.assertEquals(mActivity.getString(R.string.menu_open_in_incognito_chrome), menuTitle);
    }

    @Test
    @SmallTest
    public void incognitoNotificationClosesCustomTab() throws Exception {
        if (!isEnabled()) return;
        launchIncognitoCustomTab();

        IncognitoNotificationService.getRemoveAllIncognitoTabsIntent(mActivity)
                .getPendingIntent()
                .send();

        CriteriaHelper.pollUiThread(mActivity::isFinishing);
    }

    @Test
    @SmallTest
    public void doesNotHaveAddToHomeScreenMenuItem() throws Exception {
        if (!isEnabled()) return;
        launchIncognitoCustomTab();
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mActivity);

        MenuItem item = mCustomTabActivityTestRule.getMenu().findItem(R.id.add_to_homescreen_id);
        Assert.assertTrue(item == null || !item.isVisible());
    }

    private void launchIncognitoCustomTab() throws InterruptedException {
        launchIncognitoCustomTab(null);
    }

    private void launchIncognitoCustomTab(Callback<Intent> additionalIntentModifications)
            throws InterruptedException {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), "http://www.google.com");

        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        if (additionalIntentModifications != null) {
            additionalIntentModifications.onResult(intent);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        mActivity = mCustomTabActivityTestRule.getActivity();
    }

    private int getIncognitoThemeColor() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeColors.getDefaultThemeColor(mActivity.getResources(), true));
    }

    private int getToolbarColor() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
            return toolbar.getBackground().getColor();
        });
    }
}