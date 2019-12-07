// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common;

import androidx.annotation.Nullable;

/** Wrapper that allows callbacks to return a value as well as whether the call was successful. */
public class Result<T> {
    @Nullable
    private final T mValue;
    private final boolean mIsSuccessful;

    private Result(@Nullable T value, boolean isSuccessful) {
        this.mValue = value;
        this.mIsSuccessful = isSuccessful;
    }

    public static <T> Result<T> success(T value) {
        return new Result<>(value, /* isSuccessful= */ true);
    }

    public static <T> Result<T> failure() {
        return new Result<>(null, false);
    }

    /** Retrieves the value for the result. */
    public T getValue() {
        if (!mIsSuccessful) {
            throw new IllegalStateException("Cannot retrieve value for failed result");
        }
        return Validators.checkNotNull(mValue);
    }

    // TODO: replace isSuccessful with failed()
    public boolean isSuccessful() {
        return mIsSuccessful;
    }
}
