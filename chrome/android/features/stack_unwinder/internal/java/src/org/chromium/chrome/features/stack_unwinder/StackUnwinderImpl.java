// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.stack_unwinder;

import org.chromium.base.annotations.NativeMethods;

/** Stack unwinder implementation. */
public class StackUnwinderImpl implements StackUnwinder {
    private static final String TAG = "StackUnwinderImpl";

    @Override
    public void registerUnwinder() {
        StackUnwinderImplJni.get().registerUnwinder();
    }

    @NativeMethods
    interface Natives {
        void registerUnwinder();
    }
}
