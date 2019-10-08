// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link TabGridMessageCardViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridMessageCardViewBinderTest extends DummyUiActivityTestCase {
    private static final String ACTION_TEXT = "actionText";
    private static final String DESCRIPTION_TEXT = "descriptionText";

    private ViewGroup mItemView;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new LinearLayout(getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);

            mItemView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.tab_suggestion_card_item, null);
            view.addView(mItemView);
        });

        mItemViewModel =
                new PropertyModel.Builder(TabGridMessageCardViewProperties.ALL_KEYS)
                        .with(TabGridMessageCardViewProperties.ACTION_TEXT, ACTION_TEXT)
                        .with(TabGridMessageCardViewProperties.DESCRIPTION_TEXT, DESCRIPTION_TEXT)
                        .build();

        mItemMCP = PropertyModelChangeProcessor.create(
                mItemViewModel, mItemView, TabGridMessageCardViewBinder::bind);
    }

    private String getDescriptionText() {
        return ((TextView) mItemView.findViewById(R.id.description)).getText().toString();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testInitialBinding() {
        assertEquals(ACTION_TEXT,
                ((TextView) mItemView.findViewById(R.id.action_button)).getText().toString());
        assertEquals(DESCRIPTION_TEXT, getDescriptionText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingDescription_WithoutTemplate() {
        mItemViewModel.set(TabGridMessageCardViewProperties.DESCRIPTION_TEXT, "test");
        assertEquals("test", getDescriptionText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingDescription_WithTemplate() {
        mItemViewModel.set(
                TabGridMessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE, "%s template");
        mItemViewModel.set(TabGridMessageCardViewProperties.DESCRIPTION_TEXT, "test");
        assertEquals("test template", getDescriptionText());
    }

    @Override
    public void tearDownTest() throws Exception {
        mItemMCP.destroy();
        super.tearDownTest();
    }
}
