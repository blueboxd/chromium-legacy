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
import org.chromium.base.annotations.MainDex;
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
@MainDex
public class ModuleInstallerImpl implements ModuleInstaller {
    /** Command line switch for activating the fake backend.  */
    private static final String FAKE_FEATURE_MODULE_INSTALL = "fake-feature-module-install";
    private static ModuleInstaller sInstance = new ModuleInstallerImpl();
    private static boolean sAppContextSplitCompatted;
    private final Map<String, List<OnModuleInstallFinishedListener>> mModuleNameListenerMap =
            new HashMap<>();
    private ModuleInstallerBackend mBackend;

    /** Returns the singleton instance. */
    public static ModuleInstaller getInstance() {
        return sInstance;
    }

    public static void setInstanceForTesting(ModuleInstaller moduleInstaller) {
        sInstance = moduleInstaller;
    }

    @Override
    public void init() {
        if (sAppContextSplitCompatted) return;
        // SplitCompat.install may copy modules into Chrome's internal folder or clean them up.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            SplitCompat.install(ContextUtils.getApplicationContext());
            sAppContextSplitCompatted = true;
        }
        // SplitCompat.install may add emulated modules. Thus, update crash keys.
        updateCrashKeys();
    }

    @Override
    public void initActivity(Context context) {
        SplitCompat.install(context);
    }

    @Override
    public void recordModuleAvailability() {
        if (!CommandLine.getInstance().hasSwitch(FAKE_FEATURE_MODULE_INSTALL)) {
            PlayCoreModuleInstallerBackend.recordModuleAvailability();
        }
    }

    @Override
    public void updateCrashKeys() {
        Context context = ContextUtils.getApplicationContext();

        // Get modules that are fully installed as split APKs (excluding base which is always
        // installed). Tree set to have ordered and, thus, deterministic results.
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
        if (sAppContextSplitCompatted) {
            emulatedModules.addAll(
                    SplitInstallManagerFactory.create(context).getInstalledModules());
            emulatedModules.removeAll(fullyInstalledModules);
        }

        CrashKeys.getInstance().set(
                CrashKeyIndex.INSTALLED_MODULES, encodeCrashKeyValue(fullyInstalledModules));
        CrashKeys.getInstance().set(
                CrashKeyIndex.EMULATED_MODULES, encodeCrashKeyValue(emulatedModules));
    }

    @Override
    public void install(String moduleName, OnModuleInstallFinishedListener onFinishedListener) {
        ThreadUtils.assertOnUiThread();

        if (!mModuleNameListenerMap.containsKey(moduleName)) {
            mModuleNameListenerMap.put(moduleName, new LinkedList<>());
        }
        List<OnModuleInstallFinishedListener> onFinishedListeners =
                mModuleNameListenerMap.get(moduleName);
        onFinishedListeners.add(onFinishedListener);
        if (onFinishedListeners.size() > 1) {
            // Request is already running.
            return;
        }
        getBackend().install(moduleName);
    }

    @Override
    public void installDeferred(String moduleName) {
        ThreadUtils.assertOnUiThread();
        getBackend().installDeferred(moduleName);
    }

    private void onFinished(boolean success, List<String> moduleNames) {
        ThreadUtils.assertOnUiThread();

        for (String moduleName : moduleNames) {
            List<OnModuleInstallFinishedListener> onFinishedListeners =
                    mModuleNameListenerMap.get(moduleName);
            if (onFinishedListeners == null) continue;

            for (OnModuleInstallFinishedListener listener : onFinishedListeners) {
                listener.onFinished(success);
            }
            mModuleNameListenerMap.remove(moduleName);
        }

        if (mModuleNameListenerMap.isEmpty()) {
            mBackend.close();
            mBackend = null;
        }

        updateCrashKeys();
    }

    private ModuleInstallerBackend getBackend() {
        if (mBackend == null) {
            ModuleInstallerBackend.OnFinishedListener listener = this::onFinished;
            mBackend = CommandLine.getInstance().hasSwitch(FAKE_FEATURE_MODULE_INSTALL)
                    ? new FakeModuleInstallerBackend(listener)
                    : new PlayCoreModuleInstallerBackend(listener);
        }
        return mBackend;
    }

    private String encodeCrashKeyValue(Set<String> moduleNames) {
        if (moduleNames.isEmpty()) return "<none>";
        // Values with dots are interpreted as URLs. Some module names have dots in them. Make sure
        // they don't get sanitized.
        return TextUtils.join(",", moduleNames).replace('.', '$');
    }

    private ModuleInstallerImpl() {}
}
