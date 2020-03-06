// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;

/**
 * Tests for {@link CustomTabToolbarColorController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomTabToolbarColorControllerTest.TestTabThemeColorHelper.class})
public class CustomTabToolbarColorControllerTest {
    /**
     * {@link TabThemeColorHelper} shadow.
     */
    @Implements(TabThemeColorHelper.class)
    public static class TestTabThemeColorHelper {
        private static int sColor;
        private static boolean sIsDefaultColor;

        public static void setForAllTabs(int color, boolean isDefaultColor) {
            sColor = color;
            sIsDefaultColor = isDefaultColor;
        }

        @Implementation
        public static int getColor(Tab tab) {
            return sColor;
        }

        @Implementation
        public static boolean isDefaultColorUsed(Tab tab) {
            return sIsDefaultColor;
        }
    }

    private static final int DEFAULT_COLOR = 0x11223344;
    private static final int TAB_PROVIDED_COLOR = 0x55667788;
    private static final int USER_PROVIDED_COLOR = 0x99aabbcc;

    @Mock
    public CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock
    public Activity mActivity;
    @Mock
    public Resources mResources;
    @Mock
    public TabImpl mTab;

    @Mock
    TabObserverRegistrar mTabObserverRegistrar;

    @Mock
    public ActivityTabProvider mTabProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mResources.getColor(R.color.toolbar_background_primary)).thenReturn(DEFAULT_COLOR);
        when(mActivity.getResources()).thenReturn(mResources);

        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        when(mCustomTabIntentDataProvider.getToolbarColor()).thenReturn(USER_PROVIDED_COLOR);
        when(mCustomTabIntentDataProvider.hasCustomToolbarColor()).thenReturn(true);

        TestTabThemeColorHelper.setForAllTabs(TAB_PROVIDED_COLOR, false /* isDefaultColor */);
    }

    @Test
    public void fallsBackWhenOpenedByChrome() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        CustomTabToolbarColorController colorController = buildController(mTab);
        assertEquals(TAB_PROVIDED_COLOR, colorController.getThemeColor());
    }

    @Test
    public void useTabThemeColor_enable() {
        CustomTabToolbarColorController colorController = buildController(mTab);
        assertEquals(USER_PROVIDED_COLOR, colorController.getThemeColor());
        colorController.setUseTabThemeColor(true);
        assertEquals(TAB_PROVIDED_COLOR, colorController.getThemeColor());
    }

    @Test
    public void useTabThemeColor_enable_nullTab() {
        when(mTabProvider.get()).thenReturn(null);

        {
            CustomTabToolbarColorController colorController = buildController(null /* activeTab */);
            colorController.setUseTabThemeColor(true);
            assertEquals(USER_PROVIDED_COLOR, colorController.getThemeColor());
        }

        {
            when(mCustomTabIntentDataProvider.hasCustomToolbarColor()).thenReturn(false);
            CustomTabToolbarColorController colorController = buildController(null /* activeTab */);
            colorController.setUseTabThemeColor(true);
            assertEquals(DEFAULT_COLOR, colorController.getThemeColor());
        }
    }

    @Test
    public void useTabThemeColor_notThemable() {
        TestTabThemeColorHelper.setForAllTabs(DEFAULT_COLOR, true /* isDefaultColor */);

        CustomTabToolbarColorController colorController = buildController(mTab);
        colorController.setUseTabThemeColor(true);
        assertEquals(DEFAULT_COLOR, colorController.getThemeColor());

        colorController.setUseTabThemeColor(false);
        assertEquals(DEFAULT_COLOR, colorController.getThemeColor());
    }

    @Test
    public void useTabThemeColor_disable() {
        CustomTabToolbarColorController colorController = buildController(mTab);
        colorController.setUseTabThemeColor(true);
        assertEquals(TAB_PROVIDED_COLOR, colorController.getThemeColor());

        colorController.setUseTabThemeColor(false);
        assertEquals(USER_PROVIDED_COLOR, colorController.getThemeColor());
    }

    @Test
    public void useTabThemeColor_disable_noCustomColor() {
        when(mCustomTabIntentDataProvider.hasCustomToolbarColor()).thenReturn(false);
        CustomTabToolbarColorController colorController = buildController(mTab);
        colorController.setUseTabThemeColor(false);
        assertEquals(DEFAULT_COLOR, colorController.getThemeColor());
    }

    CustomTabToolbarColorController buildController(Tab activeTab) {
        when(mTabProvider.get()).thenReturn(activeTab);
        CustomTabToolbarColorController colorController = spy(new CustomTabToolbarColorController(
                mActivity, mTabProvider, mTabObserverRegistrar, mCustomTabIntentDataProvider));
        colorController.updateThemeColor();
        return colorController;
    }
}
