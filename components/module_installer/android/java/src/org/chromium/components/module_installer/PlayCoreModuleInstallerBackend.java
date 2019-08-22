// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.content.SharedPreferences;
import android.util.SparseLongArray;

import com.google.android.play.core.splitinstall.SplitInstallException;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallSessionState;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.RecordHistogram;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Backend that uses the Play Core SDK to download a module from Play and install it subsequently.
 */
/* package */ class PlayCoreModuleInstallerBackend
        extends ModuleInstallerBackend implements SplitInstallStateUpdatedListener {
    private static class InstallTimes {
        public final boolean mIsCached;
        public final SparseLongArray mInstallTimes = new SparseLongArray();

        public InstallTimes(boolean isCached) {
            mIsCached = isCached;
            mInstallTimes.put(SplitInstallSessionStatus.UNKNOWN, System.currentTimeMillis());
        }
    }

    private static final String TAG = "PlayCoreModInBackend";
    private static final String KEY_MODULES_ONDEMAND_REQUESTED_PREVIOUSLY =
            "key_modules_requested_previously";
    private static final String KEY_MODULES_DEFERRED_REQUESTED_PREVIOUSLY =
            "key_modules_deferred_requested_previously";
    private final Map<String, InstallTimes> mInstallTimesMap = new HashMap<>();
    private final SplitInstallManager mManager;
    private boolean mIsClosed;

    // FeatureModuleInstallStatus defined in //tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    private static final int INSTALL_STATUS_SUCCESS = 0;
    // private static final int INSTALL_STATUS_FAILURE = 1; [DEPRECATED]
    // private static final int INSTALL_STATUS_REQUEST_ERROR = 2; [DEPRECATED]
    private static final int INSTALL_STATUS_CANCELLATION = 3;
    private static final int INSTALL_STATUS_ACCESS_DENIED = 4;
    private static final int INSTALL_STATUS_ACTIVE_SESSIONS_LIMIT_EXCEEDED = 5;
    private static final int INSTALL_STATUS_API_NOT_AVAILABLE = 6;
    private static final int INSTALL_STATUS_INCOMPATIBLE_WITH_EXISTING_SESSION = 7;
    private static final int INSTALL_STATUS_INSUFFICIENT_STORAGE = 8;
    private static final int INSTALL_STATUS_INVALID_REQUEST = 9;
    private static final int INSTALL_STATUS_MODULE_UNAVAILABLE = 10;
    private static final int INSTALL_STATUS_NETWORK_ERROR = 11;
    private static final int INSTALL_STATUS_NO_ERROR = 12;
    private static final int INSTALL_STATUS_SERVICE_DIED = 13;
    private static final int INSTALL_STATUS_SESSION_NOT_FOUND = 14;
    private static final int INSTALL_STATUS_SPLITCOMPAT_COPY_ERROR = 15;
    private static final int INSTALL_STATUS_SPLITCOMPAT_EMULATION_ERROR = 16;
    private static final int INSTALL_STATUS_SPLITCOMPAT_VERIFICATION_ERROR = 17;
    private static final int INSTALL_STATUS_INTERNAL_ERROR = 18;
    private static final int INSTALL_STATUS_UNKNOWN_SPLITINSTALL_ERROR = 19;
    private static final int INSTALL_STATUS_UNKNOWN_REQUEST_ERROR = 20;
    private static final int INSTALL_STATUS_NO_SPLITCOMPAT = 21;
    // Keep this one at the end and increment appropriately when adding new status.
    private static final int INSTALL_STATUS_COUNT = 22;

    // FeatureModuleAvailabilityStatus defined in //tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    private static final int AVAILABILITY_STATUS_REQUESTED = 0;
    private static final int AVAILABILITY_STATUS_INSTALLED_REQUESTED = 1;
    private static final int AVAILABILITY_STATUS_INSTALLED_UNREQUESTED = 2;
    // Keep this one at the end and increment appropriately when adding new status.
    private static final int AVAILABILITY_STATUS_COUNT = 3;

    /** Records via UMA all modules that have been requested and are currently installed. */
    public static void recordModuleAvailability() {
        // MUST call init before creating a SplitInstallManager.
        ModuleInstaller.getInstance().init();
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> requestedModules = new HashSet<>();
        requestedModules.addAll(
                prefs.getStringSet(KEY_MODULES_ONDEMAND_REQUESTED_PREVIOUSLY, new HashSet<>()));
        requestedModules.addAll(
                prefs.getStringSet(KEY_MODULES_DEFERRED_REQUESTED_PREVIOUSLY, new HashSet<>()));
        Set<String> installedModules =
                SplitInstallManagerFactory.create(ContextUtils.getApplicationContext())
                        .getInstalledModules();

        for (String name : requestedModules) {
            EnumeratedHistogramSample sample = new EnumeratedHistogramSample(
                    "Android.FeatureModules.AvailabilityStatus." + name, AVAILABILITY_STATUS_COUNT);
            if (installedModules.contains(name)) {
                sample.record(AVAILABILITY_STATUS_INSTALLED_REQUESTED);
            } else {
                sample.record(AVAILABILITY_STATUS_REQUESTED);
            }
        }

        for (String name : installedModules) {
            if (!requestedModules.contains(name)) {
                // Module appeared without being requested. Weird.
                EnumeratedHistogramSample sample = new EnumeratedHistogramSample(
                        "Android.FeatureModules.AvailabilityStatus." + name,
                        AVAILABILITY_STATUS_COUNT);
                sample.record(AVAILABILITY_STATUS_INSTALLED_UNREQUESTED);
            }
        }
    }

    public PlayCoreModuleInstallerBackend(OnFinishedListener listener) {
        super(listener);
        // MUST call init before creating a SplitInstallManager.
        ModuleInstaller.getInstance().init();
        mManager = SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());
        mManager.registerListener(this);
    }

    @Override
    public void install(String moduleName) {
        assert !mIsClosed;

        // Record start time in order to later report the install duration via UMA. We want to make
        // a difference between modules that have been requested first before and after the last
        // Chrome start. Modules that have been requested before may install quicker as they may be
        // installed form cache. To do this, we use shared prefs to track modules previously
        // requested. Additionally, storing requested modules helps us to record module install
        // status at next Chrome start.
        assert !mInstallTimesMap.containsKey(moduleName);
        mInstallTimesMap.put(moduleName,
                new InstallTimes(storeModuleRequested(
                        moduleName, KEY_MODULES_ONDEMAND_REQUESTED_PREVIOUSLY)));

        SplitInstallRequest request =
                SplitInstallRequest.newBuilder().addModule(moduleName).build();

        mManager.startInstall(request).addOnFailureListener(exception -> {
            int status = exception instanceof SplitInstallException
                    ? getHistogramCode(((SplitInstallException) exception).getErrorCode())
                    : INSTALL_STATUS_UNKNOWN_REQUEST_ERROR;
            Log.e(TAG, "Failed to request module '%s': error code %s", moduleName, status);
            // If we reach this error condition |onStateUpdate| won't be called. Thus, call
            // |onFinished| here.
            finish(false, Collections.singletonList(moduleName), status);
        });
    }

    @Override
    public void installDeferred(String moduleName) {
        assert !mIsClosed;
        mManager.deferredInstall(Collections.singletonList(moduleName));
        storeModuleRequested(moduleName, KEY_MODULES_DEFERRED_REQUESTED_PREVIOUSLY);
    }

    @Override
    public void close() {
        assert !mIsClosed;
        mManager.unregisterListener(this);
        mIsClosed = true;
    }

    @Override
    public void onStateUpdate(SplitInstallSessionState state) {
        assert !mIsClosed;
        Log.i(TAG, "Status for modules '%s' updated to %d", state.moduleNames(), state.status());
        switch (state.status()) {
            case SplitInstallSessionStatus.DOWNLOADING:
            case SplitInstallSessionStatus.INSTALLING:
            case SplitInstallSessionStatus.INSTALLED:
                for (String name : state.moduleNames()) {
                    mInstallTimesMap.get(name).mInstallTimes.put(
                            state.status(), System.currentTimeMillis());
                }
                if (state.status() == SplitInstallSessionStatus.INSTALLED) {
                    finish(true, state.moduleNames(), INSTALL_STATUS_SUCCESS);
                }
                break;
            // DOWNLOADED only gets sent if SplitCompat is not enabled. That's an error.
            // SplitCompat should always be enabled.
            case SplitInstallSessionStatus.DOWNLOADED:
            case SplitInstallSessionStatus.CANCELED:
            case SplitInstallSessionStatus.FAILED:
                int status;
                if (state.status() == SplitInstallSessionStatus.DOWNLOADED) {
                    status = INSTALL_STATUS_NO_SPLITCOMPAT;
                } else if (state.status() == SplitInstallSessionStatus.CANCELED) {
                    status = INSTALL_STATUS_CANCELLATION;
                } else {
                    status = getHistogramCode(state.errorCode());
                }
                Log.e(TAG, "Failed to install modules '%s': error code %s", state.moduleNames(),
                        status);
                finish(false, state.moduleNames(), status);
                break;
        }
    }

    private void finish(boolean success, List<String> moduleNames, int eventId) {
        for (String name : moduleNames) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.FeatureModules.InstallStatus." + name, eventId, INSTALL_STATUS_COUNT);
            if (success) {
                recordInstallTimes(name);
            }
        }
        onFinished(success, moduleNames);
    }

    /**
     * Gets the UMA code based on a SplitInstall error code
     * @param errorCode The error code
     * @return int The User Metric Analysis code
     */
    private int getHistogramCode(@SplitInstallErrorCode int errorCode) {
        switch (errorCode) {
            case SplitInstallErrorCode.ACCESS_DENIED:
                return INSTALL_STATUS_ACCESS_DENIED;
            case SplitInstallErrorCode.ACTIVE_SESSIONS_LIMIT_EXCEEDED:
                return INSTALL_STATUS_ACTIVE_SESSIONS_LIMIT_EXCEEDED;
            case SplitInstallErrorCode.API_NOT_AVAILABLE:
                return INSTALL_STATUS_API_NOT_AVAILABLE;
            case SplitInstallErrorCode.INCOMPATIBLE_WITH_EXISTING_SESSION:
                return INSTALL_STATUS_INCOMPATIBLE_WITH_EXISTING_SESSION;
            case SplitInstallErrorCode.INSUFFICIENT_STORAGE:
                return INSTALL_STATUS_INSUFFICIENT_STORAGE;
            case SplitInstallErrorCode.INVALID_REQUEST:
                return INSTALL_STATUS_INVALID_REQUEST;
            case SplitInstallErrorCode.MODULE_UNAVAILABLE:
                return INSTALL_STATUS_MODULE_UNAVAILABLE;
            case SplitInstallErrorCode.NETWORK_ERROR:
                return INSTALL_STATUS_NETWORK_ERROR;
            case SplitInstallErrorCode.NO_ERROR:
                return INSTALL_STATUS_NO_ERROR;
            case SplitInstallErrorCode.SERVICE_DIED:
                return INSTALL_STATUS_SERVICE_DIED;
            case SplitInstallErrorCode.SESSION_NOT_FOUND:
                return INSTALL_STATUS_SESSION_NOT_FOUND;
            case SplitInstallErrorCode.SPLITCOMPAT_COPY_ERROR:
                return INSTALL_STATUS_SPLITCOMPAT_COPY_ERROR;
            case SplitInstallErrorCode.SPLITCOMPAT_EMULATION_ERROR:
                return INSTALL_STATUS_SPLITCOMPAT_EMULATION_ERROR;
            case SplitInstallErrorCode.SPLITCOMPAT_VERIFICATION_ERROR:
                return INSTALL_STATUS_SPLITCOMPAT_VERIFICATION_ERROR;
            case SplitInstallErrorCode.INTERNAL_ERROR:
                return INSTALL_STATUS_INTERNAL_ERROR;
            default:
                return INSTALL_STATUS_UNKNOWN_SPLITINSTALL_ERROR;
        }
    }

    /**
     * Stores to shared prevs that a module has been requested.
     *
     * @param moduleName Module that has been requested.
     * @param prefKey Pref key pointing to a string set to which the requested module will be added.
     * @return Whether the module has been requested previously.
     */
    private static boolean storeModuleRequested(String moduleName, String prefKey) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> modulesRequestedPreviously = prefs.getStringSet(prefKey, new HashSet<String>());
        Set<String> newModulesRequestedPreviously = new HashSet<>(modulesRequestedPreviously);
        newModulesRequestedPreviously.add(moduleName);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(prefKey, newModulesRequestedPreviously);
        editor.apply();
        return modulesRequestedPreviously.contains(moduleName);
    }

    /** Records via UMA module install times divided into install steps. */
    private void recordInstallTimes(String moduleName) {
        recordInstallTime(moduleName, "", SplitInstallSessionStatus.UNKNOWN,
                SplitInstallSessionStatus.INSTALLED);
        recordInstallTime(moduleName, ".PendingDownload", SplitInstallSessionStatus.UNKNOWN,
                SplitInstallSessionStatus.DOWNLOADING);
        recordInstallTime(moduleName, ".Download", SplitInstallSessionStatus.DOWNLOADING,
                SplitInstallSessionStatus.INSTALLING);
        recordInstallTime(moduleName, ".Installing", SplitInstallSessionStatus.INSTALLING,
                SplitInstallSessionStatus.INSTALLED);
    }

    private void recordInstallTime(
            String moduleName, String histogramSubname, int startKey, int endKey) {
        assert mInstallTimesMap.containsKey(moduleName);
        InstallTimes installTimes = mInstallTimesMap.get(moduleName);
        if (installTimes.mInstallTimes.get(startKey) == 0
                || installTimes.mInstallTimes.get(endKey) == 0) {
            // Time stamps for install times have not been stored. Don't record anything to not skew
            // data.
            return;
        }
        RecordHistogram.recordLongTimesHistogram(
                String.format("Android.FeatureModules.%sInstallDuration%s.%s",
                        installTimes.mIsCached ? "Cached" : "Uncached", histogramSubname,
                        moduleName),
                installTimes.mInstallTimes.get(endKey) - installTimes.mInstallTimes.get(startKey));
    }
}
