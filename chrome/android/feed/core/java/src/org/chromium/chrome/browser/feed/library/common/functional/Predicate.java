// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.functional;

/** Predicate support for the Feed. This is a copy of the Java 8 defined Predicate. */
public interface Predicate<T> {
    /** Evaluates this predicate on the argument. */
    boolean test(T t);
}
