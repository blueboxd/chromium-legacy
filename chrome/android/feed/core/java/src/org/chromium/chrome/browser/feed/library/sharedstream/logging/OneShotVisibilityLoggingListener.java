// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

/** Implementation of a {@link LoggingListener} that only notifies a listener of the first view. */
public class OneShotVisibilityLoggingListener implements LoggingListener {
    private final LoggingListener mLoggingListener;
    protected boolean mViewLogged;

    public OneShotVisibilityLoggingListener(LoggingListener loggingListener) {
        this.mLoggingListener = loggingListener;
    }

    @Override
    public void onViewVisible() {
        if (mViewLogged) {
            return;
        }

        mLoggingListener.onViewVisible();
        mViewLogged = true;
    }

    @Override
    public void onContentClicked() {
        mLoggingListener.onContentClicked();
    }

    @Override
    public void onContentSwiped() {
        mLoggingListener.onContentSwiped();
    }

    @Override
    public void onScrollStateChanged(int newScrollState) {
        mLoggingListener.onScrollStateChanged(newScrollState);
    }
}
