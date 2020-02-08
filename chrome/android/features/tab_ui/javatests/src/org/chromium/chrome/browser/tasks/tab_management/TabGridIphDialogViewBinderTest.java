// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.graphics.drawable.Animatable;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabGridIphDialogViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridIphDialogViewBinderTest extends DummyUiActivityTestCase {
    private TextView mCloseIPHDialogButton;
    private TabGridIphDialogParent mIphDialogParent;
    private PopupWindow mIphWindow;
    private ViewGroup mDialogParentView;
    private Animatable mIphAnimation;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup parentView = new FrameLayout(getActivity());
            mIphDialogParent = new TabGridIphDialogParent(getActivity(), parentView);
            mIphWindow = mIphDialogParent.getIphWindowForTesting();
            mDialogParentView = (ViewGroup) mIphWindow.getContentView();
            mCloseIPHDialogButton = mDialogParentView.findViewById(R.id.close_button);
            mIphAnimation = (Animatable) ((ImageView) mDialogParentView.findViewById(
                                                  R.id.animation_drawable))
                                    .getDrawable();

            mModel = new PropertyModel(TabGridIphDialogProperties.ALL_KEYS);
            mMCP = PropertyModelChangeProcessor.create(
                    mModel, mIphDialogParent, TabGridIphDialogViewBinder::bind);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphDialogCloseButtonListener() {
        AtomicBoolean iphDialogCloseButtonClicked = new AtomicBoolean();
        iphDialogCloseButtonClicked.set(false);
        mCloseIPHDialogButton.performClick();
        Assert.assertFalse(iphDialogCloseButtonClicked.get());

        mModel.set(TabGridIphDialogProperties.CLOSE_BUTTON_LISTENER,
                (View view) -> iphDialogCloseButtonClicked.set(true));

        mCloseIPHDialogButton.performClick();
        Assert.assertTrue(iphDialogCloseButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphDialogVisibility() {
        assertFalse(mIphWindow.isShowing());
        assertFalse(mIphAnimation.isRunning());

        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, true);

        assertTrue(mIphWindow.isShowing());
        if (areAnimatorsEnabled()) {
            assertTrue(mIphAnimation.isRunning());
        }

        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, false);

        assertFalse(mIphWindow.isShowing());
        if (areAnimatorsEnabled()) {
            assertFalse(mIphAnimation.isRunning());
        }
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetScrimViewObserver() {
        AtomicBoolean iphDialogScrimViewClicked = new AtomicBoolean();
        iphDialogScrimViewClicked.set(false);
        ScrimView.ScrimObserver observer = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                iphDialogScrimViewClicked.set(true);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };

        mModel.set(TabGridIphDialogProperties.SCRIM_VIEW_OBSERVER, observer);
        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, true);

        assertTrue(mDialogParentView.getChildAt(0) instanceof ScrimView);
        ScrimView scrimView = (ScrimView) mDialogParentView.getChildAt(0);
        scrimView.performClick();
        assertTrue(iphDialogScrimViewClicked.get());
    }
}
