// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.features.stack_unwinder.StackUnwinder;
import org.chromium.chrome.features.stack_unwinder.StackUnwinderImpl;

/** Provides the stack unwinder implementation inside the stack unwinder module. */
@UsedByReflection("StackUnwinderModule")
public class StackUnwinderProviderImpl implements StackUnwinderProvider {
    private final StackUnwinderImpl mStackUnwinder = new StackUnwinderImpl();

    @Override
    public StackUnwinder getStackUnwinder() {
        return mStackUnwinder;
    }
}
