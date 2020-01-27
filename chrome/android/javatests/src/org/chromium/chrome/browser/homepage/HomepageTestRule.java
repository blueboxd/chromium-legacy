// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Test rule for homepage related tests. It fetches the latest values from shared preference
 * manager, copy them before test starts, and restores those values inside SharedPreferenceManager
 * when test finished.
 */
public class HomepageTestRule implements TestRule {
    private boolean mIsHomepageEnabled;
    private boolean mIsUsingDefaultHomepage;
    private String mCustomizedHomepage;

    private final SharedPreferencesManager mManager;

    public HomepageTestRule() {
        mManager = SharedPreferencesManager.getInstance();
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    copyInitialValueBeforeTest();
                    base.evaluate();
                } finally {
                    restoreHomepageRelatedPreferenceAfterTest();
                }
            }
        };
    }

    private void copyInitialValueBeforeTest() {
        mIsHomepageEnabled = mManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mIsUsingDefaultHomepage =
                mManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
        mCustomizedHomepage = mManager.readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, "");
    }

    private void restoreHomepageRelatedPreferenceAfterTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, mIsHomepageEnabled);
        mManager.writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, mIsUsingDefaultHomepage);
        mManager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, mCustomizedHomepage);
    }

    // Utility functions that help setting up homepage related shared preference.

    /**
     * Set homepage disabled for this test cases.
     *
     * HOMEPAGE_ENABLED -> false
     */
    public void disableHomepageForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
    }

    /**
     * Set up shared preferences to use default Homepage in the testcase.
     *
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> true;
     */
    public void useDefaultHomepageForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
    }

    /**
     * Set up shared preferences to use customized homepage.
     *
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> false;
     * HOMEPAGE_CUSTOM_URI -> <b>homepage</b>
     *
     * @param homepage The customized homepage that will be used in this testcase.
     */
    public void useCustomizedHomepageForTest(String homepage) {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, false);
        mManager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, homepage);
    }
}
