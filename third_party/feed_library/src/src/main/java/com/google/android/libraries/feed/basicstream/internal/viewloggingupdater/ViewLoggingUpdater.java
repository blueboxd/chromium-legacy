// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewloggingupdater;

import com.google.android.libraries.feed.common.feedobservable.FeedObservable;

/** {@link FeedObservable} to reset whether views have been logged as visible. */
public class ViewLoggingUpdater extends FeedObservable<ResettableOneShotVisibilityLoggingListener> {
    /** Resets views logging state to allow them to be re-logged as visible. */
    public void resetViewTracking() {
        for (ResettableOneShotVisibilityLoggingListener loggingListener : mObservers) {
            loggingListener.reset();
        }
    }
}
