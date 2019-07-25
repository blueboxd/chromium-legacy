// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import android.support.annotation.IntDef;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.Tracker.DisplayLockHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.touchless.CursorObserver;
import org.chromium.ui.touchless.TouchlessEventHandler;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.FutureTask;

/**
 * Controls the UI model for key functions IPH.
 */
public class KeyFunctionsIPHMediator implements CursorObserver {
    private final PropertyModel mModel;
    private final KeyFunctionsIPHTabObserver mKeyFunctionsIPHTabObserver;
    private final ChromePreferenceManager mChromePreferenceManager;
    private final Tracker mTracker;
    private FutureTask mHideTask;
    private DisplayLockHandle mDisplayLockHandle;
    private int mPageLoadCount;
    private boolean mIsFallbackCursorModeOn;
    private boolean mShowedWhenPageLoadStarted;

    private static final long DISPLAY_DURATION_MS = 3000;

    // For the first INTRODUCTORY_SESSIONS sessions, show the IPH every INTRODUCTORY_PAGE_LOAD_CYCLE
    // page loads.
    static final int INTRODUCTORY_SESSIONS = 6;
    static final int INTRODUCTORY_PAGE_LOAD_CYCLE = 3;

    @IntDef({DisplayCause.PAGE_LOAD_STARTED, DisplayCause.PAGE_LOAD_FINISHED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DisplayCause {
        int PAGE_LOAD_STARTED = 0;
        int PAGE_LOAD_FINISHED = 1;
    }

    KeyFunctionsIPHMediator(PropertyModel model, ActivityTabProvider activityTabProvider,
            ChromePreferenceManager chromePreferenceManager, Tracker tracker) {
        mModel = model;
        mKeyFunctionsIPHTabObserver = new KeyFunctionsIPHTabObserver(activityTabProvider);
        mChromePreferenceManager = chromePreferenceManager;
        mTracker = tracker;
        TouchlessEventHandler.addCursorObserver(this);
    }

    @Override
    public void onCursorVisibilityChanged(boolean isCursorVisible) {}

    @Override
    public void onFallbackCursorModeToggled(boolean isOn) {
        mIsFallbackCursorModeOn = isOn;
    }

    private void show(@DisplayCause int displayCause) {
        if (displayCause == DisplayCause.PAGE_LOAD_STARTED) {
            mShowedWhenPageLoadStarted = false;
            int totalSessionCount = mChromePreferenceManager.readInt(
                    ChromePreferenceManager.TOUCHLESS_BROWSING_SESSION_COUNT);
            if (totalSessionCount <= INTRODUCTORY_SESSIONS
                    && mPageLoadCount % INTRODUCTORY_PAGE_LOAD_CYCLE != 1) {
                return;
            }
            if (totalSessionCount > INTRODUCTORY_SESSIONS && mPageLoadCount > 1) return;
        } else if (mShowedWhenPageLoadStarted && displayCause == DisplayCause.PAGE_LOAD_FINISHED) {
            // If we have already shown the IPH when page load started, we should avoid showing it
            // again when page load is finished.
            return;
        }

        // If we are already showing this IPH, we should release the lock.
        if (mDisplayLockHandle != null) mDisplayLockHandle.release();
        mDisplayLockHandle = mTracker.acquireDisplayLock();
        // If another IPH UI is currently shown, return.
        if (mDisplayLockHandle == null) return;

        if (displayCause == DisplayCause.PAGE_LOAD_STARTED) mShowedWhenPageLoadStarted = true;

        if (mHideTask != null) mHideTask.cancel(false);
        mHideTask = new FutureTask<Void>(() -> {
            mModel.set(KeyFunctionsIPHProperties.IS_VISIBLE, false);
            mDisplayLockHandle.release();
            mDisplayLockHandle = null;
            return null;
        });
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, mHideTask, DISPLAY_DURATION_MS);
        mModel.set(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE, mIsFallbackCursorModeOn);
        mModel.set(KeyFunctionsIPHProperties.IS_VISIBLE, true);
    }

    void destroy() {
        TouchlessEventHandler.removeCursorObserver(this);
        mKeyFunctionsIPHTabObserver.destroy();
        if (mHideTask != null) mHideTask.cancel(false);
    }

    private class KeyFunctionsIPHTabObserver extends ActivityTabProvider.ActivityTabTabObserver {
        KeyFunctionsIPHTabObserver(ActivityTabProvider tabProvider) {
            super(tabProvider);
        }

        @Override
        public void onPageLoadStarted(Tab tab, String url) {
            if (NativePageFactory.isNativePageUrl(url, tab.isIncognito())) return;

            mPageLoadCount++;
            show(DisplayCause.PAGE_LOAD_STARTED);
        }

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            if (NativePageFactory.isNativePageUrl(url, tab.isIncognito())) return;

            TabDistillabilityProvider distillabilityProvider = TabDistillabilityProvider.get(tab);
            if (distillabilityProvider != null
                    && distillabilityProvider.isDistillabilityDetermined()
                    && !distillabilityProvider.isMobileOptimized()) {
                show(DisplayCause.PAGE_LOAD_FINISHED);
            }
        }
    }
}
