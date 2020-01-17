// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Unit Tests for {@link FeatureUtilities}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FeatureUtilitiesUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    CommandLine mCommandLine;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mCommandLine.isNativeImplementation()).thenReturn(true);
        CommandLine.setInstanceForTesting(mCommandLine);

        AccessibilityUtil.setAccessibilityEnabledForTesting(false);
        FeatureUtilities.resetFlagsForTesting();
    }

    @After
    public void tearDown() {
        CommandLine.reset();
        FeatureUtilities.resetFlagsForTesting();
        AccessibilityUtil.setAccessibilityEnabledForTesting(null);
        SysUtils.resetForTesting();
    }

    @Test
    // clang-format off
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_NoEnabledFlags_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_NoEnabledFlags_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(false);
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_Layout_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_Layout_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();

        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_LayoutGroup_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_LayoutGroup_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_Group_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_Group_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_Continuation_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_Continuation_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_AllFlags_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_AllFlags_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);

        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_LayoutContinuation_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_LayoutContinuation_disabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertFalse(FeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_GroupContinuation_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_GroupContinuation_enabled() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        FeatureUtilities.cacheNativeTabSwitcherUiFlags();

        FeatureUtilities.resetFlagsForTesting();
        assertTrue(FeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(FeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }
}
