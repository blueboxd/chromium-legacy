// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.provider.Settings;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
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
    private View mAnimationCardView;
    private View mBackgroundFrameView;
    private TextView mUngroupBarTextView;
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
        mUngroupBarTextView = mUngroupBar.findViewById(R.id.dialog_ungroup_bar_text);
        mContainerParams = (FrameLayout.LayoutParams) mTabGridDialogContainer.getLayoutParams();
        mAnimationCardView = mTabGridDialogParent.getAnimationCardViewForTesting();
        mBackgroundFrameView = tabGridDialogParentView.findViewById(R.id.dialog_frame);

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
    @MediumTest
    public void testUpdateUngroupBar() throws Exception {
        mTabGridDialogContainer.removeAllViews();
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        ColorStateList showTextColor;
        ColorStateList hoverTextColor;

        // Initialize the dialog with dummy views.
        mTabGridDialogParent.resetDialog(toolbarView, recyclerView);

        // From hide to show.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ColorStateList colorStateList = mUngroupBarTextView.getTextColors();
            mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.SHOW);

            Assert.assertNotNull(mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertNotEquals(colorStateList, mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });
        // Initialize text color when the ungroup bar is showing.
        showTextColor = mUngroupBarTextView.getTextColors();

        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting() == null);

        // From show to hide.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.HIDE);

            Assert.assertNotNull(mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is not updated.
            Assert.assertEquals(showTextColor, mUngroupBarTextView.getTextColors());
            // Ungroup bar is still visible for the hiding animation.
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });

        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting() == null);
        Assert.assertEquals(View.INVISIBLE, mUngroupBar.getVisibility());

        // From hide to hover.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ColorStateList colorStateList = mUngroupBarTextView.getTextColors();
            mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.HOVERED);

            Assert.assertNotNull(mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertNotEquals(colorStateList, mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });
        // Initialize text color when the ungroup bar is being hovered on.
        hoverTextColor = mUngroupBarTextView.getTextColors();

        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting() == null);

        // From hover to hide.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.HIDE);

            Assert.assertNotNull(mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is not updated.
            Assert.assertEquals(hoverTextColor, mUngroupBarTextView.getTextColors());
            // Ungroup bar is still visible for the hiding animation.
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });

        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting() == null);
        Assert.assertEquals(View.INVISIBLE, mUngroupBar.getVisibility());

        // From show to hover.
        // First, set the ungroup bar state to show.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mTabGridDialogParent.updateUngroupBar(
                                TabGridDialogParent.UngroupBarStatus.SHOW));
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting() == null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(showTextColor, mUngroupBarTextView.getTextColors());

            mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.HOVERED);

            // There should be no animation going on.
            Assert.assertNull(mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertEquals(hoverTextColor, mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });

        // From hover to show.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(hoverTextColor, mUngroupBarTextView.getTextColors());

            mTabGridDialogParent.updateUngroupBar(TabGridDialogParent.UngroupBarStatus.SHOW);

            // There should be no animation going on.
            Assert.assertNull(mTabGridDialogParent.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertEquals(showTextColor, mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });
    }

    @Test
    @MediumTest
    public void testDialog_ZoomInZoomOut() throws Exception {
        // Setup the animation with a dummy animation params.
        TabGridDialogParent.AnimationParams params = new TabGridDialogParent.AnimationParams(
                new Rect(3, 3, 3, 3), new View(getActivity()));
        mTabGridDialogParent.setupDialogAnimation(params);
        ViewGroup parent = (ViewGroup) mTabGridDialogContainer.getParent();

        // Show the dialog with zoom-out animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.showDialog();
            if (areAnimatorsEnabled()) {
                Assert.assertTrue(mAnimationCardView.getAlpha() == 1f);
                // At the very beginning of showing animation, the animation card should be on the
                // top and the background frame should be the view below it.
                Assert.assertTrue(
                        mAnimationCardView == parent.getChildAt(parent.getChildCount() - 1));
                Assert.assertTrue(
                        mBackgroundFrameView == parent.getChildAt(parent.getChildCount() - 2));
            }
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
            Assert.assertTrue(mPopoupWindow.isShowing());
        });
        // When the card fades out, the dialog should be brought to the top.
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogContainer == parent.getChildAt(parent.getChildCount() - 1));
        Assert.assertTrue(mAnimationCardView.getAlpha() == 0f);
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);

        // Hide the dialog with zoom-in animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.hideDialog();
            if (areAnimatorsEnabled()) {
                Assert.assertTrue(mTabGridDialogContainer.getAlpha() == 1f);
                // At the very beginning of hiding animation, the dialog view should be on the top.
                Assert.assertTrue(
                        mTabGridDialogContainer == parent.getChildAt(parent.getChildCount() - 1));
            }
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
            // PopupWindow is still showing for the hide animation.
            Assert.assertTrue(mPopoupWindow.isShowing());
        });
        // When the dialog fades out, the animation card and the background frame should be brought
        // to the top.
        CriteriaHelper.pollUiThread(
                ()
                        -> mAnimationCardView == parent.getChildAt(parent.getChildCount() - 1)
                        && mBackgroundFrameView == parent.getChildAt(parent.getChildCount() - 2));
        Assert.assertTrue(mTabGridDialogContainer.getAlpha() == 0f);
        // When the animation completes, the PopupWindow should be dismissed.
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);
        Assert.assertFalse(mPopoupWindow.isShowing());
    }

    @Test
    @MediumTest
    public void testDialog_ZoomInFadeOut() throws Exception {
        // Setup the animation with a dummy animation params.
        TabGridDialogParent.AnimationParams params = new TabGridDialogParent.AnimationParams(
                new Rect(3, 3, 3, 3), new View(getActivity()));
        mTabGridDialogParent.setupDialogAnimation(params);
        // Show the dialog.
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabGridDialogParent.showDialog());
        // Hide the dialog with basic fade-out animation.
        mTabGridDialogParent.setupDialogAnimation(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.hideDialog();
            if (areAnimatorsEnabled()) {
                // At the very beginning of hiding animation, alpha of background frame and
                // animation card should both be set to 0f.
                Assert.assertTrue(mBackgroundFrameView.getAlpha() == 0f);
                Assert.assertTrue(mAnimationCardView.getAlpha() == 0f);
            }
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
            Assert.assertTrue(mPopoupWindow.isShowing());
        });
        // When the animation completes, alpha of background frame and animation card should both
        // restore to 1f. Also, the PopupWindow should be dismissed.
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);
        Assert.assertFalse(mPopoupWindow.isShowing());
        Assert.assertTrue(mBackgroundFrameView.getAlpha() == 1f);
        Assert.assertTrue(mAnimationCardView.getAlpha() == 1f);
    }

    @Test
    @MediumTest
    public void testDialog_FadeInFadeOut() throws Exception {
        // Setup the the basic fade-in and fade-out animation.
        mTabGridDialogParent.setupDialogAnimation(null);
        mBackgroundFrameView.setAlpha(1f);
        mAnimationCardView.setAlpha(1f);

        // Show the dialog with basic fade-in animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.showDialog();
            if (areAnimatorsEnabled()) {
                // At the very beginning of showing animation, alpha of background frame and
                // animation card should both be set to 0f.
                Assert.assertTrue(mAnimationCardView.getAlpha() == 0f);
                Assert.assertTrue(mBackgroundFrameView.getAlpha() == 0f);
            }
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
            Assert.assertTrue(mPopoupWindow.isShowing());
        });
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);
        Assert.assertTrue(mAnimationCardView.getAlpha() == 0f);
        Assert.assertTrue(mBackgroundFrameView.getAlpha() == 0f);

        // Hide the dialog with basic fade-out animation.
        mBackgroundFrameView.setAlpha(1f);
        mAnimationCardView.setAlpha(1f);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogParent.hideDialog();
            if (areAnimatorsEnabled()) {
                // At the very beginning of hiding animation, alpha of background frame and
                // animation card should both be set to 0f.
                Assert.assertTrue(mAnimationCardView.getAlpha() == 0f);
                Assert.assertTrue(mBackgroundFrameView.getAlpha() == 0f);
            }
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
            Assert.assertTrue(mPopoupWindow.isShowing());
        });
        // When the animation completes, alpha of background frame and animation card should both
        // restore to 1f. Also, the PopupWindow should be dismissed.
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);
        Assert.assertFalse(mPopoupWindow.isShowing());
        Assert.assertTrue(mAnimationCardView.getAlpha() == 1f);
        Assert.assertTrue(mBackgroundFrameView.getAlpha() == 1f);
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

    /**
     * TODO(crbug.com/982018): put this interface in a place to share with
     * TabListContainerViewBinderTest.areAnimatorsEnabled.
     */
    private static boolean areAnimatorsEnabled() {
        // We default to assuming that animations are enabled in case ANIMATOR_DURATION_SCALE is not
        // defined.
        final float defaultScale = 1f;
        float durationScale =
                Settings.Global.getFloat(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, defaultScale);
        return !(durationScale == 0.0);
    }
}