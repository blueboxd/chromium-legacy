// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.util.HashSet;
import java.util.List;

/** Observer for activities so that DFMs can be lazily installed on-demand. */
public class ModuleActivityObserver implements ApplicationStatus.ActivityStateListener {
    /** Tracks activities that have been splitcompatted. */
    private static HashSet<Integer> sActivityIds = new HashSet<Integer>();

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.CREATED || newState == ActivityState.RESUMED) {
            splitCompatActivity(activity);
        } else if (newState == ActivityState.DESTROYED) {
            sActivityIds.remove(activity.hashCode());
        }
    }

    /** Makes activities aware of a DFM install and prepare them to be able to use new modules. */
    public void onModuleInstalled() {
        ThreadUtils.assertOnUiThread();

        sActivityIds.clear();

        for (Activity activity : getRunningActivities()) {
            if (getStateForActivity(activity) == ActivityState.RESUMED) {
                splitCompatActivity(activity);
            }
        }
    }

    /** Split Compats activities that have not yet been split compatted. */
    private void splitCompatActivity(Activity activity) {
        Integer key = activity.hashCode();
        if (!sActivityIds.contains(key)) {
            sActivityIds.add(key);
            getModuleInstaller().initActivity(activity);
        }
    }

    @VisibleForTesting
    public ModuleInstaller getModuleInstaller() {
        return ModuleInstallerImpl.getInstance();
    }

    @VisibleForTesting
    public List<Activity> getRunningActivities() {
        return ApplicationStatus.getRunningActivities();
    }

    @VisibleForTesting
    public int getStateForActivity(Activity activity) {
        return ApplicationStatus.getStateForActivity(activity);
    }
}