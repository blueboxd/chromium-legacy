// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.functional;

/** Function definition used by the Feed. This is a subset of the Java 8 defined Function. */
public interface Function<T, R> {
    /** Applies this function to the given argument. */
    R apply(T t);
}
