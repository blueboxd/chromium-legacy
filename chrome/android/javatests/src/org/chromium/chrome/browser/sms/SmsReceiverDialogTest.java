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
import org.chromium.content_public.browser.sms.Event;
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
    final private CallbackHelper mTryAgainButtonClickedCallback = new CallbackHelper();

    private class TestSmsReceiverDialogJni implements SmsReceiverDialog.Natives {
        @Override
        public void onEvent(long nativeSmsDialogAndroid, int eventType) {
            switch (eventType) {
                case Event.CANCEL:
                    mCancelButtonClickedCallback.notifyCalled();
                    return;
                case Event.CONFIRM:
                    mConfirmButtonClickedCallback.notifyCalled();
                    return;
                case Event.TIMEOUT:
                    mTryAgainButtonClickedCallback.notifyCalled();
                    return;
                default:
                    assert false : "|eventType| is invalid";
            }
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
    public void testSmsCancel() throws Throwable {
        Dialog dialog = mSmsDialog.getDialogForTesting();

        Button cancelButton = (Button) dialog.findViewById(R.id.cancel_button);
        Assert.assertTrue(cancelButton.isEnabled());

        // Simulates the user clicking the "Cancel" button.
        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), cancelButton);
        mCancelButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testSmsReceivedUserClickingCancelButton() throws Throwable {
        Dialog dialog = mSmsDialog.getDialogForTesting();

        ProgressBar progressBar = (ProgressBar) dialog.findViewById(R.id.progress);
        ImageView doneIcon = (ImageView) dialog.findViewById(R.id.done_icon);
        ImageView errorIcon = (ImageView) dialog.findViewById(R.id.error_icon);
        TextView status = (TextView) dialog.findViewById(R.id.status);
        Button cancelButton = (Button) dialog.findViewById(R.id.cancel_button);
        Button confirmButton = (Button) dialog.findViewById(R.id.confirm_or_try_again_button);

        Assert.assertEquals(View.VISIBLE, progressBar.getVisibility());
        Assert.assertEquals(View.GONE, doneIcon.getVisibility());
        Assert.assertEquals(View.GONE, errorIcon.getVisibility());
        Assert.assertEquals(
                mActivityTestRule.getActivity().getString(R.string.sms_dialog_status_waiting),
                status.getText().toString());
        Assert.assertTrue(cancelButton.isEnabled());
        Assert.assertEquals(mActivityTestRule.getActivity().getString(R.string.confirm),
                confirmButton.getText().toString());
        Assert.assertFalse(confirmButton.isEnabled());

        // Simulates the SMS being received.
        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::smsReceived);

        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        Assert.assertEquals(View.VISIBLE, doneIcon.getVisibility());
        Assert.assertEquals(View.GONE, errorIcon.getVisibility());
        Assert.assertEquals(
                mActivityTestRule.getActivity().getString(R.string.sms_dialog_status_sms_received),
                status.getText().toString());
        Assert.assertTrue(cancelButton.isEnabled());
        Assert.assertTrue(confirmButton.isEnabled());

        // Simulates the user clicking the "Cancel" button.
        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), cancelButton);
        mCancelButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testSmsReceivedUserClickingConfirmButton() throws Throwable {
        Dialog dialog = mSmsDialog.getDialogForTesting();

        Button confirmButton = (Button) dialog.findViewById(R.id.confirm_or_try_again_button);

        // Simulates the SMS being received.
        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::smsReceived);

        // Simulates the user clicking the "Confirm" button.
        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), confirmButton);
        mConfirmButtonClickedCallback.waitForCallback(0, 1);
    }

    @Test
    @LargeTest
    public void testSmsTimeout() throws Throwable {
        Dialog dialog = mSmsDialog.getDialogForTesting();

        ProgressBar progressBar = (ProgressBar) dialog.findViewById(R.id.progress);
        ImageView doneIcon = (ImageView) dialog.findViewById(R.id.done_icon);
        ImageView errorIcon = (ImageView) dialog.findViewById(R.id.error_icon);
        TextView status = (TextView) dialog.findViewById(R.id.status);
        Button cancelButton = (Button) dialog.findViewById(R.id.cancel_button);
        Button tryAgainButton = (Button) dialog.findViewById(R.id.confirm_or_try_again_button);

        // Simulates receiving the SMS having timed out.
        TestThreadUtils.runOnUiThreadBlocking(mSmsDialog::smsTimeout);

        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        Assert.assertEquals(View.GONE, doneIcon.getVisibility());
        Assert.assertEquals(View.VISIBLE, errorIcon.getVisibility());
        Assert.assertEquals(
                mActivityTestRule.getActivity().getString(R.string.sms_dialog_status_timeout),
                status.getText().toString());
        Assert.assertEquals(View.GONE, cancelButton.getVisibility());
        Assert.assertEquals(mActivityTestRule.getActivity().getString(R.string.try_again),
                tryAgainButton.getText().toString());
        Assert.assertTrue(tryAgainButton.isEnabled());

        // Simulates the user clicking the "Try again" button.
        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), tryAgainButton);
        mTryAgainButtonClickedCallback.waitForCallback(0, 1);
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
