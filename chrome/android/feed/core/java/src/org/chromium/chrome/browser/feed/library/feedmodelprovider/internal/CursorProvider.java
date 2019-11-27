// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;

/** Interface which provides a {@link ModelCursor} */
public interface CursorProvider {
    /**
     * Create a {@link ModelCursor} for the parent, this will iterate against all the children
     * currently contained by the parent container.
     */
    ModelCursor getCursor(String parentId);
}
