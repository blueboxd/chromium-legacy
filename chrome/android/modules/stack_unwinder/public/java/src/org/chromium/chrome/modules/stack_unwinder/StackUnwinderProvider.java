// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.chrome.features.stack_unwinder.StackUnwinder;
import org.chromium.components.module_installer.builder.ModuleInterface;

/** Provides the stack unwinder implementation. */
@ModuleInterface(module = "stack_unwinder",
        impl = "org.chromium.chrome.modules.stack_unwinder.StackUnwinderProviderImpl")
public interface StackUnwinderProvider {
    StackUnwinder getStackUnwinder();
}
