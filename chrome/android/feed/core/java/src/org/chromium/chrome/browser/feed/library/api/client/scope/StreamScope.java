// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.scope;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;

/** Allows interacting with the Feed library on a per-stream level */
public interface StreamScope {
    /** Returns the current {@link Stream}. */
    Stream getStream();

    /** Returns the current {@link ModelProviderFactory}. */
    ModelProviderFactory getModelProviderFactory();
}
