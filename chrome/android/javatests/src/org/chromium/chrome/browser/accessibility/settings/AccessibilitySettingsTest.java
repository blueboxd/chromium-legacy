// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.app.Instrumentation;
import android.content.IntentFilter;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import androidx.preference.Preference;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.UiUtils;

import java.text.NumberFormat;

/**
 * Tests for the Accessibility Settings menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AccessibilitySettingsTest {
    @Rule
    public SettingsActivityTestRule<AccessibilitySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccessibilitySettings.class);

    @Before
    public void setUp() {
        mSettingsActivityTestRule.startSettingsActivity();
    }

    /**
     * Tests setting FontScaleFactor and ForceEnableZoom in AccessibilitySettings and ensures
     * that ForceEnableZoom changes corresponding to FontScaleFactor.
     */
    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testAccessibilitySettings() throws Exception {
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();
        TextScalePreference textScalePref =
                (TextScalePreference) accessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_TEXT_SCALE);
        ChromeBaseCheckBoxPreference forceEnableZoomPref =
                (ChromeBaseCheckBoxPreference) accessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_FORCE_ENABLE_ZOOM);
        NumberFormat percentFormat = NumberFormat.getPercentInstance();
        // Arbitrary value 0.4f to be larger and smaller than threshold.
        float fontSmallerThanThreshold =
                FontSizePrefs.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER - 0.4f;
        float fontBiggerThanThreshold = FontSizePrefs.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER + 0.4f;

        // Set the textScaleFactor above the threshold.
        userSetTextScale(accessibilitySettings, textScalePref, fontBiggerThanThreshold);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since above the threshold, this will check the force enable zoom button.
        Assert.assertEquals(
                percentFormat.format(fontBiggerThanThreshold), textScalePref.getAmountForTesting());
        Assert.assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontBiggerThanThreshold);

        // Set the textScaleFactor below the threshold.
        userSetTextScale(accessibilitySettings, textScalePref, fontSmallerThanThreshold);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since below the threshold and userSetForceEnableZoom is false, this will uncheck
        // the force enable zoom button.
        Assert.assertEquals(percentFormat.format(fontSmallerThanThreshold),
                textScalePref.getAmountForTesting());
        Assert.assertFalse(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(false, fontSmallerThanThreshold);

        userSetTextScale(accessibilitySettings, textScalePref, fontBiggerThanThreshold);
        // Sets onUserSetForceEnableZoom to be true.
        userSetForceEnableZoom(accessibilitySettings, forceEnableZoomPref, true);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since userSetForceEnableZoom is true, when the text scale is moved below the threshold
        // ForceEnableZoom should remain checked.
        userSetTextScale(accessibilitySettings, textScalePref, fontSmallerThanThreshold);
        Assert.assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontSmallerThanThreshold);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testCaptionPreferences() {
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();
        Preference captionsPref =
                accessibilitySettings.findPreference(AccessibilitySettings.PREF_CAPTIONS);
        Assert.assertNotNull(captionsPref);
        Assert.assertNotNull(captionsPref.getOnPreferenceClickListener());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Settings.ACTION_CAPTIONING_SETTINGS), null, false);

        onView(withText(org.chromium.chrome.R.string.accessibility_captions_title))
                .perform(click());
        monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    private void assertFontSizePrefs(
            final boolean expectedForceEnableZoom, final float expectedFontScale) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FontSizePrefs fontSizePrefs = FontSizePrefs.getInstance();
            Assert.assertEquals(expectedForceEnableZoom, fontSizePrefs.getForceEnableZoom());
            Assert.assertEquals(expectedFontScale, fontSizePrefs.getFontScaleFactor(), 0.001f);
        });
    }

    private static void userSetTextScale(final AccessibilitySettings accessibilitySettings,
            final TextScalePreference textScalePref, final float textScale) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> accessibilitySettings.onPreferenceChange(textScalePref, textScale));
    }

    private static void userSetForceEnableZoom(final AccessibilitySettings accessibilitySettings,
            final ChromeBaseCheckBoxPreference forceEnableZoomPref, final boolean enabled) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> accessibilitySettings.onPreferenceChange(forceEnableZoomPref, enabled));
    }
}