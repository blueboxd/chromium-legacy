// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.actionparser;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;

/** Factory for {@link ActionParser}. */
public interface ActionParserFactory {
    /**
     * Builds the ActionParser.
     *
     * @param contentMetadata A {@link Supplier} for {@link ContentMetadata} required for the {@link
     *     org.chromium.chrome.browser.feed.library.api.host.action.
     *      ActionApi#downloadUrl(ContentMetadata)}
     *     action. If the {@link Supplier} returns {@code null}, the download action will be
     *     suppressed. A {@link Supplier} is used instead of just a {@link ContentMetadata} to save
     * on memory and startup time. The {@link Supplier} will not be accessed until an action is
     * taken that requires it.
     */
    ActionParser build(Supplier<ContentMetadata> contentMetadata);
}
