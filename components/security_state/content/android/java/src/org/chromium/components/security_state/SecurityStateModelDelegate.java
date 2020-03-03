// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.security_state;

import org.chromium.base.annotations.CalledByNative;

/**
 * Dumb wrapper around the pointer to the C++ class SecurityStateModelDelegate.
 */
public class SecurityStateModelDelegate {
    private long mNativePtr;

    public SecurityStateModelDelegate(long nativePtr) {
        this.mNativePtr = nativePtr;
    }

    @CalledByNative
    protected long getNativePtr() {
        return mNativePtr;
    }
}
