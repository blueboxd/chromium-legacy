// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

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
        CloseTabNewTabCallbackImpl callback = new CloseTabNewTabCallbackImpl();
        // Verify that calling dispatchBeforeUnloadAndClose will close the tab asynchronously when
        // there is no beforeunload handler.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
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
        CloseTabNewTabCallbackImpl callback = new CloseTabNewTabCallbackImpl();
        // Verify that beforeunload is not run when there's no user action.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            tab.dispatchBeforeUnloadAndClose();
            return callback.hasClosed();
        }));

        callback.waitForCloseTab();
    }
}
