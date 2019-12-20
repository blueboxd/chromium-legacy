// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.net.NetworkChangeNotifierAutoDetect;

/**
 * Registration policy to make sure we only listen to network changes when
 * WebLayer is loaded in a fragment.
 */
public class WebLayerNetworkChangeNotifierRegistrationPolicy
        extends NetworkChangeNotifierAutoDetect.RegistrationPolicy
        implements BrowserFragmentImpl.Observer {
    @Override
    protected void init(NetworkChangeNotifierAutoDetect notifier) {
        super.init(notifier);
        BrowserFragmentImpl.addObserver(this);
    }

    @Override
    protected void destroy() {
        BrowserFragmentImpl.removeObserver(this);
    }

    // BrowserFragmentImpl.Observer
    @Override
    public void onFirstBrowserFragmentAttached() {
        register();
    }

    @Override
    public void onLastBrowserFragmentDetached() {
        unregister();
    }
}
