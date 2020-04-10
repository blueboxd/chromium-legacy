// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previewtab;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.firstrun.DisableFirstRun;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

/**
 * Tests the Preview Tab, also known as the Ephemeral Tab.  Based on the
 * FocusedEditableTextFieldZoomTest and TabsTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures({ChromeFeatureList.REVAMPED_CONTEXT_MENU})
public class PreviewTabTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();

    /** Needed to ensure the First Run Flow is disabled automatically during setUp, etc. */
    @Rule
    public DisableFirstRun mDisableFirstRunFlowRule = new DisableFirstRun();

    private static final String BASE_PAGE = "/chrome/test/data/android/previewtab/base_page.html";
    private static final String PREVIEW_TAB_DOM_ID = "previewTab";
    private static final String NEAR_BOTTOM_DOM_ID = "nearBottom";

    private EphemeralTabCoordinator mEphemeralTabCoordinator;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getServer().getURL(BASE_PAGE));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabbedRootUiCoordinator tabbedRootUiCoordinator =
                    ((TabbedRootUiCoordinator) mActivityTestRule.getActivity()
                                    .getRootUiCoordinatorForTesting());
            mEphemeralTabCoordinator =
                    tabbedRootUiCoordinator.getEphemeralTabCoordinatorForTesting();
        });
    }

    /**
     * Test bringing up the PT, scrolling the base page but never expanding the PT, then closing it.
     */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    public void testOpenAndClose() throws Throwable {
        Assert.assertFalse(
                "Test did not start without a Preview Tab", mEphemeralTabCoordinator.isOpened());

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                activity, tab, PREVIEW_TAB_DOM_ID, R.id.contextmenu_open_in_ephemeral_tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("The Preview Tab did not open", mEphemeralTabCoordinator.isOpened());

        // Scroll the base page.
        DOMUtils.scrollNodeIntoView(tab.getWebContents(), NEAR_BOTTOM_DOM_ID);
        Assert.assertTrue("The Preview Tab did not stay open after a scroll action",
                mEphemeralTabCoordinator.isOpened());

        // Close the PT.
        TestThreadUtils.runOnUiThreadBlocking(() -> mEphemeralTabCoordinator.close());
        // TODO(donnd): check that it's actually closed once https://crbug.com/1068449 is fixed.
    }

    // TODO(donnd): add more tests.
}
