// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;

/**
 * Manages the state of tab browser controls.
 */
public class TabBrowserControlsConstraintsHelper
        extends TabWebContentsUserData implements ImeEventObserver {
    private static final Class<TabBrowserControlsConstraintsHelper> USER_DATA_KEY =
            TabBrowserControlsConstraintsHelper.class;

    private final TabImpl mTab;
    private long mNativeTabBrowserControlsConstraintsHelper; // Lazily initialized in |update|
    private BrowserControlsVisibilityDelegate mVisibilityDelegate;

    public static void createForTab(Tab tab) {
        tab.getUserDataHost().setUserData(
                USER_DATA_KEY, new TabBrowserControlsConstraintsHelper(tab));
    }

    public static TabBrowserControlsConstraintsHelper get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Returns the current visibility constraints for the display of browser controls.
     * {@link BrowserControlsState} defines the valid return options.
     * @param tab Tab whose browser constrol state is looked into.
     * @return The current visibility constraints.
     */
    @BrowserControlsState
    public static int getConstraints(Tab tab) {
        if (tab == null || get(tab) == null) return BrowserControlsState.BOTH;
        return get(tab).getConstraints();
    }

    /**
     * Push state about whether or not the browser controls can show or hide to the renderer.
     * @param tab Tab object.
     */
    public static void updateEnabledState(Tab tab) {
        if (tab == null || get(tab) == null) return;
        get(tab).updateEnabledState();
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param tab Tab whose browser constrol state is looked into.
     * @param current The desired current state for the controls.  Pass
     *         {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *         should jump immediately.
     */
    public static void update(Tab tab, int current, boolean animate) {
        if (tab == null || get(tab) == null) return;
        get(tab).update(current, animate);
    }

    /** Constructor */
    private TabBrowserControlsConstraintsHelper(Tab tab) {
        super(tab);
        mTab = (TabImpl) tab;
        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                updateEnabledState();
            }

            @Override
            public void onInitialized(Tab tab, TabState tabState) {
                mVisibilityDelegate =
                        mTab.getDelegateFactory().createBrowserControlsVisibilityDelegate(tab);
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (isAttached) {
                    mVisibilityDelegate =
                            mTab.getDelegateFactory().createBrowserControlsVisibilityDelegate(tab);
                }
            }

            @Override
            public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
                if (mTab.isHidden()) return;
                if (isResponsive) {
                    updateEnabledState();
                } else {
                    update(BrowserControlsState.SHOWN, false);
                }
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                updateEnabledState();
            }

            @Override
            public void onDestroyed(Tab tab) {
                tab.removeObserver(this);
            }
        });
    }

    @Override
    public void destroyInternal() {
        if (mNativeTabBrowserControlsConstraintsHelper != 0) {
            TabBrowserControlsConstraintsHelperJni.get().onDestroyed(
                    mNativeTabBrowserControlsConstraintsHelper,
                    TabBrowserControlsConstraintsHelper.this);
        }
    }

    @Override
    public void initWebContents(WebContents webContents) {
        ImeAdapter.fromWebContents(webContents).addEventObserver(this);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    private void updateEnabledState() {
        if (mTab.isFrozen()) return;
        update(BrowserControlsState.BOTH, getConstraints() != BrowserControlsState.HIDDEN);
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param current The desired current state for the controls.  Pass
     *                {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    public void update(int current, boolean animate) {
        int constraints = getConstraints();

        // Do nothing if current and constraints conflict to avoid error in renderer.
        if ((constraints == BrowserControlsState.HIDDEN && current == BrowserControlsState.SHOWN)
                || (constraints == BrowserControlsState.SHOWN
                        && current == BrowserControlsState.HIDDEN)) {
            return;
        }
        if (mNativeTabBrowserControlsConstraintsHelper == 0) {
            mNativeTabBrowserControlsConstraintsHelper =
                    TabBrowserControlsConstraintsHelperJni.get().init(
                            TabBrowserControlsConstraintsHelper.this);
        }
        TabBrowserControlsConstraintsHelperJni.get().updateState(
                mNativeTabBrowserControlsConstraintsHelper,
                TabBrowserControlsConstraintsHelper.this, mTab.getWebContents(), constraints,
                current, animate);
    }

    /**
     * @return Whether hiding browser controls is enabled or not.
     */
    private boolean canAutoHide() {
        return mVisibilityDelegate != null ? mVisibilityDelegate.canAutoHideBrowserControls()
                                           : false;
    }

    /**
     * @return Whether showing browser controls is enabled or not.
     */
    public boolean canShow() {
        return mVisibilityDelegate != null ? mVisibilityDelegate.canShowBrowserControls() : false;
    }

    @BrowserControlsState
    private int getConstraints() {
        int constraints = BrowserControlsState.BOTH;
        if (!canShow()) {
            constraints = BrowserControlsState.HIDDEN;
        } else if (!canAutoHide()) {
            constraints = BrowserControlsState.SHOWN;
        }
        return constraints;
    }

    // ImeEventObserver

    @Override
    public void onNodeAttributeUpdated(boolean editable, boolean password) {
        if (mTab.isHidden()) return;
        updateEnabledState();
    }

    @NativeMethods
    interface Natives {
        long init(TabBrowserControlsConstraintsHelper caller);
        void onDestroyed(long nativeTabBrowserControlsConstraintsHelper,
                TabBrowserControlsConstraintsHelper caller);
        void updateState(long nativeTabBrowserControlsConstraintsHelper,
                TabBrowserControlsConstraintsHelper caller, WebContents webContents, int contraints,
                int current, boolean animate);
    }
}
