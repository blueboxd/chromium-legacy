// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

/** Listener for when a module install has finished. */
public interface OnModuleInstallFinishedListener {
    /**
     * Called when the install has finished.
     *
     * @param success True if the module was installed successfully.
     */
    void onFinished(boolean success);
}
