// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.util.Pair;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.WaitForFocusHelper;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.CriteriaNotSatisfiedException;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.List;

/**
 * Tests of the Switch To Tab feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SwitchToTabTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    private static final int INVALID_INDEX = -1;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Type the |text| into |activity|'s url_bar.
     *
     * @param activity The Activity which url_bar is in.
     * @param text The text will be typed into url_bar.
     */
    private void typeInOmnibox(Activity activity, String text) throws InterruptedException {
        final UrlBar urlBar = activity.findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        WaitForFocusHelper.acquireFocusForView(urlBar);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.setText(text); });
    }

    /**
     * Type the |text| into |activity|'s url_bar, and wait for switch to tab suggestion shows up.
     *
     * @param activity The Activity which url_bar is in.
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param input The text will be typed into url_bar.
     * @param expectedMatchUrl The expected url to visit after clicking tab switch button.
     */
    private void typeAndClickMatchingTabMatchSuggestion(Activity activity,
            LocationBarLayout locationBarLayout, String input, String expectedMatchUrl)
            throws InterruptedException {
        typeInOmnibox(activity, input);

        OmniboxTestUtils.waitForOmniboxSuggestions(locationBarLayout);
        // waitForOmniboxSuggestions only wait until one suggestion shows up, we need to wait util
        // autocomplete return more suggestions.
        CriteriaHelper.pollUiThread(() -> {
            Pair<Integer, OmniboxSuggestion> matchSuggestion =
                    findFirstTabMatchOmniboxSuggestion(locationBarLayout);

            Criteria.checkThat(matchSuggestion.first, Matchers.not(INVALID_INDEX));
            Criteria.checkThat(
                    matchSuggestion.second.getUrl().getSpec(), Matchers.is(expectedMatchUrl));

            OmniboxSuggestionsDropdown suggestionsDropdown =
                    AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                            locationBarLayout.getAutocompleteCoordinator());
            try {
                clickSuggestionActionAt(suggestionsDropdown, matchSuggestion.first);
            } catch (InterruptedException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
        });
    }

    /**
     * Find the first switch to tab suggestion in the omnibox suggestion list, and return the
     * suggestion and its index. This method needs to run on the UI thread.
     *
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @return The first switch to tab suggestion's index, and the suggesstion.
     */
    private Pair<Integer, OmniboxSuggestion> findFirstTabMatchOmniboxSuggestion(
            LocationBarLayout locationBarLayout) {
        ThreadUtils.assertOnUiThread();

        OmniboxSuggestionsDropdown suggestionsDropdown =
                AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                        locationBarLayout.getAutocompleteCoordinator());
        // Find the index of the first matching suggestion.
        for (int i = 0; i < suggestionsDropdown.getItemCount(); ++i) {
            OmniboxSuggestion suggestion = AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                    locationBarLayout.getAutocompleteCoordinator(), i);
            if (suggestion != null && suggestion.hasTabMatch()) {
                return new Pair<Integer, OmniboxSuggestion>(i, suggestion);
            }
        }
        return new Pair<Integer, OmniboxSuggestion>(INVALID_INDEX, null);
    }

    /**
     * Find the |index|th suggestion in |suggestionsDropdown| and click it.
     *
     * @param suggestionsDropdown The omnibox suggestion's dropdown list.
     * @param index The index of the suggestion tied to click.
     */
    private void clickSuggestionActionAt(OmniboxSuggestionsDropdown suggestionsDropdown, int index)
            throws InterruptedException {
        // Wait a bit since the button may not able to click.
        ViewGroup viewGroup = suggestionsDropdown.getViewGroup();
        BaseSuggestionView baseSuggestionView = (BaseSuggestionView) viewGroup.getChildAt(index);
        List<ImageView> buttonsList = baseSuggestionView.getActionButtons();
        Assert.assertEquals(buttonsList.size(), 1);
        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), buttonsList.get(0));
    }

    /**
     * Launch the SearchActiviy.
     */
    private SearchActivity startSearchActivity() {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        ActivityMonitor searchMonitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(searchMonitor);

        // Fire the Intent to start up the SearchActivity.
        Intent intent = new Intent();
        SearchWidgetProvider.startSearchActivity(intent, /* isVoiceSearch = */ false);
        Activity searchActivity = instrumentation.waitForMonitorWithTimeout(
                searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
        instrumentation.removeMonitor(searchMonitor);
        return (SearchActivity) searchActivity;
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxTabSwitchSuggestions")
    public void
    testSwitchToTabSuggestion() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl1);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeAndClickMatchingTabMatchSuggestion(
                mActivityTestRule.getActivity(), locationBarLayout, "about", testHttpsUrl1);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            if (tab == null) return false;
            // Make sure tab is in either upload page or result page. cannot only verify one of
            // them since on fast device tab jump to result page really quick but on slow device
            // may stay on upload page for a really long time.
            return tab.getUrlString().equals(testHttpsUrl1);
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxTabSwitchSuggestions")
    public void testNoSwitchToIncognitoTabFromNormalModel() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        // Open the url trying to match in incognito mode.
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl1, true);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        // trying to match incognito tab.
        mActivityTestRule.typeInOmnibox("about", false);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBarLayout);

        CriteriaHelper.pollUiThread(() -> {
            Pair<Integer, OmniboxSuggestion> matchSuggestion =
                    findFirstTabMatchOmniboxSuggestion(locationBarLayout);

            Criteria.checkThat(matchSuggestion.first, Matchers.is(INVALID_INDEX));
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxTabSwitchSuggestions")
    public void testSwitchToTabInSearchActivity() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl1);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        final SearchActivity searchActivity = startSearchActivity();
        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            // Make sure chrome fully in background.
            Criteria.checkThat(tab.getWindowAndroid().getActivityState(),
                    Matchers.isOneOf(ActivityState.STOPPED, ActivityState.DESTROYED));
        });

        final LocationBarLayout locationBarLayout =
                (LocationBarLayout) searchActivity.findViewById(R.id.search_location_bar);
        typeAndClickMatchingTabMatchSuggestion(
                searchActivity, locationBarLayout, "about", testHttpsUrl1);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab.getUrlString(), Matchers.is(testHttpsUrl1));
            // Make sure tab is loaded and in foreground.
            Criteria.checkThat(
                    tab.getWindowAndroid().getActivityState(), Matchers.is(ActivityState.RESUMED));
        });
    }
}
