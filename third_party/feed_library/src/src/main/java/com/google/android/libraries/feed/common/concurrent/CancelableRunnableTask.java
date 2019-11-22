// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.common.concurrent;

import java.util.concurrent.atomic.AtomicBoolean;

/** A thread safe runnable that can be canceled. */
public class CancelableRunnableTask implements CancelableTask, Runnable {
    private final AtomicBoolean canceled = new AtomicBoolean(false);
    private final Runnable runnable;

    public CancelableRunnableTask(Runnable runnable) {
        this.runnable = runnable;
    }

    @Override
    public void run() {
        if (!canceled.get()) {
            runnable.run();
        }
    }

    @Override
    public boolean canceled() {
        return canceled.get();
    }

    @Override
    public void cancel() {
        canceled.set(true);
    }
}
