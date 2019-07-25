// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.widget.Button;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.annotations.JCaller;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Tests for the SmsReceiverDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SmsReceiverDialogTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public JniMocker mocker = new JniMocker();

    private ActivityWindowAndroid mWindowAndroid;
    private SmsReceiverDialog mSmsDialog;

    final private CallbackHelper mCancelButtonClickedCallback = new CallbackHelper();
    final private CallbackHelper mContinueButtonClickedCallback = new CallbackHelper();

    private class TestSmsReceiverDialogJni implements SmsReceiverDialog.Natives {
        @Override
        public void onCancel(@JCaller SmsReceiverDialog self, long nativeSmsDialogAndroid) {
            mCancelButtonClickedCallback.notifyCalled();
        }

        @Override
        public void onContinue(@JCaller SmsReceiverDialog self, long nativeSmsDialogAndroid) {
            mContinueButtonClickedCallback.notifyCalled();
        }
    }

    @Before
    public void setUp() throws Exception {
        mocker.mock(SmsReceiverDialogJni.TEST_HOOKS, new TestSmsReceiverDialogJni());
        mActivityTestRule.startMainActivityOnBlankPage();
        mSmsDialog = createDialog();
    }

    private SmsReceiverDialog createDialog() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            SmsReceiverDialog dialog = SmsReceiverDialog.create(/*nativeSmsDialogAndroid=*/42);
            mWindowAndroid = new ActivityWindowAndroid(mActivityTestRule.getActivity());
            dialog.open(mWindowAndroid);
            return dialog;
        });
    }

    @Test
    @LargeTest
    public void testCancelButtonAndContinueButton() {
        ProgressDialog dialog = mSmsDialog.getDialogForTesting();
        Button cancelButton = dialog.getButton(DialogInterface.BUTTON_NEGATIVE);
        Assert.assertTrue(cancelButton.isEnabled());

        Button continueButton = dialog.getButton(DialogInterface.BUTTON_POSITIVE);
        Assert.assertFalse(continueButton.isEnabled());

        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::enableContinueButton);
        Assert.assertTrue(continueButton.isEnabled());
    }

    @Test
    @LargeTest
    public void testClickCancelButton() throws Throwable {
        ProgressDialog dialog = mSmsDialog.getDialogForTesting();
        Button cancelButton = dialog.getButton(DialogInterface.BUTTON_NEGATIVE);

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), cancelButton);

        mCancelButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testClickContinueButton() throws Throwable {
        ProgressDialog dialog = mSmsDialog.getDialogForTesting();
        Button continueButton = dialog.getButton(DialogInterface.BUTTON_POSITIVE);

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), continueButton);

        mContinueButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testDialogClose() {
        ProgressDialog dialog = mSmsDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::close);
        Assert.assertFalse(dialog.isShowing());
    }

    @Test
    @LargeTest
    public void testDialogDestroyed() {
        Assert.assertFalse(mSmsDialog.isDialogDestroyedForTesting());

        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::destroy);

        Assert.assertTrue(mSmsDialog.isDialogDestroyedForTesting());
    }
}
