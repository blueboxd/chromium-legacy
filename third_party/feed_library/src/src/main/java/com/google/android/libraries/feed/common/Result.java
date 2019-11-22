// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common;

/** Wrapper that allows callbacks to return a value as well as whether the call was successful. */
public class Result<T> {
    /*@Nullable*/ private final T value;
    private final boolean isSuccessful;

    private Result(/*@Nullable*/ T value, boolean isSuccessful) {
        this.value = value;
        this.isSuccessful = isSuccessful;
    }

    public static <T> Result<T> success(T value) {
        return new Result<>(value, /* isSuccessful= */ true);
    }

    public static <T> Result<T> failure() {
        return new Result<>(null, false);
    }

    /** Retrieves the value for the result. */
    public T getValue() {
        if (!isSuccessful) {
            throw new IllegalStateException("Cannot retrieve value for failed result");
        }
        return Validators.checkNotNull(value);
    }

    // TODO: replace isSuccessful with failed()
    public boolean isSuccessful() {
        return isSuccessful;
    }
}
