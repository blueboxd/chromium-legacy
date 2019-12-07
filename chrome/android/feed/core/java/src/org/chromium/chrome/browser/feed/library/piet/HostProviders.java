// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;

/** Wrapper class to hold all host-related objects. */
public class HostProviders {
    private final AssetProvider mAssetProvider;
    private final CustomElementProvider mCustomElementProvider;
    private final HostBindingProvider mHostBindingProvider;
    @Nullable
    private final LogDataCallback mLogDataCallback;

    public HostProviders(AssetProvider assetProvider, CustomElementProvider customElementProvider,
            HostBindingProvider hostBindingProvider, @Nullable LogDataCallback logDataCallback) {
        this.mAssetProvider = assetProvider;
        this.mCustomElementProvider = customElementProvider;
        this.mHostBindingProvider = hostBindingProvider;
        this.mLogDataCallback = logDataCallback;
    }

    public AssetProvider getAssetProvider() {
        return mAssetProvider;
    }

    public CustomElementProvider getCustomElementProvider() {
        return mCustomElementProvider;
    }

    public HostBindingProvider getHostBindingProvider() {
        return mHostBindingProvider;
    }

    @Nullable
    public LogDataCallback getLogDataCallback() {
        return mLogDataCallback;
    }
}
