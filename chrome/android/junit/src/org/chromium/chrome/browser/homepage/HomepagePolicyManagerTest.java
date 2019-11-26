// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the {@link HomepagePolicyManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
public class HomepagePolicyManagerTest {
    public static final String TEST_URL = "http://127.0.0.1:8000/foo.html";
    public static final String CHROME_NTP = "chrome-native://newtab/";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private HomepagePolicyManager mHomepagePolicyManager;

    @Mock
    private PrefServiceBridge mMockServiceBridge;

    @Mock
    private PrefChangeRegistrar mMockRegistrar;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        // Disable the policy during setup
        PrefServiceBridge.setInstanceForTesting(mMockServiceBridge);
        setupNewHomepagePolicyManagerForTests(false, "");

        // Verify setup
        Assert.assertFalse("#isHomepageManagedByPolicy == true without homepage pref setup",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
    }

    /**
     * Set up the homepage location for Mock PrefServiceBridge, and create HomepagePolicyManager
     * instance.
     * @param homepageLocation homepage preference that will be returned by mock pref service
     */
    private void setupNewHomepagePolicyManagerForTests(
            boolean isPolicyEnabled, String homepageLocation) {
        Mockito.when(mMockServiceBridge.isManagedPreference(Pref.HOME_PAGE))
                .thenReturn(isPolicyEnabled);
        Mockito.when(mMockServiceBridge.getString(Pref.HOME_PAGE)).thenReturn(homepageLocation);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar);
            HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        });
    }

    @Test
    @SmallTest
    public void testInitialization() {
        setupNewHomepagePolicyManagerForTests(true, TEST_URL);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("#isHomepageManagedByPolicy not consistent with test setting",
                    HomepagePolicyManager.isHomepageManagedByPolicy());
            Assert.assertEquals("#getHomepageUrl not consistent with test setting", TEST_URL,
                    HomepagePolicyManager.getHomepageUrl());
        });
    }

    @Test
    @SmallTest
    public void testInitialization_NTP() {
        setupNewHomepagePolicyManagerForTests(true, CHROME_NTP);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("#isHomepageManagedByPolicy not consistent with test setting",
                    HomepagePolicyManager.isHomepageManagedByPolicy());
            Assert.assertEquals("#getHomepageUrl not consistent with test setting", CHROME_NTP,
                    HomepagePolicyManager.getHomepageUrl());
        });
    }

    @Test
    @SmallTest
    public void testDestroy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> HomepagePolicyManager.destroy());
        Mockito.verify(mMockRegistrar).destroy();
    }

    @Test
    @SmallTest
    public void testPrefRefreshToEnablePolicy() {
        Assert.assertFalse("Policy should be disabled after set up",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

        // A new policy URL is set, which triggers the refresh of native manager.
        final String newUrl = "https://www.anothertesturl.com";
        Mockito.when(mMockServiceBridge.isManagedPreference(Pref.HOME_PAGE)).thenReturn(true);
        Mockito.when(mMockServiceBridge.getString(Pref.HOME_PAGE)).thenReturn(newUrl);

        // Update the preference, so that the policy will be enabled.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHomepagePolicyManager.onPreferenceChange(); });

        // The homepage retrieved from homepage manager should be in sync with pref setting
        Assert.assertTrue("Policy should be enabled after refresh",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
        Assert.assertEquals("#getHomepageUrl not consistent with test setting", newUrl,
                mHomepagePolicyManager.getHomepagePreference());
    }

    @Test
    @SmallTest
    public void testPrefRefreshToDisablePolicy() {
        // Set a new HomepagePolicyManager with policy enabled
        setupNewHomepagePolicyManagerForTests(true, TEST_URL);

        // The verify policyEnabled
        Assert.assertTrue("Policy should be enabled after set up",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

        // Update the preference, so that the policy will be disabled.
        Mockito.when(mMockServiceBridge.isManagedPreference(Pref.HOME_PAGE)).thenReturn(false);
        Mockito.when(mMockServiceBridge.getString(Pref.HOME_PAGE)).thenReturn("");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHomepagePolicyManager.onPreferenceChange(); });

        // The homepage retrieved from homepage manager should be in sync with pref setting
        Assert.assertFalse("Policy should be disabled after refresh",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
    public void testFeatureFlagDisabled() {
        // Disable the feature flag, the instance should not monitor the Pref changes
        // mTestFeatureMap.put(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, false);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Mockito.reset(mMockRegistrar);
            Mockito.reset(mMockServiceBridge);

            // 1. Test initialization early finishing
            HomepagePolicyManager manager = new HomepagePolicyManager(mMockRegistrar);
            Mockito.verify(mMockRegistrar, Mockito.never())
                    .addObserver(Mockito.anyInt(), Mockito.any());

            // 2. Test getters
            Assert.assertFalse("Policy should be disabled when feature flag disabled",
                    mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

            // 3. Test destroy. Registrar should not be destroyed
            manager.destroyInternal();
            Mockito.verify(mMockRegistrar, Mockito.never()).destroy();
        });
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testIllegal_GetHomepageUrl() {
        setupNewHomepagePolicyManagerForTests(false, "");
        TestThreadUtils.runOnUiThreadBlocking(() -> { HomepagePolicyManager.getHomepageUrl(); });
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    @DisableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
    public void testIllegal_Refresh() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHomepagePolicyManager.onPreferenceChange(); });
    }
}