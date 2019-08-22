// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observers;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.components.module_installer.ModuleInstaller;

import java.util.List;

/** Strategy utilizing ModuleInstaller and ApplicationStatus. */
/* package */ class ObserverStrategyImpl implements ObserverStrategy {
    @Override
    public ModuleInstaller getModuleInstaller() {
        return ModuleInstaller.getInstance();
    }

    @Override
    public List<Activity> getRunningActivities() {
        return ApplicationStatus.getRunningActivities();
    }

    @Override
    public int getStateForActivity(Activity activity) {
        return ApplicationStatus.getStateForActivity(activity);
    }
}