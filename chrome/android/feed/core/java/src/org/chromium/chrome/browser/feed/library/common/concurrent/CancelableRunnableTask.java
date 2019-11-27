// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.common.concurrent;

import java.util.concurrent.atomic.AtomicBoolean;

/** A thread safe runnable that can be canceled. */
public class CancelableRunnableTask implements CancelableTask, Runnable {
    private final AtomicBoolean mCanceled = new AtomicBoolean(false);
    private final Runnable mRunnable;

    public CancelableRunnableTask(Runnable runnable) {
        this.mRunnable = runnable;
    }

    @Override
    public void run() {
        if (!mCanceled.get()) {
            mRunnable.run();
        }
    }

    @Override
    public boolean canceled() {
        return mCanceled.get();
    }

    @Override
    public void cancel() {
        mCanceled.set(true);
    }
}
