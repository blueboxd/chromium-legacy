// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.DiscardableReferencePool;

/**
 * A global accessor to the DiscardableReferencePool.
 *
 * The application should instantiate this class and keep it alive until shutdown.
 * Then consumers may find the DiscardableReferencePool through the static API.
 */
public class GlobalDiscardableReferencePool {
    private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();
    private static GlobalDiscardableReferencePool sInstance;

    public static DiscardableReferencePool getReferencePool() {
        return sInstance.mReferencePool;
    }

    public GlobalDiscardableReferencePool() {
        assert sInstance == null;
        sInstance = this;
    }
}
