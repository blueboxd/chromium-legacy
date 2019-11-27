// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.functional;

/**
 * A functional interface that accepts an input and returns nothing. Unlike most functional
 * interfaces, Consumer is expected to cause side effects.
 *
 * <p>This interface should be used in a similar way to Java 8's Consumer interface.
 */
public interface Consumer<T> {
    /** Perform an operation using {@code input}. */
    void accept(T input);
}
