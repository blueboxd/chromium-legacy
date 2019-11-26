// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.LogDataCallback;

/** Wrapper class to hold all host-related objects. */
public class HostProviders {
    private final AssetProvider mAssetProvider;
    private final CustomElementProvider mCustomElementProvider;
    private final HostBindingProvider mHostBindingProvider;
    /*@Nullable*/ private final LogDataCallback mLogDataCallback;

    public HostProviders(AssetProvider assetProvider, CustomElementProvider customElementProvider,
            HostBindingProvider hostBindingProvider,
            /*@Nullable*/ LogDataCallback logDataCallback) {
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

    /*@Nullable*/
    public LogDataCallback getLogDataCallback() {
        return mLogDataCallback;
    }
}
