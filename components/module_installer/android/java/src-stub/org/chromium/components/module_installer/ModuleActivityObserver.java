// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;

/** Dummy fallback of ActivityObserver. */
public class ModuleActivityObserver implements ApplicationStatus.ActivityStateListener {
    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        // Do nothing.
    }

    public static void onModuleInstalled() {
        // Do nothing.
    }

    public static void addObserver(Activity activity) {
        // Do nothing.
    }
}