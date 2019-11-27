// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.functional;

/**
 * Interface which externalizes the action of committing a mutation from the class implementing the
 * mutation. This is a common design pattern used to mutate the library components. The mutation
 * builder will call a {@code Committer} to commit the accumulated changes.
 *
 * @param <R> The return value
 * @param <C> A class containing the changes which will be committed.
 */
public interface Committer<R, C> {
    /** Commit the change T, and optionally return a value R */
    R commit(C change);
}
