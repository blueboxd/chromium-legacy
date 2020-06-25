// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListenerWithScroll;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * Assertions for GestureListenerManager.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class GestureListenerManagerTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    // The page should be large enough so that scrolling occurs.
    private static final String TEST_URL =
            UrlUtils.encodeHtmlDataUri("<html><body style='height: 10000px'>");

    private static final class GestureStateListenerImpl implements GestureStateListenerWithScroll {
        private int mNumOnScrollOffsetOrExtentChangedCalls;
        public CallbackHelper mCallbackHelper = new CallbackHelper();
        private boolean mGotStarted;
        private boolean mDidScrollOffsetChangeWhileScrolling;
        private Integer mLastScrollOffsetY;

        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
            org.chromium.base.Log.e("chrome", "!!!onScrollStarted " + scrollOffsetY);
            mGotStarted = true;
            mLastScrollOffsetY = null;
        }
        @Override
        public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
            org.chromium.base.Log.e("chrome",
                    "!!!onScrollOffsetOrExtentChanged started=" + mGotStarted
                            + " scroll=" + scrollOffsetY + " last=" + mLastScrollOffsetY);
            if (mGotStarted
                    && (mLastScrollOffsetY == null || !mLastScrollOffsetY.equals(scrollOffsetY))) {
                if (mLastScrollOffsetY != null) mDidScrollOffsetChangeWhileScrolling = true;
                mLastScrollOffsetY = scrollOffsetY;
            }
        }
        @Override
        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
            org.chromium.base.Log.e("chrome", "!!!onScrollEnded, offset=" + scrollOffsetY);
            // onScrollEnded() should be preceded by onScrollStarted().
            Assert.assertTrue(mGotStarted);
            // onScrollOffsetOrExtentChanged() should be called at least twice. Once with an initial
            // value, and then with a different value.
            Assert.assertTrue(mDidScrollOffsetChangeWhileScrolling);
            mCallbackHelper.notifyCalled();
            mGotStarted = false;
        }
    }

    private float mCurrentX;
    private float mCurrentY;

    /**
     * Assertions for GestureStateListener.onScrollOffsetOrExtentChanged.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    public void testOnScrollOffsetOrExtentChanged() throws Throwable {
        mActivityTestRule.launchContentShellWithUrl("about:blank");
        WebContents webContents = mActivityTestRule.getWebContents();
        // This needs to wait for first-paint, otherwise scrolling doesn't happen.
        TestCallbackHelperContainer callbackHelperContainer =
                new TestCallbackHelperContainer(webContents);
        CallbackHelper done = callbackHelperContainer.getOnFirstVisuallyNonEmptyPaintHelper();
        mActivityTestRule.loadUrl(webContents.getNavigationController(), callbackHelperContainer,
                new LoadUrlParams(TEST_URL));
        done.waitForCallback(0);

        final GestureStateListenerImpl listener = new GestureStateListenerImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            GestureListenerManagerImpl manager =
                    (GestureListenerManagerImpl) GestureListenerManager.fromWebContents(
                            webContents);
            // shouldReportAllRootScrolls() should initially be false (as there are no listeners).
            Assert.assertFalse(manager.shouldReportAllRootScrolls());
            manager.addListener(listener);
            // Adding a listener enables root-scrolls.
            Assert.assertTrue(manager.shouldReportAllRootScrolls());
            View webContentsView = webContents.getViewAndroidDelegate().getContainerView();
            mCurrentX = webContentsView.getWidth() / 2;
            mCurrentY = webContentsView.getHeight() / 2;
            Assert.assertTrue(mCurrentY > 0);
        });

        // Perform a scroll.
        TouchCommon.performDrag(mActivityTestRule.getActivity(), mCurrentX, mCurrentX, mCurrentY,
                mCurrentY - 50, /* stepCount*/ 3, /* duration in ms */ 250);
        listener.mCallbackHelper.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            GestureListenerManagerImpl manager =
                    (GestureListenerManagerImpl) GestureListenerManager.fromWebContents(
                            webContents);
            manager.removeListener(listener);
            // Removing the only listener disbles root-scrolls.
            Assert.assertFalse(manager.shouldReportAllRootScrolls());
        });
    }
}
