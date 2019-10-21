// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;

/**
 * This class was migrated to embedder_support; this forwarding wrapper exists to avoid breaking
 * downstream code until it's updated.
 */
public class ResourcesContextWrapperFactory {
    private ResourcesContextWrapperFactory() {}

    public static Context get(Context ctx) {
        return ClassLoaderContextWrapperFactory.get(ctx);
    }
}
