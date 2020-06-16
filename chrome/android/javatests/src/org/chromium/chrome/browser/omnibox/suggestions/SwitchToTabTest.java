// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * Tests of the Switch To Tab feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SwitchToTabTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void typeAndWaitForHasTabMatchSuggestions(
            LocationBarLayout locationBarLayout, String input) throws InterruptedException {
        mActivityTestRule.typeInOmnibox(input, false);

        OmniboxTestUtils.waitForOmniboxSuggestions(locationBarLayout);
        // waitForOmniboxSuggestions only wait until one suggestion shows up, we need to wait util
        // autocomplete return more suggestions.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return findFirstHasTabMatchOmniboxSuggestion(locationBarLayout) != null;
            }
        });
    }

    OmniboxSuggestion findFirstHasTabMatchOmniboxSuggestion(LocationBarLayout locationBarLayout) {
        OmniboxSuggestionsDropdown suggestionsDropdown =
                AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                        locationBarLayout.getAutocompleteCoordinator());
        // Find the index of the first matching suggestion.
        for (int i = 0; i < suggestionsDropdown.getItemCount(); ++i) {
            OmniboxSuggestion suggestion = AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                    locationBarLayout.getAutocompleteCoordinator(), i);
            if (suggestion != null && suggestion.hasTabMatch()) {
                return suggestion;
            }
        }
        return null;
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxTabSwitchSuggestions")
    public void testSwitchToTabSuggestion() throws InterruptedException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = testServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = testServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = testServer.getURL("/chrome/test/data/android/test.html");
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl1);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeAndWaitForHasTabMatchSuggestions(locationBarLayout, "about");

        OmniboxSuggestion matchSuggestion =
                findFirstHasTabMatchOmniboxSuggestion(locationBarLayout);

        Assert.assertNotNull("No Switch to Tab suggestion returned.", matchSuggestion);
        Assert.assertEquals(matchSuggestion.getUrl().getSpec(), testHttpsUrl1);
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxTabSwitchSuggestions")
    public void testNoSwitchToIncognitoTabFromNormalModel() throws InterruptedException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = testServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = testServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = testServer.getURL("/chrome/test/data/android/test.html");
        // Open the url trying to match in incognito mode.
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl1, true);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        // trying to match incognito tab.
        mActivityTestRule.typeInOmnibox("about", false);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBarLayout);

        OmniboxSuggestion matchSuggestion =
                findFirstHasTabMatchOmniboxSuggestion(locationBarLayout);

        Assert.assertNull("Should no Switch to Incognito Tab from normal tab.", matchSuggestion);
    }
}
