// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Test to verify download later dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLaterDialogTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private DownloadLaterDialogCoordinator mDialogCoordinator;
    private PropertyModel mModel;

    @Mock
    private DownloadLaterDialogController mController;

    private ModalDialogManager getModalDialogManager() {
        return mActivityTestRule.getActivity().getModalDialogManager();
    }

    private DownloadLaterDialogView getDownloadLaterDialogView() {
        return (DownloadLaterDialogView) getModalDialogManager().getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                             .with(DownloadLaterDialogProperties.CONTROLLER, mDialogCoordinator)
                             .with(DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION,
                                     DownloadLaterDialogChoice.ON_WIFI)
                             .with(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION,
                                     DownloadLaterPromptStatus.SHOW_INITIAL)
                             .build();
            mDialogCoordinator = new DownloadLaterDialogCoordinator();
            Assert.assertNotNull(mController);
            mDialogCoordinator.initialize(mController);
        });
    }

    private void showDialog() {
        mDialogCoordinator.showDialog(
                mActivityTestRule.getActivity(), getModalDialogManager(), mModel);
    }

    private void clickPositiveButton() {
        PropertyModel modalDialogModel = getModalDialogManager().getCurrentDialogForTest();
        modalDialogModel.get(ModalDialogProperties.CONTROLLER)
                .onClick(modalDialogModel, ButtonType.POSITIVE);
    }

    @Test
    @MediumTest
    public void testShowDialogThenDismiss() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();
            Assert.assertTrue(mActivityTestRule.getActivity().getModalDialogManager().isShowing());
            mDialogCoordinator.dismissDialog(DialogDismissalCause.UNKNOWN);
            Assert.assertFalse(getModalDialogManager().isShowing());
            verify(mController).onDownloadLaterDialogCanceled();
        });
    }

    @Test
    @MediumTest
    public void testShowDialogThenDestroy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();
            Assert.assertTrue(getModalDialogManager().isShowing());
            mDialogCoordinator.destroy();
        });
    }

    @Test
    @MediumTest
    public void testDestroyWithoutShowDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mDialogCoordinator.destroy(); });
    }

    @Test
    @MediumTest
    public void testSelectRadioButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();

            // Verify the initial selection of the dialog. The controller should not get an event
            // for the initial setup.
            RadioButtonWithDescription onWifiButton =
                    (RadioButtonWithDescription) getDownloadLaterDialogView().findViewById(
                            org.chromium.chrome.browser.download.R.id.on_wifi);
            Assert.assertTrue(onWifiButton.isChecked());

            // Simulate a click on another radio button, the event should be propagated to
            // controller.
            RadioButtonWithDescription downloadNowButton =
                    getDownloadLaterDialogView().findViewById(R.id.download_now);
            Assert.assertNotNull(downloadNowButton);
            downloadNowButton.setChecked(true);
            getDownloadLaterDialogView().onCheckedChanged(null, -1);
            clickPositiveButton();
            verify(mController)
                    .onDownloadLaterDialogComplete(eq(DownloadLaterDialogChoice.DOWNLOAD_NOW),
                            eq(DownloadLaterPromptStatus.DONT_SHOW));
        });
    }
}
