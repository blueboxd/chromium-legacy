// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.ui.widget.test.R;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link RadioButtonLayout}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class RadioButtonWithDescriptionLayoutTest {
    private static final String NON_ZERO_MARGIN_ASSERT_MESSAGE =
            "First N-1 items should have a non-zero margin";
    private static final String ZERO_MARGIN_ASSERT_MESSAGE =
            "The last item should have a zero margin";
    private static final String TITLE_MATCH_ASSERT_MESSAGE =
            "Primary text set through addButtons should match the view's primary text.";
    private static final String DESCRIPTION_MATCH_ASSERT_MESSAGE =
            "Description set through addButtons should match the view's description.";
    private static final String TAG_MATCH_ASSERT_MESSAGE =
            "Tag set through addButtons should match the view's tag.";

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMargins() {
        RadioButtonWithDescriptionLayout layout = new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of buttons.
        List<RadioButtonWithDescription> buttons =
                Arrays.asList(createRadioButtonWithDescription("a", "a_desc", "a_tag"),
                        createRadioButtonWithDescription("b", "b_desc", "b_tag"),
                        createRadioButtonWithDescription("c", "c_desc", "c_tag"));
        layout.addButtons(buttons);
        Assert.assertEquals(3, layout.getChildCount());

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (i < layout.getChildCount() - 1) {
                Assert.assertNotEquals(NON_ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            } else {
                Assert.assertEquals(ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            }
        }

        // Add more buttons.
        List<RadioButtonWithDescription> moreButtons =
                Arrays.asList(createRadioButtonWithDescription("d", "d_desc", null),
                        createRadioButtonWithDescription("e", "e_desc", null),
                        createRadioButtonWithDescription("f", "f_desc", null));
        layout.addButtons(moreButtons);
        Assert.assertEquals(6, layout.getChildCount());

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (i < layout.getChildCount() - 1) {
                Assert.assertNotEquals(NON_ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            } else {
                Assert.assertEquals(ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            }
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAddButtons() {
        RadioButtonWithDescriptionLayout layout = new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescription> buttons =
                Arrays.asList(createRadioButtonWithDescription("a", "a_desc", "a_tag"),
                        createRadioButtonWithDescription("b", "b_desc", "b_tag"),
                        createRadioButtonWithDescription("c", "c_desc", "c_tag"));
        layout.addButtons(buttons);
        Assert.assertEquals(3, layout.getChildCount());

        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(TITLE_MATCH_ASSERT_MESSAGE, buttons.get(i).getPrimaryText(),
                    b.getPrimaryText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getDescriptionText(), b.getDescriptionText());
            Assert.assertEquals(TAG_MATCH_ASSERT_MESSAGE, buttons.get(i).getTag(), b.getTag());
        }

        // Add even more options, but without tags.
        List<RadioButtonWithDescription> moreButtons =
                Arrays.asList(createRadioButtonWithDescription("d", "d_desc", null),
                        createRadioButtonWithDescription("e", "e_desc", null),
                        createRadioButtonWithDescription("f", "f_desc", null));
        layout.addButtons(moreButtons);
        Assert.assertEquals(6, layout.getChildCount());
        for (int i = 0; i < 3; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(TITLE_MATCH_ASSERT_MESSAGE, buttons.get(i).getPrimaryText(),
                    b.getPrimaryText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getDescriptionText(), b.getDescriptionText());
            Assert.assertEquals(TAG_MATCH_ASSERT_MESSAGE, buttons.get(i).getTag(), b.getTag());
        }
        for (int i = 3; i < 6; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(TITLE_MATCH_ASSERT_MESSAGE, moreButtons.get(i - 3).getPrimaryText(),
                    b.getPrimaryText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    moreButtons.get(i - 3).getDescriptionText(), b.getDescriptionText());
            Assert.assertEquals(
                    TAG_MATCH_ASSERT_MESSAGE, moreButtons.get(i - 3).getTag(), b.getTag());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelection() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescription> buttons =
                Arrays.asList(createRadioButtonWithDescription("a", "a_desc", null),
                        createRadioButtonWithDescription("b", "b_desc", null),
                        createRadioButtonWithDescription("c", "c_desc", null));
        layout.addButtons(buttons);
        Assert.assertEquals(3, layout.getChildCount());

        // Nothing should be selected by default.
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription child = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertFalse(child.isChecked());
        }

        // Select the second one.
        layout.selectChildAtIndexForTesting(1);
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription child = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(i == 1, child.isChecked());
        }

        // Add even more options.
        List<RadioButtonWithDescription> moreButtons =
                Arrays.asList(createRadioButtonWithDescription("d", "d_desc", null),
                        createRadioButtonWithDescription("e", "e_desc", null),
                        createRadioButtonWithDescription("f", "f_desc", null));
        layout.addButtons(moreButtons);
        Assert.assertEquals(6, layout.getChildCount());

        // Second child should still be checked.
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription child = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(i == 1, child.isChecked());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAccessoryViewAdded() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        List<RadioButtonWithDescription> buttons =
                Arrays.asList(createRadioButtonWithDescription("a", "a_desc", null),
                        createRadioButtonWithDescription("b", "b_desc", null),
                        createRadioButtonWithDescription("c", "c_desc", null));
        layout.addButtons(buttons);

        RadioButtonWithDescription firstButton = (RadioButtonWithDescription) layout.getChildAt(0);
        final TextView accessoryTextView = new TextView(mContext);
        layout.attachAccessoryView(accessoryTextView, firstButton);
        Assert.assertEquals(
                "The accessory view should be right after the position of it's attachment host.",
                accessoryTextView, layout.getChildAt(1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAccessoryViewAddedThenReadded() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        List<RadioButtonWithDescription> buttons =
                Arrays.asList(createRadioButtonWithDescription("a", "a_desc", null),
                        createRadioButtonWithDescription("b", "b_desc", null),
                        createRadioButtonWithDescription("c", "c_desc", null));
        layout.addButtons(buttons);

        RadioButtonWithDescription firstButton = (RadioButtonWithDescription) layout.getChildAt(0);
        RadioButtonWithDescription lastButton =
                (RadioButtonWithDescription) layout.getChildAt(layout.getChildCount() - 1);
        final TextView accessoryTextView = new TextView(mContext);
        layout.attachAccessoryView(accessoryTextView, firstButton);
        layout.attachAccessoryView(accessoryTextView, lastButton);
        Assert.assertNotEquals(
                "The accessory view shouldn't be in the first position it was inserted at.",
                accessoryTextView, layout.getChildAt(1));
        Assert.assertEquals("The accessory view should be at the new position it was placed at.",
                accessoryTextView, layout.getChildAt(layout.getChildCount() - 1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCombinedRadioButtons() {
        // Test if radio buttons are set up correctly when there are multiple classes in the same
        // layout.
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        RadioButtonWithDescription b1 = createRadioButtonWithDescription("a", "a_desc", null);
        RadioButtonWithDescription b2 = createRadioButtonWithDescription("b", "b_desc", null);

        RadioButtonWithEditText b3 = new RadioButtonWithEditText(mContext, null);
        b3.setPrimaryText("c");
        b3.setDescriptionText("c_desc");

        List<RadioButtonWithDescription> buttons = Arrays.asList(b1, b2, b3);
        layout.addButtons(buttons);

        layout.selectChildAtIndexForTesting(3);
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(TITLE_MATCH_ASSERT_MESSAGE, buttons.get(i).getPrimaryText(),
                    b.getPrimaryText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getDescriptionText(), b.getDescriptionText());
            Assert.assertEquals(i == 3, b.isChecked());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testInflateFromLayout() {
        View content = LayoutInflater.from(mContext).inflate(
                R.layout.radio_button_with_description_layout_test, null, false);

        RadioButtonWithDescriptionLayout layout =
                content.findViewById(R.id.test_radio_button_layout);
        RadioButtonWithDescription b1 = content.findViewById(R.id.test_radio_description_1);
        RadioButtonWithDescription b2 = content.findViewById(R.id.test_radio_description_2);
        RadioButtonWithEditText b3 = content.findViewById(R.id.test_radio_edit_text_1);
        RadioButtonWithEditText b4 = content.findViewById(R.id.test_radio_edit_text_2);

        Assert.assertNotNull(layout);
        Assert.assertNotNull(b1);
        Assert.assertNotNull(b2);
        Assert.assertNotNull(b3);
        Assert.assertNotNull(b4);
    }

    private RadioButtonWithDescription createRadioButtonWithDescription(
            String primary, String description, Object tag) {
        RadioButtonWithDescription b = new RadioButtonWithDescription(mContext, null);
        b.setPrimaryText(primary);
        b.setDescriptionText(description);
        b.setTag(tag);
        return b;
    }
}
