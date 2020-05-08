// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.base.annotations.UsedByReflection;

/**
 * Provides the required Java implementation for the dynamic feature module, which is intended to
 * implement the module functionality. This class is empty since we access the module contents
 * strictly via native code.
 */
@UsedByReflection("StackUnwinderModule")
public class StackUnwinderDummyImpl implements StackUnwinderDummyInterface {}
