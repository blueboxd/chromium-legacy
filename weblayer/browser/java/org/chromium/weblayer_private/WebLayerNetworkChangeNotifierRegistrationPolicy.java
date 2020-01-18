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
        extends NetworkChangeNotifierAutoDetect.RegistrationPolicy implements BrowserImpl.Observer {
    private static int sBrowserCount;

    @Override
    protected void init(NetworkChangeNotifierAutoDetect notifier) {
        super.init(notifier);
        BrowserImpl.addObserver(this);
    }

    @Override
    protected void destroy() {
        BrowserImpl.removeObserver(this);
    }

    // BrowserImpl.Observer
    @Override
    public void onBrowserCreated() {
        if (sBrowserCount == 0) {
            register();
        }
        sBrowserCount++;
    }

    @Override
    public void onBrowserDestroyed() {
        assert (sBrowserCount > 0);
        sBrowserCount--;
        if (sBrowserCount == 0) {
            unregister();
        }
    }
}
