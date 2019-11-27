// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.scope;

import org.chromium.chrome.browser.feed.library.api.client.scope.StreamScope;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;

/** Per-stream instance of the feed library. */
public final class FeedStreamScope implements StreamScope {
    private final Stream mStream;
    private final ModelProviderFactory mModelProviderFactory;

    public FeedStreamScope(Stream stream, ModelProviderFactory modelProviderFactory) {
        this.mStream = stream;
        this.mModelProviderFactory = modelProviderFactory;
    }

    @Override
    public Stream getStream() {
        return mStream;
    }

    @Override
    public ModelProviderFactory getModelProviderFactory() {
        return mModelProviderFactory;
    }
}
