// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.AnchoredPopupWindow;

/**
 * Tests for {@link TabGroupPopupUiViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGroupPopupUiViewBinderTest extends DummyUiActivityTestCase {
    private ToolbarPhone mTopAnchorView;
    private FrameLayout mBottomAnchorView;
    private TabGroupPopupUiParent mParent;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        mTopAnchorView = new ToolbarPhone(getActivity(), null);
        mTopAnchorView.setId(R.id.toolbar);
        mBottomAnchorView = new FrameLayout(getActivity());
        mParent = new TabGroupPopupUiParent(getActivity(), mTopAnchorView);

        mModel = new PropertyModel(TabGroupPopupUiProperties.ALL_KEYS);
        mMCP = PropertyModelChangeProcessor.create(
                mModel, mParent, TabGroupPopupUiViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibility() {
        AnchoredPopupWindow popupWindow = mParent.getCurrentPopupWindowForTesting();
        assertNotNull(popupWindow);
        assertFalse(popupWindow.isShowing());

        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        assertTrue(popupWindow.isShowing());

        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        assertFalse(popupWindow.isShowing());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetContentViewAlpha() {
        ViewGroup containerView = mParent.getCurrentStripContainerViewForTesting();
        assertNotNull(containerView);
        assertEquals(1f, containerView.getAlpha(), 0);

        mModel.set(TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA, 0.5f);
        assertEquals(0.5f, containerView.getAlpha(), 0);

        mModel.set(TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA, 0.25f);
        assertEquals(0.25f, containerView.getAlpha(), 0);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetAnchorView() {
        ViewGroup topContainerView;
        ViewGroup bottomContainerView;

        // Assume initial anchor view is top toolbar, and components for bottom toolbar is not
        // initialized.
        topContainerView = mParent.getTopStripContainerViewForTesting();
        bottomContainerView = mParent.getBottomStripContainerViewForTesting();
        assertEquals(topContainerView, mParent.getCurrentStripContainerViewForTesting());
        assertNull(bottomContainerView);

        // Create a dummy view representing the strip component and add it to the current container
        // view.
        FrameLayout contentView = new FrameLayout(getActivity());
        mParent.getCurrentStripContainerViewForTesting().addView(contentView);

        // Change anchor view from the top toolbar to the bottom toolbar.
        mModel.set(TabGroupPopupUiProperties.ANCHOR_VIEW, mBottomAnchorView);

        // Check that components related to bottom anchor view are initialized. Also, the ownership
        // of the content view should be given to the bottom strip container.
        topContainerView = mParent.getTopStripContainerViewForTesting();
        bottomContainerView = mParent.getBottomStripContainerViewForTesting();
        assertNotNull(bottomContainerView);
        assertEquals(bottomContainerView, mParent.getCurrentStripContainerViewForTesting());
        assertEquals(0, topContainerView.getChildCount());
        assertEquals(1, bottomContainerView.getChildCount());
        assertEquals(contentView, bottomContainerView.getChildAt(0));

        // Change anchor view from the bottom toolbar to the top toolbar.
        mModel.set(TabGroupPopupUiProperties.ANCHOR_VIEW, mTopAnchorView);

        // Check the ownership of the content view should be given back to top strip container.
        topContainerView = mParent.getTopStripContainerViewForTesting();
        bottomContainerView = mParent.getBottomStripContainerViewForTesting();
        assertNotNull(topContainerView);
        assertNotNull(bottomContainerView);
        assertEquals(topContainerView, mParent.getCurrentStripContainerViewForTesting());
        assertEquals(1, topContainerView.getChildCount());
        assertEquals(0, bottomContainerView.getChildCount());
        assertEquals(contentView, topContainerView.getChildAt(0));
    }
}
