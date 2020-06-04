// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;

/**
 * Test to verify download later dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLaterDialogTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private DownloadLaterDialogCoordinator mDialogCoordinator;

    @Mock
    private ModalDialogProperties.Controller mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull("controller should not be null", mController);
            mDialogCoordinator = new DownloadLaterDialogCoordinator(mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getModalDialogManager(), mController);
        });
    }

    @Test
    @MediumTest
    public void testShowDialogThenDismiss() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogCoordinator.showDialog();
            Assert.assertTrue(mActivityTestRule.getActivity().getModalDialogManager().isShowing());
            mDialogCoordinator.dismissDialog(DialogDismissalCause.UNKNOWN);
            Assert.assertFalse(mActivityTestRule.getActivity().getModalDialogManager().isShowing());
        });
    }

    @Test
    @MediumTest
    public void testShowDialogThenDestroy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogCoordinator.showDialog();
            Assert.assertTrue(mActivityTestRule.getActivity().getModalDialogManager().isShowing());
            mDialogCoordinator.destroy();
        });
    }
}
