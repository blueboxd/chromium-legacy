// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.atomic.AtomicReference;

/**
 * DummyUiActivity Tests for the {@link TabGridDialogView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridDialogViewTest extends DummyUiActivityTestCase {
    private int mToolbarHeight;
    private int mTopMargin;
    private int mSideMargin;
    private FrameLayout mDummyParent;
    private View mUngroupBar;
    private View mAnimationCardView;
    private View mBackgroundFrameView;
    private TextView mUngroupBarTextView;
    private RelativeLayout mTabGridDialogContainer;
    private FrameLayout.LayoutParams mContainerParams;
    private TabGridDialogView mTabGridDialogView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDummyParent = new FrameLayout(getActivity());
            ScrimView scrimView = new ScrimView(getActivity(), null, mDummyParent);
            getActivity().setContentView(mDummyParent);
            LayoutInflater.from(getActivity())
                    .inflate(R.layout.tab_grid_dialog_layout, mDummyParent, true);

            mTabGridDialogView = mDummyParent.findViewById(R.id.dialog_parent_view);
            mTabGridDialogView.setupScrimView(scrimView);
            mTabGridDialogContainer = mTabGridDialogView.findViewById(R.id.dialog_container_view);
            mUngroupBar = mTabGridDialogContainer.findViewById(R.id.dialog_ungroup_bar);
            mUngroupBarTextView = mUngroupBar.findViewById(R.id.dialog_ungroup_bar_text);
            mContainerParams = (FrameLayout.LayoutParams) mTabGridDialogContainer.getLayoutParams();
            mAnimationCardView = mTabGridDialogView.findViewById(R.id.dialog_animation_card_view);
            mBackgroundFrameView = mTabGridDialogView.findViewById(R.id.dialog_frame);

            mToolbarHeight = (int) getActivity().getResources().getDimension(
                    R.dimen.tab_group_toolbar_height);
            mTopMargin = (int) getActivity().getResources().getDimension(
                    R.dimen.tab_grid_dialog_top_margin);
            mSideMargin = (int) getActivity().getResources().getDimension(
                    R.dimen.tab_grid_dialog_side_margin);
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdateDialogWithOrientation() {
        mockDialogStatus(false);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_PORTRAIT);

        Assert.assertEquals(mTopMargin, mContainerParams.topMargin);
        Assert.assertEquals(mSideMargin, mContainerParams.leftMargin);
        Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());

        mockDialogStatus(false);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_LANDSCAPE);

        Assert.assertEquals(mSideMargin, mContainerParams.topMargin);
        Assert.assertEquals(mTopMargin, mContainerParams.leftMargin);
        Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());

        mockDialogStatus(true);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_PORTRAIT);

        Assert.assertEquals(mTopMargin, mContainerParams.topMargin);
        Assert.assertEquals(mSideMargin, mContainerParams.leftMargin);
        Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());

        mockDialogStatus(true);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_LANDSCAPE);

        Assert.assertEquals(mSideMargin, mContainerParams.topMargin);
        Assert.assertEquals(mTopMargin, mContainerParams.leftMargin);
        Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testResetDialog() {
        mTabGridDialogContainer.removeAllViews();
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        recyclerView.setVisibility(View.GONE);

        mTabGridDialogView.resetDialog(toolbarView, recyclerView);

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
    public void testUpdateUngroupBar() {
        AtomicReference<ColorStateList> showTextColorReference = new AtomicReference<>();
        AtomicReference<ColorStateList> hoverTextColorReference = new AtomicReference<>();
        // Initialize the dialog with dummy views.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogContainer.removeAllViews();
            View toolbarView = new View(getActivity());
            View recyclerView = new View(getActivity());
            mTabGridDialogView.resetDialog(toolbarView, recyclerView);
        });

        // From hide to show.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ColorStateList colorStateList = mUngroupBarTextView.getTextColors();
            mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW);

            Assert.assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertNotEquals(colorStateList, mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
            // Initialize text color when the ungroup bar is showing.
            showTextColorReference.set(mUngroupBarTextView.getTextColors());
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        // From show to hide.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HIDE);

            Assert.assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is not updated.
            Assert.assertEquals(showTextColorReference.get(), mUngroupBarTextView.getTextColors());
            // Ungroup bar is still visible for the hiding animation.
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));
        // Ungroup bar is not visible after the hiding animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(View.INVISIBLE, mUngroupBar.getVisibility()));

        // From hide to hover.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ColorStateList colorStateList = mUngroupBarTextView.getTextColors();
            mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HOVERED);

            Assert.assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertNotEquals(colorStateList, mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
            // Initialize text color when the ungroup bar is being hovered on.
            hoverTextColorReference.set(mUngroupBarTextView.getTextColors());
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        // From hover to hide.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HIDE);

            Assert.assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is not updated.
            Assert.assertEquals(hoverTextColorReference.get(), mUngroupBarTextView.getTextColors());
            // Ungroup bar is still visible for the hiding animation.
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));
        // Ungroup bar is not visible after the hiding animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(View.INVISIBLE, mUngroupBar.getVisibility()));

        // From show to hover.
        // First, set the ungroup bar state to show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW));
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(showTextColorReference.get(), mUngroupBarTextView.getTextColors());

            mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HOVERED);

            // There should be no animation going on.
            Assert.assertNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertEquals(hoverTextColorReference.get(), mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });

        // From hover to show.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(hoverTextColorReference.get(), mUngroupBarTextView.getTextColors());

            mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW);

            // There should be no animation going on.
            Assert.assertNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
            // Verify that the ungroup bar textView is updated.
            Assert.assertEquals(showTextColorReference.get(), mUngroupBarTextView.getTextColors());
            Assert.assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1075677")
    public void testDialog_ZoomInZoomOut() {
        AtomicReference<ViewGroup> parentViewReference = new AtomicReference<>();
        // Setup the animation with a dummy animation source view.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View sourceView = new View(getActivity());
            mTabGridDialogView.setupDialogAnimation(sourceView);
            parentViewReference.set((ViewGroup) mTabGridDialogContainer.getParent());
        });
        ViewGroup parent = parentViewReference.get();

        // Show the dialog with zoom-out animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // At the very beginning of showing animation, the animation card should be on the
            // top and the background frame should be the view below it. Both view should have
            // alpha set to be 1.
            mTabGridDialogView.showDialog();
            if (areAnimatorsEnabled()) {
                Assert.assertSame(
                        mAnimationCardView, parent.getChildAt(parent.getChildCount() - 1));
                Assert.assertSame(
                        mBackgroundFrameView, parent.getChildAt(parent.getChildCount() - 2));
            }
            Assert.assertEquals(1f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(1f, mBackgroundFrameView.getAlpha(), 0.0);
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
            Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        });
        // When the card fades out, the dialog should be brought to the top, and alpha of animation
        // related views should be set to 0.
        CriteriaHelper.pollUiThread(Criteria.equals(
                mTabGridDialogContainer, () -> parent.getChildAt(parent.getChildCount() - 1)));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0));
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0));

        // Hide the dialog with zoom-in animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.hideDialog();
            // At the very beginning of hiding animation, the dialog view should be on the top,
            // and alpha of animation related views should remain 0.
            if (areAnimatorsEnabled()) {
                Assert.assertSame(
                        mTabGridDialogContainer, parent.getChildAt(parent.getChildCount() - 1));
            }
            Assert.assertEquals(1f, mTabGridDialogContainer.getAlpha(), 0.0);
            Assert.assertEquals(1f, mBackgroundFrameView.getAlpha(), 0.0);
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
            // PopupWindow is still showing for the hide animation.
            Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        });
        // When the dialog fades out, the animation card and the background frame should be brought
        // to the top.
        CriteriaHelper.pollUiThread(
                ()
                        -> mAnimationCardView == parent.getChildAt(parent.getChildCount() - 1)
                        && mBackgroundFrameView == parent.getChildAt(parent.getChildCount() - 2));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0f, mTabGridDialogContainer.getAlpha(), 0.0));
        // When the animation completes, the PopupWindow should be dismissed.
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
        });
    }

    @Test
    @MediumTest
    public void testDialog_ZoomInFadeOut() {
        // Setup the animation with a dummy animation source view.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View sourceView = new View(getActivity());
            mTabGridDialogView.setupDialogAnimation(sourceView);
        });
        // Show the dialog.
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabGridDialogView.showDialog());
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        // After the zoom in animation, alpha of animation related views should be 0.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
        });

        // Hide the dialog with basic fade-out animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.setupDialogAnimation(null);
            mTabGridDialogView.hideDialog();
            // Alpha of animation related views should remain 0.
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
            Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        });
        // When the animation completes, the PopupWindow should be dismissed. The alpha of animation
        // related views should remain 0.
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
        });
    }

    @Test
    @MediumTest
    public void testDialog_FadeInFadeOut() {
        // Setup the the basic fade-in and fade-out animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.setupDialogAnimation(null);
            // Initially alpha of animation related views should be 0.
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
        });

        // Show the dialog with basic fade-in animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.showDialog();
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
            Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        });
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
        });

        // Hide the dialog with basic fade-out animation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridDialogView.hideDialog();
            if (areAnimatorsEnabled()) {
                // At the very beginning of hiding animation, alpha of background frame and
                // animation card should both be set to 0f.
                Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
            }
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
            Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        });
        // When the animation completes, the PopupWindow should be dismissed. The alpha of animation
        // related views should remain 0.
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());
            Assert.assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
            Assert.assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
        });
    }

    private void mockDialogStatus(boolean isShowing) {
        mContainerParams.setMargins(0, 0, 0, 0);
        if (isShowing) {
            mTabGridDialogView.setVisibility(View.VISIBLE);
        } else {
            mTabGridDialogView.setVisibility(View.GONE);
        }
    }
}
