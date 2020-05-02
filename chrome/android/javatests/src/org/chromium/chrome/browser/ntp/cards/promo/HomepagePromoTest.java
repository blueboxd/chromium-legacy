// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.os.Build.VERSION_CODES;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationIntegrationTestRule;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit test for {@link HomepagePromoController}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.HOMEPAGE_PROMO_CARD)
public class HomepagePromoTest {
    public static final String PARTNER_HOMEPAGE_URL = "http://127.0.0.1:8000/foo.html";
    public static final String CUSTOM_TEST_URL = "http://127.0.0.1:8000/bar.html";

    private static final int NTP_HEADER_POSITION = 0;

    private boolean mHasHomepagePromoDismissed; // Test value before the test.
    private boolean mHasSignInPromoDismissed; // Test value before the test.

    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();
    @Rule
    public BasePartnerBrowserCustomizationIntegrationTestRule mPartnerTestRule =
            new BasePartnerBrowserCustomizationIntegrationTestRule();
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    private Tracker mTracker;
    @Mock
    private HomepagePromoVariationManager mMockVariationManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Set promo not dismissed.
        mHasHomepagePromoDismissed = HomepagePromoUtils.isPromoDismissedInSharedPreference();
        HomepagePromoUtils.setPromoDismissedInSharedPreference(false);

        mHasSignInPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);

        // Set up mock tracker
        Mockito.when(mTracker.shouldTriggerHelpUI(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE))
                .thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);

        // By default, use default homepage just like a first time user.
        mHomepageTestRule.useDefaultHomepageForTest();
        SnackbarManager.setDurationForTesting(1000);

        // Start activity first. Doing this to make SignInManager to allow signInPromo to show up.
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        SignInPromo.setDisablePromoForTests(false);
        HomepagePromoUtils.setPromoDismissedInSharedPreference(mHasHomepagePromoDismissed);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, mHasSignInPromoDismissed);
        HomepagePromoVariationManager.setInstanceForTesting(null);
    }

    /**
     * Test that the home page promo should show for users with default homepage.
     * If the user change the homepage, the promo should be gone.
     */
    @Test
    @SmallTest
    public void testSetUp_Basic() {
        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        Assert.assertTrue("Homepage Promo should match creation criteria.",
                HomepagePromoUtils.shouldCreatePromo(null));

        View homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull("Homepage promo should be added to NTP.", homepagePromo);
        Assert.assertEquals(
                "Homepage promo should be visible.", View.VISIBLE, homepagePromo.getVisibility());

        // Change the homepage from homepage manager, so that state listeners get the update.
        // The promo should be gone from the screen.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            HomepageManager.getInstance().setHomepagePreferences(false, false, CUSTOM_TEST_URL);
        });
        homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull("Promo should be gone after homepage changes.", homepagePromo);
    }

    @Test
    @SmallTest
    public void testSetUp_HasSignInPromo() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        Assert.assertTrue("Homepage Promo should match creation criteria.",
                HomepagePromoUtils.shouldCreatePromo(null));
        Assert.assertNull("Homepage promo should not show on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNotNull("SignInPromo should be displayed on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        // Dismiss the signInPromo, and refresh NTP, the homepage promo should show up.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        Assert.assertNull("SignInPromo should be dismissed.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        View homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull(
                "Homepage promo should be added to NTP when SignInPromo dismissed.", homepagePromo);
    }

    /**
     * Clicking on dismiss button should hide the promo.
     */
    @Test
    @MediumTest
    public void testDismiss() {
        // In order to dismiss the promo, we have to use the large / compact variation.
        setVariationForTests(LayoutStyle.COMPACT);

        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        scrollToHomepagePromo();

        onView(withId(R.id.promo_secondary_button)).perform(click());
        Assert.assertNull("Homepage promo should not be removed.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));

        // Load to NTP one more time. The promo should not show.
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        Assert.assertNull("Homepage promo should not be added to NTP.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O, message = "crbug.com/1077316s")
    public void testChangeHomepageAndUndo() {
        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        scrollToHomepagePromo();

        // Click on the promo card to set the homepage to NTP.
        onView(withId(R.id.promo_primary_button)).perform(click());
        Assert.assertNull("Homepage promo should not be removed.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));

        Assert.assertTrue("Homepage should be set to NTP after clicking on promo primary button.",
                NewTabPage.isNTPUrl(HomepageManager.getHomepageUri()));

        // Verifications on snackbar, and click on "undo".
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
        Assert.assertEquals("Undo snackbar not shown.", Snackbar.UMA_HOMEPAGE_PROMO_CHANGED_UNDO,
                snackbar.getIdentifierForTesting());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { snackbar.getController().onAction(snackbar.getActionDataForTesting()); });
        Assert.assertEquals("Homepage should change back to partner homepage after undo.",
                HomepageManager.getHomepageUri(), PARTNER_HOMEPAGE_URL);
    }

    private void scrollToHomepagePromo() {
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(NTP_HEADER_POSITION + 1));
        waitForView((ViewGroup) mActivityTestRule.getActivity().findViewById(R.id.homepage_promo),
                allOf(withId(R.id.promo_primary_button), isDisplayed()));
    }

    private void setVariationForTests(@LayoutStyle int variation) {
        Mockito.when(mMockVariationManager.getLayoutVariation()).thenReturn(variation);
        HomepagePromoVariationManager.setInstanceForTesting(mMockVariationManager);
    }
}
