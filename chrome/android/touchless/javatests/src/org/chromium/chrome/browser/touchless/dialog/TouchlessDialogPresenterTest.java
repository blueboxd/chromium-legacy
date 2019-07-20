// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.dialog;

import static org.chromium.chrome.browser.touchless.dialog.TouchlessDialogTestUtils.showDialog;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.touchless.NoTouchActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link TouchlessDialogPresenter}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchlessDialogPresenterTest {
    @Rule
    public ChromeActivityTestRule<NoTouchActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(NoTouchActivity.class);
    private NoTouchActivity mActivity;
    private ModalDialogManager mManager;
    private UiDevice mUiDevice;

    private static final int TIMEOUT_MS = 500;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mManager = mActivity.getModalDialogManager();
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    @Test
    @DisabledTest(message = "crbug.com/984004")
    @SmallTest
    @Feature({"TouchlessModalDialog"})
    public void testItemSelection() throws Exception {
        CallbackHelper item1Callback = new CallbackHelper();
        CallbackHelper item2Callback = new CallbackHelper();
        PropertyModel dialog = TouchlessDialogTestUtils.createDialog("dialog", false,
                new String[] {"1", "2"},
                new View.OnClickListener[] {
                        (v) -> item1Callback.notifyCalled(), (v) -> item2Callback.notifyCalled()});

        // Enable 'keyboard mode' by sending a key press event.
        // Doing so, the first item will be focused when the dialog appears.
        mUiDevice.pressDPadDown();
        showDialog(mManager, dialog);
        mUiDevice.wait(Until.findObject(By.text("dialog")), TIMEOUT_MS);

        UiObject2 firstItem = mUiDevice.findObject(By.text("1")).getParent();
        UiObject2 secondItem = mUiDevice.findObject(By.text("2")).getParent();

        Assert.assertTrue("First item is not selected", firstItem.isFocused());
        Assert.assertFalse("Second item is selected", secondItem.isFocused());

        Assert.assertEquals(0, item1Callback.getCallCount());
        mUiDevice.pressDPadCenter();
        item1Callback.waitForCallback(0, 1);

        mUiDevice.pressDPadDown();
        secondItem.wait(Until.focused(true), TIMEOUT_MS);
        Assert.assertFalse("First item is selected", firstItem.isFocused());

        Assert.assertEquals(0, item2Callback.getCallCount());
        mUiDevice.pressDPadCenter();
        item2Callback.waitForCallback(0, 1);
        Assert.assertEquals(1, item1Callback.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"TouchlessModalDialog"})
    public void testFullscreen() throws Exception {
        PropertyModel dialog = TouchlessDialogTestUtils.createDialog("dialog", true);

        Rect visibleRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleRect);

        showDialog(mManager, dialog);
        mUiDevice.wait(Until.findObject(By.text("dialog")), TIMEOUT_MS);
        UiObject2 dialogObject = mUiDevice.findObject(By.text("dialog")).getParent();
        Assert.assertEquals(visibleRect, dialogObject.getVisibleBounds());
    }

    @Test
    @SmallTest
    @Feature({"TouchlessModalDialog"})
    public void testNotFullscreen() throws Exception {
        PropertyModel dialog = TouchlessDialogTestUtils.createDialog("dialog", false);

        Rect visibleRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleRect);

        showDialog(mManager, dialog);
        mUiDevice.wait(Until.findObject(By.text("dialog")), TIMEOUT_MS);
        UiObject2 dialogObject = mUiDevice.findObject(By.text("dialog")).getParent();
        Assert.assertNotEquals(visibleRect, dialogObject.getVisibleBounds());
    }

    @Test
    @SmallTest
    @Feature({"TouchlessModalDialog"})
    public void testDismiss() throws Exception {
        PropertyModel dialog = TouchlessDialogTestUtils.createDialog("dialog");
        UiObject dialogUiObject = mUiDevice.findObject(new UiSelector().text("dialog"));

        showDialog(mManager, dialog);
        dialogUiObject.waitForExists(TIMEOUT_MS);

        mUiDevice.pressBack();
        dialogUiObject.waitUntilGone(TIMEOUT_MS);
    }
}
