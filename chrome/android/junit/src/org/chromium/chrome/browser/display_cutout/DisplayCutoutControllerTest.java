// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.view.Window;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Tests for {@link DisplayCutoutController} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisplayCutoutControllerTest {
    @Mock
    private TabImpl mTab;

    @Mock
    private WindowAndroid mWindowAndroid;

    @Mock
    private Window mWindow;

    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Mock
    private ChromeActivity mChromeActivity;

    @Mock
    private InsetObserverView mInsetObserver;

    private DisplayCutoutController mDisplayCutoutController;

    private WeakReference<Activity> mActivityRef;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityRef = new WeakReference<>(mChromeActivity);

        // Mock dependency on InsetObserverView.

        when(mChromeActivity.getWindow()).thenReturn(mWindow);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
        when(mChromeActivity.getInsetObserverView()).thenReturn(mInsetObserver);

        mDisplayCutoutController = spy(new DisplayCutoutController(mTab));
    }

    @Test
    @SmallTest
    public void testViewportFitUpdate() {
        verify(mDisplayCutoutController, never()).maybeUpdateLayout();

        mDisplayCutoutController.setViewportFit(ViewportFit.COVER);
        verify(mDisplayCutoutController).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testViewportFitUpdateNotChanged() {
        verify(mDisplayCutoutController, never()).maybeUpdateLayout();

        mDisplayCutoutController.setViewportFit(ViewportFit.AUTO);
        verify(mDisplayCutoutController, never()).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenAutoAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutController.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutController.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverForcedAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutController.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenContainAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutController.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenAutoAndNotInteractable() {
        mDisplayCutoutController.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverAndNotInteractable() {
        mDisplayCutoutController.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverForcedAndNotInteractable() {
        mDisplayCutoutController.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenContainAndNotInteractable() {
        mDisplayCutoutController.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals("LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT",
                mDisplayCutoutController.getDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testLayoutOnInteractability_True() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutController and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, true);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayoutOnInteractability_False() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutController and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayoutOnShown() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutController and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onShown(mTab, TabSelectionType.FROM_NEW);
        verify(mWindow).getAttributes();
    }
}
