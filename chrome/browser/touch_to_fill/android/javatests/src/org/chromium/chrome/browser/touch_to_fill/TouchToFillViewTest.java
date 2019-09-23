// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;

import android.support.test.filters.MediumTest;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.SheetState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the Touch To Fill component ensure that model changes are reflected in the sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillViewTest {
    @Mock
    private TouchToFillProperties.ViewEventListener mMockListener;

    private PropertyModel mModel;
    private TouchToFillView mTouchToFillView;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    public TouchToFillViewTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mModel = TouchToFillProperties.createDefaultModel(mMockListener);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTouchToFillView =
                    new TouchToFillView(getActivity(), getActivity().getBottomSheetController());
            TouchToFillCoordinator.setUpModelChangeProcessors(mModel, mTouchToFillView);
        });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        assertThat(mTouchToFillView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == SheetState.HIDDEN);
        assertThat(mTouchToFillView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testSubtitleUrlChangedByModel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(FORMATTED_URL, "www.example.org");
            mModel.set(VISIBLE, true);
        });
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText(), is(getFormattedSubtitle("www.example.org")));

        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(FORMATTED_URL, "m.example.org"));

        assertThat(subtitle.getText(), is(getFormattedSubtitle("m.example.org")));
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == SheetState.HIDDEN);
        verify(mMockListener).onDismissed();
    }

    private ChromeActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    private String getFormattedSubtitle(String url) {
        return getActivity().getString(R.string.touch_to_fill_sheet_subtitle, url);
    }

    private @SheetState int getBottomSheetState() {
        pollUiThread(() -> getActivity().getBottomSheet() != null);
        return getActivity().getBottomSheet().getSheetState();
    }
}