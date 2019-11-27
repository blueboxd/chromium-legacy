// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.functional;

/**
 * A functional interface which represents a supplier of results.
 *
 * <p>This interface should be used in a similar way to Java 8's Supplier interface.
 */
public interface Supplier<T> {
    /** Gets a result. */
    T get();
}
