// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.url.GURL;

/**
 * Instrumentation tests for the Paint Preview player.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaintPreviewPlayerTest extends DummyUiActivityTestCase {
    private static final long TIMEOUT_MS = ScalableTimeout.scaleTimeout(5000);

    private static final String TEST_DATA_DIR = "components/test/data/";
    private static final String TEST_DIRECTORY_KEY = "wikipedia";
    private static final String TEST_URL = "https://en.m.wikipedia.org/wiki/Main_Page";
    private static final String TEST_MAIN_PICTURE_LINK_URL =
            "https://en.m.wikipedia.org/wiki/File:Volc%C3%A1n_Ubinas,_Arequipa,_Per%C3%BA,_2015-08-02,_DD_50.JPG";
    private static final String TEST_IN_VIEWPORT_LINK_URL =
            "https://en.m.wikipedia.org/wiki/Arequipa";
    private static final String TEST_OUT_OF_VIEWPORT_LINK_URL =
            "https://foundation.wikimedia.org/wiki/Privacy_policy";

    private static final int TEST_PAGE_WIDTH = 1082;
    private static final int TEST_PAGE_HEIGHT = 5019;

    @Rule
    public PaintPreviewTestRule mPaintPreviewTestRule = new PaintPreviewTestRule();

    private PlayerManager mPlayerManager;
    private TestLinkClickHandler mLinkClickHandler;

    /**
     * LinkClickHandler implementation for caching the last URL that was clicked.
     */
    public class TestLinkClickHandler implements LinkClickHandler {
        GURL mUrl;

        @Override
        public void onLinkClicked(GURL url) {
            mUrl = url;
        }
    }

    @Override
    public void tearDownTest() throws Exception {
        super.tearDownTest();
        CallbackHelper destroyed = new CallbackHelper();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            mPlayerManager.destroy();
            destroyed.notifyCalled();
        });
        destroyed.waitForFirst();
    }

    /**
     * Tests the the player correctly initializes and displays a sample paint preview with 1 frame.
     */
    @Test
    @MediumTest
    public void singleFrameDisplayTest() {
        initPlayerManager();
        final View playerHostView = mPlayerManager.getView();
        final View activityContentView = getActivity().findViewById(android.R.id.content);

        // Assert that the player view has the same dimensions as the content view.
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean contentSizeNonZero = activityContentView.getWidth() > 0
                            && activityContentView.getHeight() > 0;
                    boolean viewSizeMatchContent =
                            activityContentView.getWidth() == playerHostView.getWidth()
                            && activityContentView.getHeight() == playerHostView.getHeight();
                    return contentSizeNonZero && viewSizeMatchContent;
                },
                "Player size doesn't match R.id.content", TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Tests that link clicks in the player work correctly.
     */
    @Test
    @MediumTest
    public void linkClickTest() {
        initPlayerManager();
        final View playerHostView = mPlayerManager.getView();

        // Click on the top left picture and assert it directs to the correct link.
        assertLinkUrl(playerHostView, 92, 424, TEST_MAIN_PICTURE_LINK_URL);
        assertLinkUrl(playerHostView, 67, 527, TEST_MAIN_PICTURE_LINK_URL);
        assertLinkUrl(playerHostView, 466, 668, TEST_MAIN_PICTURE_LINK_URL);
        assertLinkUrl(playerHostView, 412, 432, TEST_MAIN_PICTURE_LINK_URL);

        // Click on a link that is visible in the default viewport.
        assertLinkUrl(playerHostView, 732, 698, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 876, 716, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 798, 711, TEST_IN_VIEWPORT_LINK_URL);

        // Scroll to the bottom, and click on a link.
        scrollToBottom();
        assertLinkUrl(playerHostView, 322, 4946, TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 376, 4954, TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 422, 4965, TEST_OUT_OF_VIEWPORT_LINK_URL);
    }

    /**
     * Scrolls to the bottom fo the paint preview.
     */
    private void scrollToBottom() {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        int deviceHeight = uiDevice.getDisplayHeight();

        Rect visibleContentRect = new Rect();
        getActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleContentRect);
        int statusBarHeight = visibleContentRect.top;

        int navigationBarHeight = 100;
        int resourceId = getActivity().getResources().getIdentifier(
                "navigation_bar_height", "dimen", "android");
        if (resourceId > 0) {
            navigationBarHeight = getActivity().getResources().getDimensionPixelSize(resourceId);
        }

        int padding = 20;
        int swipeSteps = 5;
        int viewPortBottom = deviceHeight - statusBarHeight - navigationBarHeight;
        int fromY = deviceHeight - navigationBarHeight - padding;
        int toY = statusBarHeight + padding;
        int delta = fromY - toY;
        while (viewPortBottom < scaleAbsoluteCoordinateToViewCoordinate(TEST_PAGE_HEIGHT)) {
            uiDevice.swipe(50, fromY, 50, toY, swipeSteps);
            viewPortBottom += delta;
        }
        // Repeat an addition time to avoid flakiness.
        uiDevice.swipe(50, fromY, 50, toY, swipeSteps);
    }

    private void initPlayerManager() {
        mLinkClickHandler = new TestLinkClickHandler();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(UrlUtils.getIsolatedTestFilePath(TEST_DATA_DIR));
            mPlayerManager = new PlayerManager(new GURL(TEST_URL), getActivity(), service,
                    TEST_DIRECTORY_KEY, mLinkClickHandler, Assert::assertTrue, 0xffffffff);
            getActivity().setContentView(mPlayerManager.getView());
        });

        // Wait until PlayerManager is initialized.
        CriteriaHelper.pollUiThread(() -> mPlayerManager != null,
                "PlayerManager was not initialized.", TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // Assert that the player view is added to the player host view.
        CriteriaHelper.pollUiThread(
                () -> ((ViewGroup) mPlayerManager.getView()).getChildCount() > 0,
                "Player view is not added to the host view.", TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /*
     * Scales the provided coordinate to be view relative
     */
    private int scaleAbsoluteCoordinateToViewCoordinate(int coordinate) {
        float scaleFactor = (float) mPlayerManager.getView().getWidth() / (float) TEST_PAGE_WIDTH;
        return Math.round((float) coordinate * scaleFactor);
    }

    /*
     * Asserts that the expectedUrl is found in the view at absolute coordinates x and y.
     */
    private void assertLinkUrl(View view, int x, int y, String expectedUrl) {
        int scaledX = scaleAbsoluteCoordinateToViewCoordinate(x);
        int scaledY = scaleAbsoluteCoordinateToViewCoordinate(y);

        // In this test scaledY will only exceed the view height if scrolled to the bottom of a
        // page.
        if (scaledY > view.getHeight()) {
            scaledY = view.getHeight()
                    - (scaleAbsoluteCoordinateToViewCoordinate(TEST_PAGE_HEIGHT) - scaledY);
        }

        mLinkClickHandler.mUrl = null;

        int[] locationXY = new int[2];
        view.getLocationOnScreen(locationXY);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.click(scaledX + locationXY[0], scaledY + locationXY[1]);

        CriteriaHelper.pollUiThread(
                ()
                        -> {
                    GURL url = mLinkClickHandler.mUrl;
                    if (url == null) return false;

                    return url.getSpec().equals(expectedUrl);
                },
                "Link press on abs (" + x + ", " + y + ") failed. Expected: " + expectedUrl
                        + ", found: "
                        + (mLinkClickHandler.mUrl == null ? null
                                                          : mLinkClickHandler.mUrl.getSpec()),
                TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
