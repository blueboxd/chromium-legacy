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
    private final AssetProvider assetProvider;
    private final CustomElementProvider customElementProvider;
    private final HostBindingProvider hostBindingProvider;
    /*@Nullable*/ private final LogDataCallback logDataCallback;

    public HostProviders(AssetProvider assetProvider, CustomElementProvider customElementProvider,
            HostBindingProvider hostBindingProvider,
            /*@Nullable*/ LogDataCallback logDataCallback) {
        this.assetProvider = assetProvider;
        this.customElementProvider = customElementProvider;
        this.hostBindingProvider = hostBindingProvider;
        this.logDataCallback = logDataCallback;
    }

    public AssetProvider getAssetProvider() {
        return assetProvider;
    }

    public CustomElementProvider getCustomElementProvider() {
        return customElementProvider;
    }

    public HostBindingProvider getHostBindingProvider() {
        return hostBindingProvider;
    }

    /*@Nullable*/
    public LogDataCallback getLogDataCallback() {
        return logDataCallback;
    }
}
