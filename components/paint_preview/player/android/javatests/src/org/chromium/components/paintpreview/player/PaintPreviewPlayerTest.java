// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
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
    @DisabledTest(message = "crbug.com/1065441")
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
        int playerHeight = playerHostView.getHeight();
        assertLinkUrl(playerHostView, 322, playerHeight - (TEST_PAGE_HEIGHT - 4946),
                TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 376, playerHeight - (TEST_PAGE_HEIGHT - 4954),
                TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 422, playerHeight - (TEST_PAGE_HEIGHT - 4965),
                TEST_OUT_OF_VIEWPORT_LINK_URL);
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
        while (viewPortBottom < TEST_PAGE_HEIGHT) {
            int fromY = deviceHeight - navigationBarHeight - padding;
            int toY = statusBarHeight + padding;
            uiDevice.swipe(50, fromY, 50, toY, swipeSteps);
            viewPortBottom += fromY - toY;
        }
    }

    private void initPlayerManager() {
        mLinkClickHandler = new TestLinkClickHandler();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(UrlUtils.getIsolatedTestFilePath(TEST_DATA_DIR));
            mPlayerManager = new PlayerManager(new GURL(TEST_URL), getActivity(), service,
                    TEST_DIRECTORY_KEY, mLinkClickHandler, Assert::assertTrue);
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

    private void assertLinkUrl(View view, int x, int y, String expectedUrl) {
        mLinkClickHandler.mUrl = null;
        dispatchTapEvent(view, x, y);
        CriteriaHelper.pollUiThread(
                ()-> {
                    GURL url = mLinkClickHandler.mUrl;
                    if (url == null) return false;

                    return url.getSpec().equals(expectedUrl);
                },
                "Link press on (" + x + ", " + y + ") failed. Expected: " + expectedUrl
                        + ", found: "
                        + (mLinkClickHandler.mUrl == null ? null
                                                          : mLinkClickHandler.mUrl.getSpec()),
                TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void dispatchTapEvent(View view, int x, int y) {
        long downTime = SystemClock.uptimeMillis();
        MotionEvent downEvent = MotionEvent.obtain(
                downTime, downTime + 100, MotionEvent.ACTION_DOWN, (float) x, (float) y, 0);
        MotionEvent upEvent = MotionEvent.obtain(
                downTime + 150, downTime + 200, MotionEvent.ACTION_UP, (float) x, (float) y, 0);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            view.dispatchTouchEvent(downEvent);
            view.dispatchTouchEvent(upEvent);
        });
    }
}
