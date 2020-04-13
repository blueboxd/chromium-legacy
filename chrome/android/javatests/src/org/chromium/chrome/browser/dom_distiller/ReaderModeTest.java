// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.notNullValue;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Objects;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for Reader Mode (Simplified view).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ReaderModeTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";
    private static final String TITLE = "Test Page Title";
    private static final String CONTENT = "Lorem ipsum";

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws InterruptedException, TimeoutException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void testReaderModeInfobarShown() {
        waitForReaderModeInfobar();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInCCT() throws TimeoutException {
        Tab originalTab = mActivityTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> mActivityTestRule.getActivity()
                                                                 .getReaderModeManager()
                                                                 .activateReaderMode(originalTab));
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                Criteria.checkThat(customTabActivity::getActivityTab, notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(TITLE, distillerViewerTab);

        innerHtml = getInnerHtml(distillerViewerTab);
        assertThat(innerHtml).contains("article-header");
        assertThat(innerHtml).contains(CONTENT);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInTab() throws TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).doesNotContain("article-header");

        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> mActivityTestRule.getActivity()
                                                                 .getReaderModeManager()
                                                                 .activateReaderMode(tab));
        waitForDistillation(TITLE, mActivityTestRule.getActivity().getActivityTab());

        innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).contains("article-header");
        assertThat(innerHtml).contains(CONTENT);
    }

    /**
     * Wait until a {@link CustomTabActivity} shows up, and return it.
     * @return a {@link CustomTabActivity}
     */
    private CustomTabActivity waitForCustomTabActivity() {
        AtomicReference<CustomTabActivity> activity = new AtomicReference<>();
        CriteriaHelper.pollUiThread(() -> {
            for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                if (runningActivity instanceof CustomTabActivity) {
                    activity.set((CustomTabActivity) runningActivity);
                    return true;
                }
            }
            return false;
        });
        return activity.get();
    }

    /**
     * @param tab The tab to be inspected.
     * @return The inner HTML of a certain {@link Tab}.
     */
    private String getInnerHtml(Tab tab) throws TimeoutException {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(
                tab.getWebContents(), "document.body.innerHTML");
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }

    /**
     * Wait until a {@link ReaderModeInfoBar} shows up.
     */
    private void waitForReaderModeInfobar() {
        CriteriaHelper.pollUiThread(() -> {
            for (InfoBar infobar : mActivityTestRule.getInfoBars()) {
                if (infobar instanceof ReaderModeInfoBar) return true;
            }
            return false;
        });
    }

    /**
     * Wait until the distilled content is shown on the {@link Tab}.
     * @param expectedTitle The expected title of the distilled content
     * @param tab the tab to wait
     */
    private void waitForDistillation(
            @SuppressWarnings("SameParameterValue") String expectedTitle, Tab tab) {
        CriteriaHelper.pollUiThread(
                Criteria.equals("chrome-distiller", () -> tab.getUrl().getScheme()));
        ChromeTabUtils.waitForTabPageLoaded(tab, null);
        // Distiller Viewer load the content dynamically, so waitForTabPageLoaded() is not enough.
        CriteriaHelper.pollUiThread(Criteria.equals(expectedTitle, tab::getTitle));
    }
}
