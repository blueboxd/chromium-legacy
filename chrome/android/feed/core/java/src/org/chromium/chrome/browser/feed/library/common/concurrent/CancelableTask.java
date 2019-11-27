// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.common.concurrent;

/** Interface for a task that can be canceled. */
public interface CancelableTask {
    /** Returns if the task has been canceled. */
    boolean canceled();

    /** Cancels the task to prevent it from running. */
    void cancel();
}
