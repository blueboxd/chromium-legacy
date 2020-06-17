// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.DatePicker;
import android.widget.TimePicker;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogProperties.State;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Test to verify download date time picker.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadDateTimePickerDialogTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private DownloadDateTimePickerDialogCoordinator mDialog;
    private PropertyModel mModel;

    @Mock
    private DownloadDateTimePickerDialogCoordinator.Controller mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            long now = System.currentTimeMillis();
            mModel = new PropertyModel.Builder(DownloadDateTimePickerDialogProperties.ALL_KEYS)
                             .with(DownloadDateTimePickerDialogProperties.STATE, State.DATE)
                             .with(DownloadDateTimePickerDialogProperties.INITIAL_TIME, now)
                             .with(DownloadDateTimePickerDialogProperties.MIN_TIME, now)
                             .with(DownloadDateTimePickerDialogProperties.MAX_TIME,
                                     now + DownloadDateTimePickerDialogCoordinator.MAX_TIME)
                             .build();
            mDialog = new DownloadDateTimePickerDialogCoordinator();
            Assert.assertNotNull(mController);
            mDialog.initialize(mController);
        });
    }

    private void showDialog() {
        mDialog.showDialog(mActivityTestRule.getActivity(), getModalDialogManager(), mModel);
    }

    private ModalDialogManager getModalDialogManager() {
        return mActivityTestRule.getActivity().getModalDialogManager();
    }

    private void clickButton(@ButtonType int type) {
        PropertyModel modalDialogModel = getModalDialogManager().getCurrentDialogForTest();
        modalDialogModel.get(ModalDialogProperties.CONTROLLER).onClick(modalDialogModel, type);
    }

    /**
     * Returns the {@link DownloadDateTimePickerView}. The date time picker dialog must be showing.
     */
    private DownloadDateTimePickerView getView() {
        PropertyModel modalDialogModel = getModalDialogManager().getCurrentDialogForTest();
        return (DownloadDateTimePickerView) (modalDialogModel.get(
                ModalDialogProperties.CUSTOM_VIEW));
    }

    private DatePicker getDatePicker() {
        return (DatePicker) (getView().findViewById(R.id.date_picker));
    }

    private TimePicker getTimePicker() {
        return (TimePicker) (getView().findViewById(R.id.time_picker));
    }

    @Test
    @MediumTest
    public void testSelectDateAndTimeThenDestroy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();
            Assert.assertTrue(getModalDialogManager().isShowing());
            Assert.assertEquals(View.VISIBLE, getDatePicker().getVisibility());
            Assert.assertEquals(View.GONE, getTimePicker().getVisibility());

            // Pass through the date picker.
            clickButton(ButtonType.POSITIVE);

            Assert.assertEquals(View.GONE, getDatePicker().getVisibility());
            Assert.assertEquals(View.VISIBLE, getTimePicker().getVisibility());

            // Pass through the time picker.
            clickButton(ButtonType.POSITIVE);

            mDialog.destroy();

            // TODO(xingliu): Verify the timestamp.
            verify(mController).onDateTimePicked(anyLong());
            verify(mController, times(0)).onDateTimePickerCanceled();
        });
    }

    @Test
    @MediumTest
    public void testCancelDateSelection() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();

            // Cancel the date picker.
            clickButton(ButtonType.NEGATIVE);
            Assert.assertFalse(getModalDialogManager().isShowing());

            verify(mController, times(0)).onDateTimePicked(anyLong());
            verify(mController).onDateTimePickerCanceled();
        });
    }

    @Test
    @MediumTest
    public void testCancelTimeSelection() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();

            // Pass through the date picker.
            clickButton(ButtonType.POSITIVE);
            Assert.assertTrue(getModalDialogManager().isShowing());

            // Cancel the time picker.
            clickButton(ButtonType.NEGATIVE);
            Assert.assertFalse(getModalDialogManager().isShowing());

            verify(mController, times(0)).onDateTimePicked(anyLong());
            verify(mController).onDateTimePickerCanceled();
        });
    }

    @Test
    @MediumTest
    public void testShowDialogThenDestroy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();
            Assert.assertTrue(getModalDialogManager().isShowing());
            mDialog.destroy();
            verify(mController, times(0)).onDateTimePicked(anyLong());
            verify(mController, times(0)).onDateTimePickerCanceled();
        });
    }
}
