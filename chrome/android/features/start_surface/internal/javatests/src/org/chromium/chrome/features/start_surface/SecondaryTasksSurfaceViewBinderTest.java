// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/** Tests for {@link SecondaryTasksSurfaceViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SecondaryTasksSurfaceViewBinderTest extends DummyUiActivityTestCase {
    private ViewGroup mParentView;
    private View mTasksSurfaceView;
    private PropertyModel mPropertyModel;
    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Note that the specific type of the parent view and tasks surface view do not matter
            // for the SecondaryTasksSurfaceViewBinderTest.
            mParentView = new FrameLayout(getActivity());
            mTasksSurfaceView = new View(getActivity());
            getActivity().setContentView(mParentView);
        });

        mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(mPropertyModel,
                new TasksSurfaceViewBinder.ViewHolder(mParentView, mTasksSurfaceView),
                SecondaryTasksSurfaceViewBinder::bind);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityAfterShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityBeforeShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityWithTopBar() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());
        mPropertyModel.set(TOP_BAR_HEIGHT, 20);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(20, layoutParams.topMargin);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotEquals(mTasksSurfaceView.getParent(), null);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTopBarHeight() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());

        // Setting the top bar height shouldn't cause a NullPointerException when the layout params
        // are null, since this should be handled in the *ViewBinder.
        mPropertyModel.set(TOP_BAR_HEIGHT, 20);
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);

        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals("Top bar height isn't initialized correctly.", 20, layoutParams.topMargin);

        layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        mPropertyModel.set(TOP_BAR_HEIGHT, 40);
        assertEquals("Wrong top bar height.", 40, layoutParams.topMargin);
    }
}
