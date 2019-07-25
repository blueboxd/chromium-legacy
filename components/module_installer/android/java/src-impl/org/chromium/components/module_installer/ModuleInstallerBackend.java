// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.AsyncTask;

import java.util.List;

/** A backend for installing dynamic feature modules that contain the actual install logic. */
@JNINamespace("module_installer")
/* package */ abstract class ModuleInstallerBackend {
    private final OnFinishedListener mListener;

    /** Listener for when a module install has finished. */
    interface OnFinishedListener {
        /**
         * Called when the module install has finished.
         * @param success True if the install was successful.
         * @param moduleNames Names of modules whose install is finished.
         */
        void onFinished(boolean success, List<String> moduleNames);
    }

    public ModuleInstallerBackend(OnFinishedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListener = listener;
    }

    /**
     * Asynchronously installs module.
     * @param moduleName Name of the module.
     */
    public abstract void install(String moduleName);

    /**
     * Asynchronously installs module in the background.
     * @param moduleName Name of the module.
     */
    public abstract void installDeferred(String moduleName);

    /**
     * Releases resources of this backend. Calling this method an install is in progress results in
     * undefined behavior. Calling any other method on this backend after closing results in
     * undefined behavior, too.
     */
    public abstract void close();

    /** To be called when module install has finished. */
    protected void onFinished(boolean success, List<String> moduleNames) {
        ThreadUtils.assertOnUiThread();

        // Call native to perform additional tasks (e.g., load resources). These
        // tasks can be blocking, and are done asynchronously (outside the UI
        // thread). Once done, return to call |mListender.onFinished()| on the
        // UI thread.
        // TODO(crbug.com/987252): Add timing metrics.
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                for (String moduleName : moduleNames) {
                    nativeOnInstallModule(success, moduleName);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                mListener.onFinished(success, moduleNames);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private static native void nativeOnInstallModule(boolean success, String moduleName);
}
