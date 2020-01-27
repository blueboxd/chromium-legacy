// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common;

import androidx.annotation.Nullable;

/**
 * Class similar to Guava's Preconditions. Not all users of Feed can use Guava libraries so we
 * define our own here.
 */
public class Validators {
    private Validators() {}

    /**
     * Ensures that an object reference passed as a parameter to the calling method is not null.
     *
     * @param reference an object reference
     * @param debugString a debug string to add to the NPE if reference is null
     * @param formatArgs arguments to populate the debugString using String.format
     * @return the non-null reference that was validated
     * @throws NullPointerException if {@code reference} is null
     */
    @SuppressWarnings("AnnotateFormatMethod")
    public static <T> T checkNotNull(
            @Nullable T reference, String debugString, Object... formatArgs) {
        if (reference == null) {
            throw new NullPointerException(String.format(debugString, formatArgs));
        }
        return reference;
    }

    /**
     * Ensures that an object reference passed as a parameter to the calling method is not null.
     *
     * @param reference an object reference
     * @return the non-null reference that was validated
     * @throws NullPointerException if {@code reference} is null
     */
    public static <T> T checkNotNull(@Nullable T reference) {
        if (reference == null) {
            throw new NullPointerException();
        }
        return reference;
    }

    /**
     * Assert that {@code expression} is {@code true}.
     *
     * @param expression a boolean value that should be true
     * @param formatString a format string for the message for the IllegalStateException
     * @param formatArgs arguments to the format string
     * @throws IllegalStateException thrown if the expression is false
     */
    @SuppressWarnings("AnnotateFormatMethod")
    public static void checkState(boolean expression, String formatString, Object... formatArgs) {
        if (!expression) {
            throw new IllegalStateException(String.format(formatString, formatArgs));
        }
    }

    /**
     * Assert that {@code expression} is {@code true}.
     *
     * @param expression a boolean value that should be true
     * @throws IllegalStateException thrown if the expression is false
     */
    public static void checkState(boolean expression) {
        if (!expression) {
            throw new IllegalStateException();
        }
    }
}
