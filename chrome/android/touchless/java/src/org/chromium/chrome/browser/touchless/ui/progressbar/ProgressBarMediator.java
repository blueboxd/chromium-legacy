// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
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

    private static final int MINIMUM_DISPLAY_DURATION_MS = 3 * 1000;

    ProgressBarMediator(PropertyModel model, ActivityTabProvider activityTabProvider) {
        mProgressBarTabObserver = new ProgressBarTabObserver(activityTabProvider);
        mModel = model;
    }

    void onActivityResume() {
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

    private void onLoadingStopped() {
        mCanHideProgressBar = true;
        hide();
    }

    void destroy() {
        if (mHideTask != null) mHideTask.cancel(false);
        mProgressBarTabObserver.destroy();
    }

    private class ProgressBarTabObserver extends ActivityTabProvider.ActivityTabTabObserver {
        ProgressBarTabObserver(ActivityTabProvider tabProvider) {
            super(tabProvider);
        }

        @Override
        public void onPageLoadStarted(Tab tab, String url) {
            if (NativePageFactory.isNativePageUrl(url, tab.isIncognito())) {
                mModel.set(ProgressBarProperties.IS_ENABLED, false);
            } else {
                mModel.set(ProgressBarProperties.IS_ENABLED, true);
                mWasDisplayedForMinimumDuration = false;
                mCanHideProgressBar = false;
                show();
                mModel.set(ProgressBarProperties.PROGRESS_FRACTION, 0f);
            }
        }

        @Override
        public void onLoadProgressChanged(Tab tab, int progress) {
            if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;

            mModel.set(ProgressBarProperties.PROGRESS_FRACTION, progress / 100f);
            mModel.set(ProgressBarProperties.URL,
                    UrlUtilities.getDomainAndRegistry(tab.getUrl(), false));
        }

        @Override
        public void onCrash(Tab tab) {
            onLoadingStopped();
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            onLoadingStopped();
        }

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            onLoadingStopped();
        }
    }
}