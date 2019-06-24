// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.themes;

import static org.chromium.chrome.browser.ChromeFeatureList.ANDROID_NIGHT_MODE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceManager.UI_THEME_SETTING_KEY;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences.ThemeSetting;
import org.chromium.chrome.browser.widget.RadioButtonWithDescription;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for ThemePreferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ANDROID_NIGHT_MODE)
public class ThemePreferencesTest extends DummyUiActivityTestCase {
    private ThemePreferences mFragment;
    private RadioButtonGroupThemePreference mPreference;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        ChromePreferenceManager.getInstance().removeKey(UI_THEME_SETTING_KEY);
        Preferences preferences = PreferencesTest.startPreferences(
                InstrumentationRegistry.getInstrumentation(), ThemePreferences.class.getName());
        mFragment = (ThemePreferences) preferences.getMainFragmentCompat();
        mPreference = (RadioButtonGroupThemePreference) mFragment.findPreference(
                ThemePreferences.PREF_UI_THEME_PREF);
    }

    @Override
    public void tearDownTest() throws Exception {
        ChromePreferenceManager.getInstance().removeKey(UI_THEME_SETTING_KEY);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @Feature({"Themes"})
    public void testSelectThemes() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Select System default
            Assert.assertEquals(R.id.system_default, getButton(0).getId());
            selectButton(0);
            assertButtonCheckedCorrectly("System default", 0);
            Assert.assertEquals(ThemeSetting.SYSTEM_DEFAULT, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    ChromePreferenceManager.getInstance().readInt(UI_THEME_SETTING_KEY));

            // Select Light
            Assert.assertEquals(R.id.light, getButton(1).getId());
            selectButton(1);
            assertButtonCheckedCorrectly("Light", 1);
            Assert.assertEquals(ThemeSetting.LIGHT, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    ChromePreferenceManager.getInstance().readInt(UI_THEME_SETTING_KEY));

            // Select Dark
            Assert.assertEquals(R.id.dark, getButton(2).getId());
            selectButton(2);
            assertButtonCheckedCorrectly("Dark", 2);
            Assert.assertEquals(ThemeSetting.DARK, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    ChromePreferenceManager.getInstance().readInt(UI_THEME_SETTING_KEY));
        });
    }

    private RadioButtonWithDescription getButton(int index) {
        return (RadioButtonWithDescription) mPreference.getButtonsForTesting().get(index);
    }

    private void selectButton(int index) {
        getButton(index).onClick(null);
    }

    private boolean isRestUnchecked(int selectedIndex) {
        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            if (i != selectedIndex && getButton(i).isChecked()) {
                return false;
            }
        }
        return true;
    }

    private void assertButtonCheckedCorrectly(String buttonTitle, int index) {
        Assert.assertTrue(buttonTitle + " button should be checked.", getButton(index).isChecked());
        Assert.assertTrue(
                "Buttons except " + buttonTitle + " should be unchecked.", isRestUnchecked(index));
    }
}
