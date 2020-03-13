// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.view.ContentView;

/**
 * Unit tests for {@link ChromeFullscreenManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenManagerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    // Since these tests don't depend on the heights being pixels, we can use these as dpi directly.
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int EXTRA_TOP_CONTROL_HEIGHT = 20;

    @Mock
    private Activity mActivity;
    @Mock
    private ControlContainer mControlContainer;
    @Mock
    private View mContainerView;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private android.content.res.Resources mResources;
    @Mock
    private ChromeFullscreenManager.FullscreenListener mFullscreenListener;
    @Mock
    private Tab mTab;
    @Mock
    private ContentView mContentView;

    private UserDataHost mUserDataHost = new UserDataHost();
    private ChromeFullscreenManager mFullscreenManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(R.dimen.control_container_height))
                .thenReturn(TOOLBAR_HEIGHT);
        when(mControlContainer.getView()).thenReturn(mContainerView);
        when(mContainerView.getVisibility()).thenReturn(View.VISIBLE);
        when(mTab.isUserInteractable()).thenReturn(true);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getContentView()).thenReturn(mContentView);
        doNothing().when(mContentView).removeOnHierarchyChangeListener(any());
        doNothing().when(mContentView).removeOnSystemUiVisibilityChangeListener(any());
        doNothing().when(mContentView).addOnHierarchyChangeListener(any());
        doNothing().when(mContentView).addOnSystemUiVisibilityChangeListener(any());

        ChromeFullscreenManager fullscreenManager = new ChromeFullscreenManager(
                mActivity, ChromeFullscreenManager.ControlsPosition.TOP);
        mFullscreenManager = spy(fullscreenManager);
        mFullscreenManager.initialize(
                mControlContainer, mTabModelSelector, R.dimen.control_container_height);
        mFullscreenManager.addListener(mFullscreenListener);
        when(mFullscreenManager.getTab()).thenReturn(mTab);
    }

    @Test
    public void testInitialTopControlsHeight() {
        assertEquals("Wrong initial top controls height.", TOOLBAR_HEIGHT,
                mFullscreenManager.getTopControlsHeight());
    }

    @Test
    public void testListenersNotifiedOfTopControlsHeightChange() {
        final int topControlsHeight = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        final int topControlsMinHeight = EXTRA_TOP_CONTROL_HEIGHT;
        mFullscreenManager.setTopControlsHeight(topControlsHeight, topControlsMinHeight);
        verify(mFullscreenListener)
                .onTopControlsHeightChanged(topControlsHeight, topControlsMinHeight);
    }

    // controlsResizeView tests ---
    // For these tests, we will simulate the scrolls assuming we either completely show or hide (or
    // scroll until the min-height) the controls and don't leave at in-between positions. The reason
    // is that ChromeFullscreenManager only flips the mControlsResizeView bit if the controls are
    // idle, meaning they're at the min-height or fully shown. Making sure the controls snap to
    // these two positions is not CFM's responsibility as it's handled in native code by compositor
    // or blink.

    @Test
    public void testControlsResizeViewChanges() {
        // Let's use simpler numbers for this test.
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabFullscreenObserver =
                mFullscreenManager.getTabFullscreenObserverForTesting();

        mFullscreenManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        // Initially, the controls should be fully visible.
        assertTrue("Browser controls aren't fully visible.",
                mFullscreenManager.areBrowserControlsFullyVisible());
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mFullscreenManager.controlsResizeView());

        // Scroll to fully hidden.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -100,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 0,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("Browser controls aren't at min-height.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertFalse("ControlsResizeView is true,"
                        + " but it should be false when the controls are hidden.",
                mFullscreenManager.controlsResizeView());

        // Now, scroll back to fully visible.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("Browser controls are hidden when they should be fully visible.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertTrue("Browser controls aren't fully visible.",
                mFullscreenManager.areBrowserControlsFullyVisible());
        // #controlsResizeView should be flipped back to true.
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mFullscreenManager.controlsResizeView());
    }

    @Test
    public void testControlsResizeViewChangesWithMinHeight() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabFullscreenObserver =
                mFullscreenManager.getTabFullscreenObserverForTesting();

        mFullscreenManager.setTopControlsHeight(topHeight, topMinHeight);
        mFullscreenManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // Initially, the controls should be fully visible.
        assertTrue("Browser controls aren't fully visible.",
                mFullscreenManager.areBrowserControlsFullyVisible());
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mFullscreenManager.controlsResizeView());

        // Scroll all the way to the min-height.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -75,
                /*bottomControlsOffsetY*/ 60, /*contentOffsetY*/ 25,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("Browser controls aren't at min-height.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertFalse("ControlsResizeView is true,"
                        + " but it should be false when the controls are at min-height.",
                mFullscreenManager.controlsResizeView());

        // Now, scroll back to fully visible.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("Browser controls are at min-height when they should be fully visible.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertTrue("Browser controls aren't fully visible.",
                mFullscreenManager.areBrowserControlsFullyVisible());
        // #controlsResizeView should be flipped back to true.
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mFullscreenManager.controlsResizeView());
    }

    @Test
    public void testControlsResizeViewWhenControlsAreNotIdle() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabFullscreenObserver =
                mFullscreenManager.getTabFullscreenObserverForTesting();

        mFullscreenManager.setTopControlsHeight(topHeight, topMinHeight);
        mFullscreenManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mFullscreenManager.controlsResizeView());

        // Scroll a little hide the controls partially.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -25,
                /*bottomControlsOffsetY*/ 20, /*contentOffsetY*/ 75,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("ControlsResizeView is false, but it should still be true.",
                mFullscreenManager.controlsResizeView());

        // Scroll controls all the way to the min-height.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -75,
                /*bottomControlsOffsetY*/ 60, /*contentOffsetY*/ 25,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("ControlsResizeView is true,"
                        + " but it should've flipped to false since the controls are idle now.",
                mFullscreenManager.controlsResizeView());

        // Scroll controls to show a little more.
        tabFullscreenObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -50,
                /*bottomControlsOffsetY*/ 40, /*contentOffsetY*/ 50,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("ControlsResizeView is true, but it should still be false.",
                mFullscreenManager.controlsResizeView());
    }

    // --- controlsResizeView tests
}
