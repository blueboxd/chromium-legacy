// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observers;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;

import java.util.HashSet;

/**
 *  Observer for activities so that DFMs can be lazily installed on-demand.
 *  Note that ActivityIds are managed globally and therefore any changes to it are to be made
 *  using a single thread (in this case, the UI thread).
 */
public class ModuleActivityObserver implements ApplicationStatus.ActivityStateListener {
    private static HashSet<Integer> sActivityIds = new HashSet<Integer>();
    private final ObserverStrategy mStrategy;

    public ModuleActivityObserver() {
        this(new ObserverStrategyImpl());
    }

    public ModuleActivityObserver(ObserverStrategy strategy) {
        mStrategy = strategy;
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        ThreadUtils.assertOnUiThread();

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

        for (Activity activity : mStrategy.getRunningActivities()) {
            if (mStrategy.getStateForActivity(activity) == ActivityState.RESUMED) {
                splitCompatActivity(activity);
            }
        }
    }

    /** Split Compats activities that have not yet been split compatted. */
    private void splitCompatActivity(Activity activity) {
        Integer key = activity.hashCode();
        if (!sActivityIds.contains(key)) {
            sActivityIds.add(key);
            mStrategy.getModuleInstaller().initActivity(activity);
        }
    }
}