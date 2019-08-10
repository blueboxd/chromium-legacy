// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.app.Dialog;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
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
    final private CallbackHelper mConfirmButtonClickedCallback = new CallbackHelper();

    private class TestSmsReceiverDialogJni implements SmsReceiverDialog.Natives {
        @Override
        public void onCancel(long nativeSmsDialogAndroid) {
            mCancelButtonClickedCallback.notifyCalled();
        }

        @Override
        public void onConfirm(long nativeSmsDialogAndroid) {
            mConfirmButtonClickedCallback.notifyCalled();
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
            SmsReceiverDialog dialog =
                    SmsReceiverDialog.create(/*nativeSmsDialogAndroid=*/42, "https://example.com");
            mWindowAndroid = new ActivityWindowAndroid(mActivityTestRule.getActivity());
            dialog.open(mWindowAndroid);
            return dialog;
        });
    }

    @Test
    @LargeTest
    public void testCancelButtonAndConfirmButton() {
        Dialog dialog = mSmsDialog.getDialogForTesting();

        Button cancelButton = (Button) dialog.findViewById(R.id.cancel_button);
        Assert.assertTrue(cancelButton.isEnabled());

        Button confirmButton = (Button) dialog.findViewById(R.id.confirm_button);
        Assert.assertFalse(confirmButton.isEnabled());

        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::smsReceived);
        Assert.assertTrue(confirmButton.isEnabled());
    }

    @Test
    @LargeTest
    public void testClickCancelButton() throws Throwable {
        Dialog dialog = mSmsDialog.getDialogForTesting();
        Button cancelButton = (Button) dialog.findViewById(R.id.cancel_button);

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), cancelButton);

        mCancelButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testClickConfirmButton() throws Throwable {
        Dialog dialog = mSmsDialog.getDialogForTesting();
        Button confirmButton = (Button) dialog.findViewById(R.id.confirm_button);

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), confirmButton);

        mConfirmButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testStatusRowChangesWhenMessageReceived() {
        Dialog dialog = mSmsDialog.getDialogForTesting();

        ProgressBar progressBar = (ProgressBar) dialog.findViewById(R.id.progress);
        ImageView doneIcon = (ImageView) dialog.findViewById(R.id.done_icon);
        TextView status = (TextView) dialog.findViewById(R.id.status);

        Assert.assertEquals(View.VISIBLE, progressBar.getVisibility());
        Assert.assertEquals(View.GONE, doneIcon.getVisibility());
        Assert.assertEquals(
                mActivityTestRule.getActivity().getString(R.string.sms_dialog_status_waiting),
                status.getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::smsReceived);

        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        Assert.assertEquals(View.VISIBLE, doneIcon.getVisibility());
        Assert.assertEquals(
                mActivityTestRule.getActivity().getString(R.string.sms_dialog_status_sms_received),
                status.getText().toString());
    }

    @Test
    @LargeTest
    public void testDialogClose() {
        Dialog dialog = mSmsDialog.getDialogForTesting();
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
