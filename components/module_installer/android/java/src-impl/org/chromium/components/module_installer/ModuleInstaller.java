// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.text.TextUtils;

import com.google.android.play.core.splitcompat.SplitCompat;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;

import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

/** Installs dynamic feature modules (DFMs). */
public class ModuleInstaller {
    /** Command line switch for activating the fake backend.  */
    private static final String FAKE_FEATURE_MODULE_INSTALL = "fake-feature-module-install";
    private static final Map<String, List<OnModuleInstallFinishedListener>> sModuleNameListenerMap =
            new HashMap<>();
    private static ModuleInstallerBackend sBackend;
    private static boolean sSplitCompatted;

    /** Needs to be called before trying to access a module. */
    public static void init() {
        if (sSplitCompatted) return;
        // SplitCompat.install may copy modules into Chrome's internal folder or clean them up.
        try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
            SplitCompat.install(ContextUtils.getApplicationContext());
            sSplitCompatted = true;
        }
        // SplitCompat.install may add emulated modules. Thus, update crash keys.
        updateCrashKeys();
    }

    /**
     * Needs to be called in attachBaseContext of the activities that want to have access to
     * splits prior to application restart.
     *
     * For details, see:
     * https://developer.android.com/reference/com/google/android/play/core/splitcompat/SplitCompat.html#install(android.content.Context)
     */
    public static void initActivity(Context context) {
        SplitCompat.install(context);
    }

    /**
     * Records via UMA all modules that have been requested and are currently installed. The intent
     * is to measure the install penetration of each module.
     */
    public static void recordModuleAvailability() {
        if (!CommandLine.getInstance().hasSwitch(FAKE_FEATURE_MODULE_INSTALL)) {
            PlayCoreModuleInstallerBackend.recordModuleAvailability();
        }
    }

    /** Writes fully installed and emulated modules to crash keys. */
    public static void updateCrashKeys() {
        Context context = ContextUtils.getApplicationContext();

        // Get modules that are fully installed as split APKs (excluding base which is always
        // intalled). Tree set to have ordered and, thus, deterministic results.
        Set<String> fullyInstalledModules = new TreeSet<>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Split APKs are only supported on Android L+.
            try {
                PackageInfo packageInfo = context.getPackageManager().getPackageInfo(
                        BuildInfo.getInstance().packageName, 0);
                if (packageInfo.splitNames != null) {
                    fullyInstalledModules.addAll(Arrays.asList(packageInfo.splitNames));
                }
            } catch (NameNotFoundException e) {
                throw new RuntimeException(e);
            }
        }

        // Create temporary split install manager to retrieve both fully installed and emulated
        // modules. Then remove fully installed ones to get emulated ones only. Querying the
        // installed modules can only be done if splitcompat has already been called. Otherwise,
        // emulation of later modules won't work. If splitcompat has not been called no modules are
        // emulated. Therefore, use an empty set in that case.
        Set<String> emulatedModules = new TreeSet<>();
        if (sSplitCompatted) {
            emulatedModules.addAll(
                    SplitInstallManagerFactory.create(context).getInstalledModules());
            emulatedModules.removeAll(fullyInstalledModules);
        }

        CrashKeys.getInstance().set(
                CrashKeyIndex.INSTALLED_MODULES, encodeCrashKeyValue(fullyInstalledModules));
        CrashKeys.getInstance().set(
                CrashKeyIndex.EMULATED_MODULES, encodeCrashKeyValue(emulatedModules));
    }

    /**
     * Requests the install of a module. The install will be performed asynchronously.
     *
     * @param moduleName Name of the module as defined in GN.
     * @param onFinishedListener Listener to be called once installation is finished.
     */
    public static void install(
            String moduleName, OnModuleInstallFinishedListener onFinishedListener) {
        ThreadUtils.assertOnUiThread();

        if (!sModuleNameListenerMap.containsKey(moduleName)) {
            sModuleNameListenerMap.put(moduleName, new LinkedList<>());
        }
        List<OnModuleInstallFinishedListener> onFinishedListeners =
                sModuleNameListenerMap.get(moduleName);
        onFinishedListeners.add(onFinishedListener);
        if (onFinishedListeners.size() > 1) {
            // Request is already running.
            return;
        }
        getBackend().install(moduleName);
    }

    /**
     * Asynchronously installs module in the background when on unmetered connection and charging.
     * Install is best effort and may fail silently. Upon success, the module will only be available
     * after Chrome restarts.
     *
     * @param moduleName Name of the module.
     */
    public static void installDeferred(String moduleName) {
        ThreadUtils.assertOnUiThread();
        getBackend().installDeferred(moduleName);
    }

    private static void onFinished(boolean success, List<String> moduleNames) {
        ThreadUtils.assertOnUiThread();

        for (String moduleName : moduleNames) {
            List<OnModuleInstallFinishedListener> onFinishedListeners =
                    sModuleNameListenerMap.get(moduleName);
            if (onFinishedListeners == null) continue;

            for (OnModuleInstallFinishedListener listener : onFinishedListeners) {
                listener.onFinished(success);
            }
            sModuleNameListenerMap.remove(moduleName);
        }

        if (sModuleNameListenerMap.isEmpty()) {
            sBackend.close();
            sBackend = null;
        }

        updateCrashKeys();
    }

    private static ModuleInstallerBackend getBackend() {
        if (sBackend == null) {
            ModuleInstallerBackend.OnFinishedListener listener = ModuleInstaller::onFinished;
            sBackend = CommandLine.getInstance().hasSwitch(FAKE_FEATURE_MODULE_INSTALL)
                    ? new FakeModuleInstallerBackend(listener)
                    : new PlayCoreModuleInstallerBackend(listener);
        }
        return sBackend;
    }

    private static String encodeCrashKeyValue(Set<String> moduleNames) {
        if (moduleNames.isEmpty()) return "<none>";
        // Values with dots are interpreted as URLs. Some module names have dots in them. Make sure
        // they don't get sanitized.
        return TextUtils.join(",", moduleNames).replace('.', '$');
    }

    private ModuleInstaller() {}
}
