// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Unit tests for {@link RadioButtonWithDescriptionAndAuxButton}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class RadioButtonWithDescriptionAndAuxButtonTest extends DummyUiActivityTestCase {
    private class AuxButtonClickedListener
            implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
        private CallbackHelper mCallbackHelper = new CallbackHelper();
        private int mClickedId;

        AuxButtonClickedListener() {}

        @Override
        public void onAuxButtonClicked(int clickedId) {
            mCallbackHelper.notifyCalled();
            mClickedId = clickedId;
        }

        int getTimesCalled() {
            return mCallbackHelper.getCallCount();
        }

        int getClickedId() {
            return mClickedId;
        }
    }

    private AuxButtonClickedListener mListener;
    private Activity mActivity;

    private RadioButtonWithDescriptionAndAuxButton mRadioButton;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mActivity = getActivity();
        mListener = new AuxButtonClickedListener();

        setupViewsForTest();
    }

    private void setupViewsForTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View layout = LayoutInflater.from(mActivity).inflate(
                    R.layout.radio_button_with_description_and_aux_button_test, null, false);
            mActivity.setContentView(layout);

            mRadioButton = layout.findViewById(R.id.test_radio_button);
            Assert.assertNotNull(mRadioButton);
        });
    }

    @Test
    @SmallTest
    public void testOnAuxButtonClicked() {
        mRadioButton.setAuxButtonClickedListener(mListener);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRadioButton.getAuxButtonForTests().performClick(); });
        Assert.assertEquals("AuxButtonClickedListener#onAuxButtonClicked should be called once", 1,
                mListener.getTimesCalled());
        Assert.assertEquals(R.id.test_radio_button, mListener.getClickedId());
    }

    @Test
    @SmallTest
    public void testAuxButtonEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mRadioButton.setEnabled(false); });
        Assert.assertFalse("Primary TextView should be set to disabled.",
                mRadioButton.getPrimaryTextView().isEnabled());
        Assert.assertFalse("Description TextView should be set to disabled.",
                mRadioButton.getDescriptionTextView().isEnabled());
        Assert.assertFalse("RadioButton should be set to disabled.",
                mRadioButton.getRadioButtonView().isEnabled());
        Assert.assertFalse("Aux Button should be set to disabled.",
                mRadioButton.getAuxButtonForTests().isEnabled());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mRadioButton.setAuxButtonEnabled(true); });
        Assert.assertFalse("Primary TextView should keep disabled.",
                mRadioButton.getPrimaryTextView().isEnabled());
        Assert.assertFalse("Description TextView should keep disabled.",
                mRadioButton.getDescriptionTextView().isEnabled());
        Assert.assertFalse(
                "RadioButton should keep disabled.", mRadioButton.getRadioButtonView().isEnabled());
        Assert.assertTrue("Aux Button should be set to enabled.",
                mRadioButton.getAuxButtonForTests().isEnabled());
    }

    @Test
    @SmallTest
    public void testPaddingAndBackgroundValue() {
        View radioContainer = mRadioButton.findViewById(R.id.radio_container);
        int lateralPadding = mRadioButton.getResources().getDimensionPixelSize(
                R.dimen.radio_button_with_description_lateral_padding);
        int auxButtonSpacing = mRadioButton.getResources().getDimensionPixelSize(
                R.dimen.radio_button_with_description_and_aux_button_spacing);
        Assert.assertEquals("Lateral padding should be set in the radio container.", lateralPadding,
                radioContainer.getPaddingStart());
        Assert.assertEquals("Aux button spacing should be set in the radio container.",
                auxButtonSpacing, radioContainer.getPaddingEnd());
        Assert.assertEquals("Lateral padding should be set to 0 in the radio button root layout.",
                0, mRadioButton.getPaddingStart());
        Assert.assertNotNull(
                "Background should be set in the radio container.", radioContainer.getBackground());
        Assert.assertNull("Background should be null in the radio button root layout",
                mRadioButton.getBackground());
    }
}
