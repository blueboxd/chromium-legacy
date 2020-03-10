// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.TabbedModeTabDelegateFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Tab-related histogram collection.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabUmaTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestUrl = mTestServer.getURL(TEST_PATH);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private TabbedModeTabDelegateFactory createTabDelegateFactory() {
        BrowserControlsVisibilityDelegate visibilityDelegate =
                new BrowserControlsVisibilityDelegate(BrowserControlsState.BOTH) {};
        return new TabbedModeTabDelegateFactory(mActivityTestRule.getActivity(), visibilityDelegate,
                new ObservableSupplierImpl<ShareDelegate>());
    }

    /**
     * Verify that Tab.StatusWhenSwitchedBackToForeground is correctly recording lazy loads.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabStatusWhenSwitchedToLazyLoads() throws ExecutionException {
        final Tab tab = TestThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = TabBuilder.createForLazyLoad(new LoadUrlParams(mTestUrl))
                                    .setWindow(mActivityTestRule.getActivity().getWindowAndroid())
                                    .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                                    .setDelegateFactory(createTabDelegateFactory())
                                    .setInitiallyHidden(true)
                                    .build();
                return bgTab;
            }
        });

        String histogram = "Tab.StatusWhenSwitchedBackToForeground";
        HistogramDelta lazyLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_STATUS_LAZY_LOAD_FOR_BG_TAB);
        Assert.assertEquals(0, lazyLoadCount.getDelta()); // Sanity check.

        // Show the tab and verify that one sample was recorded in the lazy load bucket.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.show(TabSelectionType.FROM_USER); });
        Assert.assertEquals(1, lazyLoadCount.getDelta());

        // Show the tab again and verify that we didn't record another sample.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.show(TabSelectionType.FROM_USER); });
        Assert.assertEquals(1, lazyLoadCount.getDelta());
    }
}
