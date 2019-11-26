// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewloggingupdater;

import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.OneShotVisibilityLoggingListener;

/** Extension of {@link OneShotVisibilityLoggingListener} that allows for re-logging visibility. */
public class ResettableOneShotVisibilityLoggingListener extends OneShotVisibilityLoggingListener {
    public ResettableOneShotVisibilityLoggingListener(LoggingListener loggingListener) {
        super(loggingListener);
    }

    /**
     * Resets whether this view has been logged as visible. This will result in the view being
     * re-logged as visible once it is visible.
     */
    public void reset() {
        mViewLogged = false;
    }
}
