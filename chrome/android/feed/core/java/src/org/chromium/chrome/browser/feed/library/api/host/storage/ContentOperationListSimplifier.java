// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import java.util.List;

/**
 * Simplifies a {@link List<ContentOperation>} by combining and removing Operations according to the
 * methods in {@link ContentMutation.Builder}.
 */
final class ContentOperationListSimplifier {
    /**
     * Returns a new {@link List<ContentOperation>}, which is a simplification of {@code
     * contentOperations}.
     *
     * <p>The returned list will have a length at most equal to {@code contentOperations}.
     */
    List<ContentOperation> simplify(List<ContentOperation> contentOperations) {
        // TODO: implement
        return contentOperations;
    }
}
