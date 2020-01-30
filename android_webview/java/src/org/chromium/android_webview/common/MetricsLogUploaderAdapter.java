// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.base.Consumer;

/**
 * This class adapts from the uploader requirements in //components/embedder_support
 * to PlatformServiceBridge.
 */
public class MetricsLogUploaderAdapter implements Consumer<byte[]> {
    private final PlatformServiceBridge mPlatformServiceBridge;

    public MetricsLogUploaderAdapter(PlatformServiceBridge platformServiceBridge) {
        mPlatformServiceBridge = platformServiceBridge;
    }

    @Override
    public void accept(byte[] data) {
        mPlatformServiceBridge.logMetrics(data);
    }
}
