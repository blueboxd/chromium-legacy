// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.weblayer_private.WebLayerImpl;

import java.io.File;

/**
 * WebLayer is responsible for initializing state necessary to use* any of the classes in web layer.
 */
public final class WebLayer {
    private static WebLayer sInstance;
    private WebLayerImpl mWebLayer;

    public static WebLayer getInstance() {
        if (sInstance == null) {
            sInstance = new WebLayer();
        }
        return sInstance;
    }

    WebLayer() {
        mWebLayer = new WebLayerImpl();
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public void destroy() {
        // TODO: implement me.
        sInstance = null;
    }

    /**
     * Creates a new Profile with the given path. Pass in an empty path for an in-memory profile.
     */
    public Profile createProfile(File path) {
        return new Profile(path);
    }
}
