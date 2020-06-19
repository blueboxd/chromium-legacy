// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.ui.util.TokenHolder;

/**
 * Unit tests for ToolbarAppMenuManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarAppMenuManagerTest {
    @Mock
    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    @Mock
    private Activity mActivity;
    @Mock
    private TopToolbarCoordinator mToolbar;
    @Mock
    private ToolbarAppMenuManager.SetFocusFunction mFocusFunction;
    @Mock
    private AppMenuCoordinator mAppMenuCoordinator;
    @Mock
    private AppMenuHandler mAppMenuHandler;
    @Mock
    private AppMenuButtonHelper mAppMenuButtonHelper;
    @Mock
    MenuButton mMenuButton;
    @Mock
    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    @Mock
    private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock
    private Runnable mRequestRenderRunnable;

    private UpdateMenuItemHelper.MenuUiState mMenuUiState;
    private ObservableSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private ToolbarAppMenuManager mToolbarAppMenuManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mAppMenuHandler).when(mAppMenuCoordinator).getAppMenuHandler();
        doReturn(mAppMenuButtonHelper).when(mAppMenuHandler).createAppMenuButtonHelper();
        doReturn(mAppMenuPropertiesDelegate)
                .when(mAppMenuCoordinator)
                .getAppMenuPropertiesDelegate();
        doReturn(mMenuButton).when(mToolbar).getMenuButtonWrapper();
        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        mAppMenuSupplier = new ObservableSupplierImpl<>();
        mMenuUiState = new UpdateMenuItemHelper.MenuUiState();
        doReturn(mMenuUiState).when(mUpdateMenuItemHelper).getUiState();

        mToolbarAppMenuManager =
                new ToolbarAppMenuManager(mAppMenuSupplier, mControlsVisibilityDelegate, mActivity,
                        mToolbar, mFocusFunction, mRequestRenderRunnable, true, () -> false);
    }

    @Test
    public void testInitialization() {
        mAppMenuSupplier.set(mAppMenuCoordinator);
        verify(mAppMenuHandler).addObserver(mToolbarAppMenuManager);
        verify(mAppMenuHandler).createAppMenuButtonHelper();
    }

    @Test
    public void testAppMenuVisiblityChange_badgeShowing() {
        mAppMenuSupplier.set(mAppMenuCoordinator);
        doReturn(42)
                .when(mControlsVisibilityDelegate)
                .showControlsPersistentAndClearOldToken(TokenHolder.INVALID_TOKEN);
        doReturn(true).when(mMenuButton).isShowingAppMenuUpdateBadge();
        doReturn(true).when(mToolbar).isShowingAppMenuUpdateBadge();
        mToolbarAppMenuManager.onMenuVisibilityChanged(true);

        verify(mFocusFunction).setFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);
        verify(mToolbar).removeAppMenuUpdateBadge(true);
        verify(mUpdateMenuItemHelper).onMenuButtonClicked();

        mToolbarAppMenuManager.onMenuVisibilityChanged(false);
        verify(mControlsVisibilityDelegate).releasePersistentShowingToken(42);
    }

    @Test
    public void testAppMenuHighlightChange() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        doReturn(42)
                .when(mControlsVisibilityDelegate)
                .showControlsPersistentAndClearOldToken(TokenHolder.INVALID_TOKEN);

        mToolbarAppMenuManager.onMenuHighlightChanged(true);
        verify(mMenuButton).setMenuButtonHighlight(true);

        mToolbarAppMenuManager.onMenuHighlightChanged(false);
        verify(mMenuButton).setMenuButtonHighlight(false);
        verify(mControlsVisibilityDelegate).releasePersistentShowingToken(42);
    }

    @Test
    public void testAppMenuUpdateBadge() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        doReturn(true).when(mActivity).isDestroyed();
        mToolbarAppMenuManager.updateStateChanged();

        verify(mToolbar, never()).showAppMenuUpdateBadge();
        verify(mRequestRenderRunnable, never()).run();
        verify(mToolbar, never()).removeAppMenuUpdateBadge(false);

        doReturn(false).when(mActivity).isDestroyed();
        mToolbarAppMenuManager.updateStateChanged();

        verify(mToolbar, never()).showAppMenuUpdateBadge();
        verify(mRequestRenderRunnable, never()).run();
        verify(mToolbar, times(1)).removeAppMenuUpdateBadge(false);

        mMenuUiState.buttonState = new UpdateMenuItemHelper.MenuButtonState();
        mToolbarAppMenuManager.updateStateChanged();

        verify(mToolbar).showAppMenuUpdateBadge();
        verify(mRequestRenderRunnable).run();
        verify(mToolbar, times(1)).removeAppMenuUpdateBadge(false);
    }

    @Test
    public void testAppMenuUpdateBadge_activityShouldNotShow() {
        ToolbarAppMenuManager newManager =
                new ToolbarAppMenuManager(mAppMenuSupplier, mControlsVisibilityDelegate, mActivity,
                        mToolbar, mFocusFunction, mRequestRenderRunnable, false, () -> false);

        doReturn(true).when(mActivity).isDestroyed();
        newManager.updateStateChanged();

        verify(mToolbar, never()).showAppMenuUpdateBadge();
        verify(mRequestRenderRunnable, never()).run();
        verify(mToolbar, never()).removeAppMenuUpdateBadge(false);

        doReturn(false).when(mActivity).isDestroyed();
        newManager.updateStateChanged();

        verify(mToolbar, never()).showAppMenuUpdateBadge();
        verify(mRequestRenderRunnable, never()).run();
        verify(mToolbar, never()).removeAppMenuUpdateBadge(false);

        mMenuUiState.buttonState = new UpdateMenuItemHelper.MenuButtonState();
        newManager.updateStateChanged();

        verify(mToolbar, never()).showAppMenuUpdateBadge();
        verify(mRequestRenderRunnable, never()).run();
        verify(mToolbar, never()).removeAppMenuUpdateBadge(false);
    }
}
