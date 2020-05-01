// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Unit test for {@link HomepagePromoUtils}.
 *
 * TODO(wenyufu):
 * 1. Decouple HomepageTestRule for unit tests so that this test can run with test rule.
 * 2. Make NewTabPage#isNTP more test friendly, then add test case for:
 *      - Partner homepage is NTP
 *      - Default homepage
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomepagePromoUnitTest {
    private static final String PARTNER_URL = "http://127.0.0.1:8000/foo.html";
    private static final String CUSTOM_URL = "http://127.0.0.1:8000/foo.html";
    private static final String NTP_URL = "chrome://newtab";

    @Before
    public void setUp() {
        HomepagePromoUtils.setBypassUrlIsNTPForTests(true);
    }

    private void setupTest(
            boolean useChromeNTP, boolean useDefaultUri, String customUri, String partnerUri) {
        // By default, enable the homepage and set promo is not dismissed.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        HomepagePromoUtils.setPromoDismissedInSharedPreference(false);

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, useChromeNTP);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, useDefaultUri);
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, customUri);
        PartnerBrowserCustomizations.getInstance().setHomepageForTests(partnerUri);
    }

    @Test
    @SmallTest
    public void testShouldCreatePromo_PartnerUri() {
        setupTest(false, true, "", PARTNER_URL);
        Assert.assertTrue("Promo should show.", HomepagePromoUtils.shouldCreatePromo(null));
    }

    @Test
    @SmallTest
    public void testShouldCreatePromo_CustomNTP() {
        setupTest(true, false, CUSTOM_URL, "");
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
        Assert.assertFalse("Promo should not show.", HomepagePromoUtils.shouldCreatePromo(null));
    }

    @Test
    @SmallTest
    public void testShouldCreatePromo_HomepageDisabled() {
        setupTest(false, true, "", PARTNER_URL);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
        Assert.assertFalse("Promo should not show.", HomepagePromoUtils.shouldCreatePromo(null));
    }

    @Test
    @SmallTest
    public void testShouldCreatePromo_Dismissed() {
        setupTest(false, true, "", PARTNER_URL);
        HomepagePromoUtils.setPromoDismissedInSharedPreference(true);
        Assert.assertFalse("Promo should not show.", HomepagePromoUtils.shouldCreatePromo(null));
    }
}
