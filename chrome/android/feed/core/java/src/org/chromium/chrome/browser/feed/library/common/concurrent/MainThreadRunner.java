// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent;

import android.os.Handler;
import android.os.Looper;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.concurrent.Executor;

/** Executes a task on the main thread (UI thread) with optional delay. */
/*@DoNotMock("Use org.chromium.chrome.browser.feed.library.common.concurrent.FakeMainThreadRunner
 * instead")*/
public class MainThreadRunner {
    private static final String TAG = "MainThreadRunner";
    private static final Handler sHandler = new Handler(Looper.getMainLooper());

    private final Executor mExecutor;

    public MainThreadRunner() {
        this.mExecutor = sHandler::post;
    }

    /** Executes the {@code runnable} on the {@link Executor} used to initialize this class. */
    public void execute(String name, Runnable runnable) {
        Logger.i(TAG, "Running task [%s] on the Main Thread", name);
        mExecutor.execute(runnable);
    }

    public CancelableTask executeWithDelay(String name, Runnable runnable, long delayMs) {
        CancelableRunnableTask cancelable = new CancelableRunnableTask(runnable);
        Logger.i(TAG, "Running task [%s] on the Main Thread with a delay of %d milliseconds", name,
                delayMs);
        sHandler.postDelayed(cancelable, delayMs);
        return cancelable;
    }
}
