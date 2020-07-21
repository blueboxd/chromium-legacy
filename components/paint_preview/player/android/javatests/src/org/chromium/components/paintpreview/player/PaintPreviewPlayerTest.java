// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.url.GURL;

/**
 * Instrumentation tests for the Paint Preview player.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaintPreviewPlayerTest extends DummyUiActivityTestCase {
    private static final long TIMEOUT_MS = ScalableTimeout.scaleTimeout(5000);

    private static final String TEST_DIRECTORY_KEY = "test_dir";
    private static final String TEST_URL = "https://www.chromium.org";
    private static final String TEST_IN_VIEWPORT_LINK_URL = "http://www.google.com/";
    private static final String TEST_OUT_OF_VIEWPORT_LINK_URL = "http://example.com/";
    private final Rect mInViewportLinkRect = new Rect(700, 650, 900, 700);
    private final Rect mOutOfViewportLinkRect = new Rect(300, 4900, 450, 5000);

    private static final int TEST_PAGE_WIDTH = 1082;
    private static final int TEST_PAGE_HEIGHT = 5019;

    @Rule
    public PaintPreviewTestRule mPaintPreviewTestRule = new PaintPreviewTestRule();

    @Rule
    public TemporaryFolder mTempFolder = new TemporaryFolder();

    private PlayerManager mPlayerManager;
    private TestLinkClickHandler mLinkClickHandler;
    private CallbackHelper mRefreshedCallback;

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
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activityContentView.getWidth(), Matchers.greaterThan(0));
            Criteria.checkThat(activityContentView.getHeight(), Matchers.greaterThan(0));
            Criteria.checkThat(
                    activityContentView.getWidth(), Matchers.is(playerHostView.getWidth()));
            Criteria.checkThat(
                    activityContentView.getHeight(), Matchers.is(playerHostView.getHeight()));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Tests that link clicks in the player work correctly.
     */
    @Test
    @MediumTest
    public void linkClickTest() {
        initPlayerManager();
        final View playerHostView = mPlayerManager.getView();

        // Click on a link that is visible in the default viewport.
        assertLinkUrl(playerHostView, 720, 670, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 880, 675, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 800, 680, TEST_IN_VIEWPORT_LINK_URL);

        // Scroll to the bottom, and click on a link.
        scrollToBottom();
        assertLinkUrl(playerHostView, 320, 4920, TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 375, 4950, TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 430, 4980, TEST_OUT_OF_VIEWPORT_LINK_URL);
    }

    @Test
    @MediumTest
    public void overscrollRefreshTest() throws Exception {
        initPlayerManager();
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        int deviceHeight = uiDevice.getDisplayHeight();
        int statusBarHeight = statusBarHeight();
        int navigationBarHeight = navigationBarHeight();
        int padding = 20;
        int toY = deviceHeight - navigationBarHeight - padding;
        int fromY = statusBarHeight + padding;
        uiDevice.swipe(50, fromY, 50, toY, 5);

        mRefreshedCallback.waitForFirst();
    }

    /**
     * Tests that an initialization failure is reported properly.
     */
    @Test
    @MediumTest
    public void initializationCallbackErrorReported() throws Exception {
        CallbackHelper compositorErrorCallback = new CallbackHelper();
        mLinkClickHandler = new TestLinkClickHandler();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(mTempFolder.getRoot().getPath());
            // Use the wrong URL to simulate a failure.
            mPlayerManager = new PlayerManager(new GURL("about:blank"), getActivity(), service,
                    TEST_DIRECTORY_KEY, mLinkClickHandler,
                    () -> { Assert.fail("Unexpected overscroll refresh attempted."); },
                    () -> {
                        Assert.fail("View Ready callback occurred, but expected a failure.");
                    }, null,
                    0xffffffff, () -> { compositorErrorCallback.notifyCalled(); }, false);
            mPlayerManager.setCompressOnClose(false);
        });
        compositorErrorCallback.waitForFirst();
    }

    private int statusBarHeight() {
        Rect visibleContentRect = new Rect();
        getActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleContentRect);
        return visibleContentRect.top;
    }

    private int navigationBarHeight() {
        int navigationBarHeight = 100;
        int resourceId = getActivity().getResources().getIdentifier(
                "navigation_bar_height", "dimen", "android");
        if (resourceId > 0) {
            navigationBarHeight = getActivity().getResources().getDimensionPixelSize(resourceId);
        }
        return navigationBarHeight;
    }

    /**
     * Scrolls to the bottom fo the paint preview.
     */
    private void scrollToBottom() {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        int deviceHeight = uiDevice.getDisplayHeight();

        int statusBarHeight = statusBarHeight();
        int navigationBarHeight = navigationBarHeight();

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
        mRefreshedCallback = new CallbackHelper();
        CallbackHelper viewReady = new CallbackHelper();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(mTempFolder.getRoot().getPath());
            Assert.assertTrue(service.createSingleSkpForKey(TEST_DIRECTORY_KEY, TEST_URL,
                    TEST_PAGE_WIDTH, TEST_PAGE_HEIGHT,
                    new Rect[] {mInViewportLinkRect, mOutOfViewportLinkRect},
                    new String[] {TEST_IN_VIEWPORT_LINK_URL, TEST_OUT_OF_VIEWPORT_LINK_URL}));
            mPlayerManager = new PlayerManager(new GURL(TEST_URL), getActivity(), service,
                    TEST_DIRECTORY_KEY, mLinkClickHandler,
                    () -> { mRefreshedCallback.notifyCalled(); },
                    () -> { viewReady.notifyCalled(); }, null,
                    0xffffffff, () -> { Assert.fail("Compositor initialization failed."); },
                    false);
            mPlayerManager.setCompressOnClose(false);
            getActivity().setContentView(mPlayerManager.getView());
        });

        // Wait until PlayerManager is initialized.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    "PlayerManager was not initialized.", mPlayerManager, Matchers.notNullValue());
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        try {
            viewReady.waitForFirst();
        } catch (Exception e) {
            Assert.fail("View ready was not called.");
        }

        // Assert that the player view is added to the player host view.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Player view is not added to the host view.",
                    ((ViewGroup) mPlayerManager.getView()).getChildCount(),
                    Matchers.greaterThan(0));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
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

        CriteriaHelper.pollUiThread(() -> {
            GURL url = mLinkClickHandler.mUrl;
            String msg = "Link press on abs (" + x + ", " + y + ") failed.";
            Criteria.checkThat(msg, url, Matchers.notNullValue());
            Criteria.checkThat(msg, url.getSpec(), Matchers.is(expectedUrl));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
