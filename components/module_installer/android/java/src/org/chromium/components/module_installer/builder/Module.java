// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;
import org.chromium.components.module_installer.util.Timer;

import java.util.HashSet;
import java.util.Set;

/**
 * Represents a feature module. Can be used to install the module, access its interface, etc. See
 * {@link ModuleInterface} for how to conveniently create an instance of the module class for a
 * specific feature module.
 *
 * @param <T> The interface of the module
 */
@JNINamespace("module_installer")
public class Module<T> {
    private final String mName;
    private final Class<T> mInterfaceClass;
    private final String mImplClassName;

    private T mImpl;

    private InstallEngine mInstaller;

    private static boolean sNativeInitialized;
    private static final Set<String> sPendingNativeRegistrations = new HashSet<>();

    /**
     * Instantiates a module.
     *
     * @param name The module's name as used with {@link ModuleInstaller}.
     * @param interfaceClass {@link Class} object of the module interface.
     * @param implClassName fully qualified class name of the implementation of the module's
     *                      interface.
     */
    public Module(String name, Class<T> interfaceClass, String implClassName) {
        mName = name;
        mInterfaceClass = interfaceClass;
        mImplClassName = implClassName;
    }

    @VisibleForTesting
    public InstallEngine getInstallEngine() {
        if (mInstaller == null) {
            try (Timer timer = new Timer()) {
                mInstaller = new ModuleEngine(mImplClassName);
            }
        }
        return mInstaller;
    }

    @VisibleForTesting
    public void setInstallEngine(InstallEngine engine) {
        mInstaller = engine;
    }

    /**
     * Returns true if the module is currently installed and can be accessed.
     */
    public boolean isInstalled() {
        try (Timer timer = new Timer()) {
            return getInstallEngine().isInstalled(mName);
        }
    }

    /**
     * Requests install of the module.
     */
    public void install(InstallListener listener) {
        try (Timer timer = new Timer()) {
            assert !isInstalled();
            getInstallEngine().install(mName, listener);
        }
    }

    /**
     * Requests deferred install of the module.
     */
    public void installDeferred() {
        try (Timer timer = new Timer()) {
            getInstallEngine().installDeferred(mName);
        }
    }

    /**
     * Returns the implementation of the module interface. Must only be called if the module is
     * installed.
     */
    public T getImpl() {
        try (Timer timer = new Timer()) {
            assert isInstalled();
            if (mImpl == null) {
                // Accessing classes in the module may cause its DEX file to be loaded. And on some
                // devices that causes a read mode violation.
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    mImpl = mInterfaceClass.cast(Class.forName(mImplClassName).newInstance());
                } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                        | IllegalArgumentException e) {
                    throw new RuntimeException(e);
                }

                // Load the module's native library if there's one present, and the Chrome native
                // library itself has been loaded.
                if (sNativeInitialized) {
                    loadNativeLibrary(mName);
                } else {
                    sPendingNativeRegistrations.add(mName);
                }
            }
            return mImpl;
        }
    }

    private static void loadNativeLibrary(String name) {
        // TODO(https://crbug.com/870055): Whitelist modules, until each module explicitly indicates
        // its need for library loading through this system.
        if (!"test_dummy".equals(name)) return;

        ModuleJni.get().loadNativeLibrary(name);
    }

    /**
     * To be called after the main native library has been loaded. Any module instances
     * created before the native library is loaded have their native component queued
     * for loading and registration. Calling this methed completes that process.
     **/
    public static void doDeferredNativeRegistrations() {
        for (String name : sPendingNativeRegistrations) {
            loadNativeLibrary(name);
        }
        sPendingNativeRegistrations.clear();
        sNativeInitialized = true;
    }

    @NativeMethods
    interface Natives {
        void loadNativeLibrary(String name);
    }
}
