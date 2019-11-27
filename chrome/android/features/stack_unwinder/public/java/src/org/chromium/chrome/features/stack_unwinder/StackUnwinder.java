// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.stack_unwinder;

/** Provides a mechanism to register the native unwinder. */
public interface StackUnwinder {
    /**
     * Registers the native unwinder with the StackSamplingProfiler.
     */
    void registerUnwinder();
}
