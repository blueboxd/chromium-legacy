// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Test to verify download later dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLaterDialogTest {
    private static final long INVALID_START_TIME = -1;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private DownloadLaterDialogCoordinator mDialogCoordinator;
    private PropertyModel mModel;

    @Mock
    private DownloadLaterDialogController mController;

    @Mock
    DownloadDateTimePickerDialogCoordinator mDateTimePicker;

    @Mock
    PrefService mPrefService;

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
        when(mPrefService.getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS))
                .thenReturn(DownloadLaterPromptStatus.SHOW_INITIAL);

        mActivityTestRule.startMainActivityOnBlankPage();

        mDialogCoordinator = new DownloadLaterDialogCoordinator(mDateTimePicker);
        mModel = new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                         .with(DownloadLaterDialogProperties.CONTROLLER, mDialogCoordinator)
                         .with(DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION,
                                 DownloadLaterDialogChoice.ON_WIFI)
                         .with(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION,
                                 DownloadLaterPromptStatus.SHOW_INITIAL)
                         .build();
        Assert.assertNotNull(mController);
        mDialogCoordinator.initialize(mController);
    }

    private void showDialog() {
        mDialogCoordinator.showDialog(
                mActivityTestRule.getActivity(), getModalDialogManager(), mPrefService, mModel);
    }

    private void clickPositiveButton() {
        onView(withId(org.chromium.chrome.R.id.positive_button)).perform(click());
    }

    private void clickNegativeButton() {
        onView(withId(org.chromium.chrome.R.id.negative_button)).perform(click());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Crash on Android P bots without crashlog crbug.com/1101413")
    public void testShowDialogThenDismiss() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();
        });

        clickNegativeButton();
        verify(mController).onDownloadLaterDialogCanceled();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Crash on Android P bots without crashlog crbug.com/1101413")
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
        });

        clickPositiveButton();
        verify(mController)
                .onDownloadLaterDialogComplete(
                        eq(DownloadLaterDialogChoice.DOWNLOAD_NOW), eq(INVALID_START_TIME));
    }

    @Test
    @MediumTest
    public void testSelectDownloadLater() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showDialog();

            RadioButtonWithDescription downloadLaterButton =
                    getDownloadLaterDialogView().findViewById(R.id.choose_date_time);
            Assert.assertNotNull(downloadLaterButton);
            downloadLaterButton.setChecked(true);
            getDownloadLaterDialogView().onCheckedChanged(null, -1);
        });

        clickPositiveButton();
        verify(mController, times(0)).onDownloadLaterDialogComplete(anyInt(), anyLong());
        verify(mDateTimePicker).showDialog(any(), any(), any());
    }
}
