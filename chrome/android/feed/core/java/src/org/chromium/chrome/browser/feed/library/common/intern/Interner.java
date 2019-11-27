// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

/**
 * Similar to {@link String#intern} but for other immutable types. An interner will de-duplicate
 * objects that are identical so only one copy is stored in memory.
 */
public interface Interner<T> {
    /**
     * Returns a canonical representation for the give input object. If there is already an internal
     * object equal to the input object, then the internal object is returned. Otherwise, the input
     * object is added internally and a reference to it is returned.
     */
    T intern(T input);

    /** Clears the internally store objects. */
    void clear();

    /** Returns the number of the internally stored objects. */
    int size();
}
