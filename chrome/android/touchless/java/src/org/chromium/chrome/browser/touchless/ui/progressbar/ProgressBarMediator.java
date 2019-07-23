// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.touchless.TouchlessUrlUtilities;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.FutureTask;

/**
 * This part of the touchless progress bar component controls the UI model based on the status of
 * the current tab and key events.
 */
public class ProgressBarMediator {
    private final PropertyModel mModel;
    private final ProgressBarTabObserver mProgressBarTabObserver;
    private FutureTask mHideTask;
    private boolean mCanHideProgressBar;
    private boolean mWasDisplayedForMinimumDuration;
    private boolean mSkipShowingOnNextActivityStart;

    private static final int MINIMUM_DISPLAY_DURATION_MS = 3 * 1000;
    private static final int MAXIMUM_PROGRESS = 100;

    ProgressBarMediator(PropertyModel model, ActivityTabProvider activityTabProvider) {
        mProgressBarTabObserver = new ProgressBarTabObserver(activityTabProvider);
        mModel = model;
    }

    void skipShowingOnNextActivityStart() {
        mSkipShowingOnNextActivityStart = true;
    }

    void onActivityStart() {
        if (mSkipShowingOnNextActivityStart) {
            mSkipShowingOnNextActivityStart = false;
            return;
        }

        if (mModel.get(ProgressBarProperties.IS_ENABLED)) show();
    }

    private void show() {
        if (mHideTask != null) mHideTask.cancel(false);
        mHideTask = new FutureTask<Void>(() -> {
            mWasDisplayedForMinimumDuration = true;
            hide();
            return null;
        });
        PostTask.postDelayedTask(
                UiThreadTaskTraits.DEFAULT, mHideTask, MINIMUM_DISPLAY_DURATION_MS);
        mModel.set(ProgressBarProperties.IS_VISIBLE, true);
    }

    private void hide() {
        if (!mCanHideProgressBar || !mWasDisplayedForMinimumDuration) return;

        mModel.set(ProgressBarProperties.IS_VISIBLE, false);
    }

    private void startLoadProgress() {
        mWasDisplayedForMinimumDuration = false;
        mCanHideProgressBar = false;

        mModel.set(ProgressBarProperties.PROGRESS_FRACTION, 0f);
        show();
    }

    private void finishLoadProgress() {
        mCanHideProgressBar = true;
        hide();
    }

    private void updateLoadProgress(int progress) {
        mModel.set(ProgressBarProperties.PROGRESS_FRACTION, progress / ((float) MAXIMUM_PROGRESS));
        if (progress == MAXIMUM_PROGRESS) finishLoadProgress();
    }

    void destroy() {
        if (mHideTask != null) mHideTask.cancel(false);
        mProgressBarTabObserver.destroy();
    }

    // This class follows the same logic used in ToolbarManager#mTabObserver for updating the shown
    // URL as well as updating the progress bar.
    private class ProgressBarTabObserver extends ActivityTabProvider.ActivityTabTabObserver {
        ProgressBarTabObserver(ActivityTabProvider tabProvider) {
            super(tabProvider);
        }

        @Override
        public void onDidStartNavigation(Tab tab, NavigationHandle navigation) {
            if (!navigation.isInMainFrame()) return;

            if (tab.getWebContents() != null
                    && tab.getWebContents().getNavigationController() != null
                    && tab.getWebContents().getNavigationController().isInitialNavigation()) {
                updateUrl(tab);
            }

            if (navigation.isSameDocument()) return;

            if (NativePageFactory.isNativePageUrl(navigation.getUrl(), tab.isIncognito())) {
                mModel.set(ProgressBarProperties.IS_ENABLED, false);
                finishLoadProgress();
                return;
            }

            mModel.set(ProgressBarProperties.IS_ENABLED, true);
            startLoadProgress();
            updateLoadProgress(tab.getProgress());
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            updateUrl(tab);
        }

        @Override
        public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
            updateUrl(tab);
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            if (!toDifferentDocument) return;

            // If we made some progress, fast-forward to complete.
            if (tab.getProgress() < MAXIMUM_PROGRESS) {
                updateLoadProgress(MAXIMUM_PROGRESS);
            } else {
                finishLoadProgress();
            }
        }

        @Override
        public void onLoadProgressChanged(Tab tab, int progress) {
            if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;

            updateLoadProgress(progress);
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            if (!didStartLoad) return;

            updateUrl(tab);
            if (didFinishLoad) finishLoadProgress();
        }

        @Override
        public void onCrash(Tab tab) {
            finishLoadProgress();
        }

        private void updateUrl(Tab tab) {
            if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;

            mModel.set(ProgressBarProperties.URL,
                    TouchlessUrlUtilities.getUrlForDisplay(tab.getUrl()));
        }
    }
}