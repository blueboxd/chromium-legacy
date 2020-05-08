// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.components.module_installer.builder.ModuleInterface;

/**
 * Provides the required Java interface for the dynamic feature module, which is intended to
 * provide access the module functionality. We access the module contents strictly via native code
 * so don't use this interface.
 */
@ModuleInterface(module = "stack_unwinder",
        impl = "org.chromium.chrome.modules.stack_unwinder.StackUnwinderDummyImpl")
public interface StackUnwinderDummyInterface {}
