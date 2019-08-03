// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Configuration;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * DummyUiActivity Tests for the {@link TabGridDialogParent}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridDialogParentTest extends DummyUiActivityTestCase {
    private int mToolbarHeight;
    private int mTopMargin;
    private int mSideMargin;
    private FrameLayout mDummyParent;
    private View mUngroupBar;
    private View mBlockView;
    private RelativeLayout mTabGridDialogContainer;
    private PopupWindow mPopoupWindow;
    private FrameLayout.LayoutParams mContainerParams;
    private TabGridDialogParent mTabGridDialogParent;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        FeatureUtilities.setIsTabToGtsAnimationEnabledForTesting(true);

        mDummyParent = new FrameLayout(getActivity());
        mTabGridDialogParent = new TabGridDialogParent(getActivity(), mDummyParent);
        mPopoupWindow = mTabGridDialogParent.getPopupWindowForTesting();
        FrameLayout tabGridDialogParentView =
                mTabGridDialogParent.getTabGridDialogParentViewForTesting();

        mTabGridDialogContainer = tabGridDialogParentView.findViewById(R.id.dialog_container_view);
        mUngroupBar = mTabGridDialogContainer.findViewById(R.id.dialog_ungroup_bar);
        mContainerParams = (FrameLayout.LayoutParams) mTabGridDialogContainer.getLayoutParams();
        mBlockView = tabGridDialogParentView.findViewById(R.id.dialog_block_view);

        mToolbarHeight =
                (int) getActivity().getResources().getDimension(R.dimen.tab_group_toolbar_height);
        mTopMargin =
                (int) getActivity().getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        mSideMargin = (int) getActivity().getResources().getDimension(
                R.dimen.tab_grid_dialog_side_margin);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdateDialogWithOrientation() throws Exception {
        mockDialogStatus(false);

        mTabGridDialogParent.updateDialogWithOrientation(
                getActivity(), Configuration.ORIENTATION_PORTRAIT);

        Assert.assertEquals(mTopMargin, mContainerParams.topMargin);
        Assert.assertEquals(mSideMargin, mContainerParams.leftMargin);
        Assert.assertFalse(mPopoupWindow.isShowing());

        mockDialogStatus(false);

        mTabGridDialogParent.updateDialogWithOrientation(
                getActivity(), Configuration.ORIENTATION_LANDSCAPE);

        Assert.assertEquals(mSideMargin, mContainerParams.topMargin);
        Assert.assertEquals(mTopMargin, mContainerParams.leftMargin);
        Assert.assertFalse(mPopoupWindow.isShowing());

        mockDialogStatus(true);

        mTabGridDialogParent.updateDialogWithOrientation(
                getActivity(), Configuration.ORIENTATION_PORTRAIT);

        Assert.assertEquals(mTopMargin, mContainerParams.topMargin);
        Assert.assertEquals(mSideMargin, mContainerParams.leftMargin);
        Assert.assertTrue(mPopoupWindow.isShowing());

        mockDialogStatus(true);

        mTabGridDialogParent.updateDialogWithOrientation(
                getActivity(), Configuration.ORIENTATION_LANDSCAPE);

        Assert.assertEquals(mSideMargin, mContainerParams.topMargin);
        Assert.assertEquals(mTopMargin, mContainerParams.leftMargin);
        Assert.assertTrue(mPopoupWindow.isShowing());
    }

    @Test
    @SmallTest
    public void testResetDialog() throws Exception {
        mTabGridDialogContainer.removeAllViews();
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        recyclerView.setVisibility(View.GONE);

        mTabGridDialogParent.resetDialog(toolbarView, recyclerView);

        // It should contain three child views: top tool bar, recyclerview and ungroup bar.
        Assert.assertEquals(3, mTabGridDialogContainer.getChildCount());
        Assert.assertEquals(View.VISIBLE, recyclerView.getVisibility());
        RelativeLayout.LayoutParams params =
                (RelativeLayout.LayoutParams) recyclerView.getLayoutParams();
        Assert.assertEquals(mToolbarHeight, params.topMargin);
        Assert.assertEquals(0, params.leftMargin);
        Assert.assertEquals(0, params.rightMargin);
        Assert.assertEquals(0, params.bottomMargin);
    }

    @Test
    @SmallTest
    public void testUpdateUngroupBar() throws Exception {
        mTabGridDialogContainer.removeAllViews();
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        mUngroupBar.setVisibility(View.GONE);

        mTabGridDialogParent.resetDialog(toolbarView, recyclerView);
        mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.SHOW);

        Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        Assert.assertTrue(mUngroupBar.getAlpha() == 1f);
        // Ungroup bar is brought to front.
        Assert.assertEquals(mUngroupBar,
                mTabGridDialogContainer.getChildAt(mTabGridDialogContainer.getChildCount() - 1));

        mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.HOVERED);
        Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        Assert.assertTrue(mUngroupBar.getAlpha() == 0.7f);
        // Content view should be brought to front.
        Assert.assertEquals(recyclerView,
                mTabGridDialogContainer.getChildAt(mTabGridDialogContainer.getChildCount() - 1));

        mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.HIDE);
        Assert.assertEquals(View.INVISIBLE, mUngroupBar.getVisibility());
    }

    @Test
    @MediumTest
    public void testShowDialog() throws Exception {
        // Setup basic showing animation for dialog.
        mTabGridDialogParent.setupDialogAnimation(null);
        mBlockView.setClickable(false);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mockDialogStatus(false);
            mTabGridDialogParent.showDialog();
            // Block view is set to clickable before animation to block clicks during animation.
            Assert.assertTrue(mBlockView.isClickable());
            Assert.assertNotNull(mTabGridDialogParent.getCurrentAnimatorForTesting());
            Assert.assertTrue(mPopoupWindow.isShowing());
        });

        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentAnimatorForTesting() == null);
        Assert.assertFalse(mBlockView.isClickable());
    }

    @Test
    @MediumTest
    public void testHideDialog() throws Exception {
        // Setup basic hiding animation for dialog.
        mTabGridDialogParent.setupDialogAnimation(null);
        mBlockView.setClickable(false);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mockDialogStatus(true);
            mTabGridDialogParent.hideDialog();
            // Block view is set to clickable before animation to block clicks during animation.
            Assert.assertTrue(mBlockView.isClickable());
            Assert.assertNotNull(mTabGridDialogParent.getCurrentAnimatorForTesting());
            // PopupWindow is still showing now for the closing animation.
            Assert.assertTrue(mPopoupWindow.isShowing());
        });

        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentAnimatorForTesting() == null);
        Assert.assertFalse(mBlockView.isClickable());
        // PopupWindow is dismissed when the animation is over.
        Assert.assertFalse(mPopoupWindow.isShowing());
    }

    private void mockDialogStatus(boolean isShowing) {
        mContainerParams.setMargins(0, 0, 0, 0);
        if (isShowing) {
            mPopoupWindow.showAtLocation(mDummyParent, Gravity.CENTER, 0, 0);
            Assert.assertTrue(mPopoupWindow.isShowing());
        } else {
            mPopoupWindow.dismiss();
            Assert.assertFalse(mPopoupWindow.isShowing());
        }
    }

    @Override
    public void tearDownTest() throws Exception {
        mTabGridDialogParent.destroy();
        super.tearDownTest();
    }
}
