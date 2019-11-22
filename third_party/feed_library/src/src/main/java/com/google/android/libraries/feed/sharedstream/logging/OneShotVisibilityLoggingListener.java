// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

/** Implementation of a {@link LoggingListener} that only notifies a listener of the first view. */
public class OneShotVisibilityLoggingListener implements LoggingListener {
    private final LoggingListener loggingListener;
    protected boolean viewLogged;

    public OneShotVisibilityLoggingListener(LoggingListener loggingListener) {
        this.loggingListener = loggingListener;
    }

    @Override
    public void onViewVisible() {
        if (viewLogged) {
            return;
        }

        loggingListener.onViewVisible();
        viewLogged = true;
    }

    @Override
    public void onContentClicked() {
        loggingListener.onContentClicked();
    }

    @Override
    public void onContentSwiped() {
        loggingListener.onContentSwiped();
    }

    @Override
    public void onScrollStateChanged(int newScrollState) {
        loggingListener.onScrollStateChanged(newScrollState);
    }
}
