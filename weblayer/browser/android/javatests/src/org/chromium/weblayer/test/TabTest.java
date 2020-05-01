// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.concurrent.TimeoutException;

/**
 * Tests for Tab.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TabTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Test
    @SmallTest
    public void testBeforeUnload() {
        String url = mActivityTestRule.getTestDataURL("before_unload.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        Assert.assertTrue(mActivity.hasWindowFocus());

        // Touch the view so that beforeunload will be respected (if the user doesn't interact with
        // the tab, it's ignored).
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        // Round trip through the renderer to make sure te above touch is handled before we call
        // dispatchBeforeUnloadAndClose().
        mActivityTestRule.executeScriptSync("var x = 1", true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().dispatchBeforeUnloadAndClose(); });

        // Wait till the main window loses focus due to the app modal beforeunload dialog.
        BoundedCountDownLatch noFocusLatch = new BoundedCountDownLatch(1);
        BoundedCountDownLatch hasFocusLatch = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getWindow()
                    .getDecorView()
                    .getViewTreeObserver()
                    .addOnWindowFocusChangeListener((boolean hasFocus) -> {
                        (hasFocus ? hasFocusLatch : noFocusLatch).countDown();
                    });
        });
        noFocusLatch.timedAwait();

        // Verify closing the tab works still while beforeunload is showing (no crash).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().destroyTab(mActivity.getBrowser().getActiveTab());
        });

        // Focus returns to the main window because the dialog is dismissed when the tab is
        // destroyed.
        hasFocusLatch.timedAwait();
    }

    @Test
    @SmallTest
    public void testBeforeUnloadNoHandler() {
        String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        OnTabRemovedTabListCallbackImpl callback = new OnTabRemovedTabListCallbackImpl();
        // Verify that calling dispatchBeforeUnloadAndClose will close the tab asynchronously when
        // there is no beforeunload handler.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mActivity.getBrowser().registerTabListCallback(callback);
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.dispatchBeforeUnloadAndClose();
            return callback.hasClosed();
        }));

        callback.waitForCloseTab();
    }

    @Test
    @SmallTest
    public void testBeforeUnloadNoInteraction() {
        String url = mActivityTestRule.getTestDataURL("before_unload.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        OnTabRemovedTabListCallbackImpl callback = new OnTabRemovedTabListCallbackImpl();
        // Verify that beforeunload is not run when there's no user action.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mActivity.getBrowser().registerTabListCallback(callback);
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.dispatchBeforeUnloadAndClose();
            return callback.hasClosed();
        }));

        callback.waitForCloseTab();
    }

    private Bitmap captureScreenShot(float scale) throws TimeoutException {
        Bitmap[] bitmapHolder = new Bitmap[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivity.getTab();
            tab.captureScreenShot(scale, (Bitmap bitmap, int errorCode) -> {
                Assert.assertNotNull(bitmap);
                Assert.assertEquals(0, errorCode);
                bitmapHolder[0] = bitmap;
                // Failure is ok here, so not checking |bitmap| or |errorCode|.
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return bitmapHolder[0];
    }

    private void checkQuadrantColors(Bitmap bitmap) {
        int quarterWidth = bitmap.getWidth() / 4;
        int quarterHeight = bitmap.getHeight() / 4;
        Assert.assertEquals(Color.rgb(255, 0, 0), bitmap.getPixel(quarterWidth, quarterHeight));
        Assert.assertEquals(Color.rgb(0, 255, 0), bitmap.getPixel(quarterWidth * 3, quarterHeight));
        Assert.assertEquals(Color.rgb(0, 0, 255), bitmap.getPixel(quarterWidth, quarterHeight * 3));
        Assert.assertEquals(
                Color.rgb(128, 128, 128), bitmap.getPixel(quarterWidth * 3, quarterHeight * 3));
    }

    @Test
    @SmallTest
    public void testCaptureScreenShot() throws TimeoutException {
        String url = mActivityTestRule.getTestDataURL("quadrant_colors.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);

        Bitmap bitmap = captureScreenShot(1.f);
        checkQuadrantColors(bitmap);
        Bitmap halfBitmap = captureScreenShot(0.5f);
        checkQuadrantColors(bitmap);

        final int allowedError = 10;
        Assert.assertTrue(Math.abs(bitmap.getWidth() / 2 - halfBitmap.getWidth()) < allowedError);
        Assert.assertTrue(Math.abs(bitmap.getHeight() / 2 - halfBitmap.getHeight()) < allowedError);
    }

    @Test
    @SmallTest
    public void testCaptureScreenShotDoesNotHang() throws TimeoutException {
        String startupUrl = "about:blank";
        mActivity = mActivityTestRule.launchShellWithUrl(startupUrl);

        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivity.getTab();
            tab.captureScreenShot(1.f, (Bitmap bitmap, int errorCode) -> {
                // Failure is ok here, so not checking |bitmap| or |errorCode|.
                callbackHelper.notifyCalled();
            });
            mActivity.destroyFragment();
        });
        callbackHelper.waitForFirst();
    }
}
