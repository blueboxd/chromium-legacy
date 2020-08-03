// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A utility class for querying information about the default browser setting.
 */
public final class DefaultBrowserInfo {
    /**
     * A list of potential default browser states.  To add a type to this list please update
     * MobileDefaultBrowserState in histograms.xml and make sure to keep this list in sync.
     * Additions should be treated as APPEND ONLY to keep the UMA metric semantics the same over
     * time.
     */
    @IntDef({MobileDefaultBrowserState.NO_DEFAULT, MobileDefaultBrowserState.CHROME_SYSTEM_DEFAULT,
            MobileDefaultBrowserState.CHROME_INSTALLED_DEFAULT,
            MobileDefaultBrowserState.OTHER_SYSTEM_DEFAULT,
            MobileDefaultBrowserState.OTHER_INSTALLED_DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MobileDefaultBrowserState {
        int NO_DEFAULT = 0;
        int CHROME_SYSTEM_DEFAULT = 1;
        int CHROME_INSTALLED_DEFAULT = 2;
        int OTHER_SYSTEM_DEFAULT = 3;
        int OTHER_INSTALLED_DEFAULT = 4;
        int NUM_ENTRIES = 5;
    }

    /** Contains all status related to the default browser state on the device. */
    public static class DefaultInfo {
        /** Whether or not Chrome is the system browser. */
        public final boolean isChromeSystem;

        /** Whether or not Chrome is the default browser. */
        public final boolean isChromeDefault;

        /** Whether or not the default browser is the system browser. */
        public final boolean isDefaultSystem;

        /** Whether or not the user has set a default browser. */
        public final boolean hasDefault;

        /** The number of browsers installed on this device. */
        public final int browserCount;

        /** The number of system browsers installed on this device. */
        public final int systemCount;

        /** Creates an instance of the {@link DefaultInfo} class. */
        public DefaultInfo(boolean isChromeSystem, boolean isChromeDefault, boolean isDefaultSystem,
                boolean hasDefault, int browserCount, int systemCount) {
            this.isChromeSystem = isChromeSystem;
            this.isChromeDefault = isChromeDefault;
            this.isDefaultSystem = isDefaultSystem;
            this.hasDefault = hasDefault;
            this.browserCount = browserCount;
            this.systemCount = systemCount;
        }
    }

    private static DefaultInfoTask sDefaultInfoTask;

    /** A lock to synchronize background tasks to retrieve browser information. */
    private static final Object sDirCreationLock = new Object();

    private static AsyncTask<ArrayList<String>> sDefaultBrowserFetcher;

    /** Don't instantiate me. */
    private DefaultBrowserInfo() {}

    /**
     * Initialize an AsyncTask for getting menu title of opening a link in default browser.
     */
    public static void initBrowserFetcher() {
        // TODO(crbug.com/1107527): Make this depend on getDefaultBrowserInfo instead of rolling
        //  it's own AsyncTask.  Potentially update DefaultInfo to include extra data if necessary.
        synchronized (sDirCreationLock) {
            if (sDefaultBrowserFetcher == null) {
                sDefaultBrowserFetcher = new BackgroundOnlyAsyncTask<ArrayList<String>>() {
                    @Override
                    protected ArrayList<String> doInBackground() {
                        Context context = ContextUtils.getApplicationContext();
                        ArrayList<String> menuTitles = new ArrayList<String>(2);
                        // Store the package label of current application.
                        menuTitles.add(getTitleFromPackageLabel(
                                context, BuildInfo.getInstance().hostPackageLabel));

                        PackageManager pm = context.getPackageManager();
                        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();

                        // Caches whether Chrome is set as a default browser on the device.
                        boolean isDefault = info != null && info.match != 0
                                && TextUtils.equals(
                                           context.getPackageName(), info.activityInfo.packageName);
                        SharedPreferencesManager.getInstance().writeBoolean(
                                ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, isDefault);

                        // Check if there is a default handler for the Intent.  If so, store its
                        // label.
                        String packageLabel = null;
                        if (info != null && info.match != 0 && info.loadLabel(pm) != null) {
                            packageLabel = info.loadLabel(pm).toString();
                        }
                        menuTitles.add(getTitleFromPackageLabel(context, packageLabel));
                        return menuTitles;
                    }
                };
                sDefaultBrowserFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        }
    }

    private static String getTitleFromPackageLabel(Context context, String packageLabel) {
        return packageLabel == null
                ? context.getString(R.string.menu_open_in_product_default)
                : context.getString(R.string.menu_open_in_product, packageLabel);
    }

    /**
     * @return Title of the menu item for opening a link in the default browser.
     * @param forceChromeAsDefault Whether the Custom Tab is created by Chrome.
     */
    public static String getTitleOpenInDefaultBrowser(final boolean forceChromeAsDefault) {
        if (sDefaultBrowserFetcher == null) {
            initBrowserFetcher();
        }
        try {
            // If the Custom Tab was created by Chrome, Chrome should handle the action for the
            // overflow menu.
            return forceChromeAsDefault ? sDefaultBrowserFetcher.get().get(0)
                                        : sDefaultBrowserFetcher.get().get(1);
        } catch (InterruptedException | ExecutionException e) {
            return ContextUtils.getApplicationContext().getString(
                    R.string.menu_open_in_product_default);
        }
    }

    /**
     * Determines various information about browsers on the system.
     * @param callback To be called with a {@link DefaultInfo} instance if possible.  Can be {@code
     *         null}.
     * @see DefaultInfo
     */
    public static void getDefaultBrowserInfo(Callback<DefaultInfo> callback) {
        ThreadUtils.checkUiThread();
        if (sDefaultInfoTask == null) sDefaultInfoTask = new DefaultInfoTask();
        sDefaultInfoTask.get(callback);
    }

    /**
     * Log statistics about the current default browser to UMA.
     */
    public static void logDefaultBrowserStats() {
        getDefaultBrowserInfo(info -> {
            if (info == null) return;

            RecordHistogram.recordCount100Histogram(
                    getSystemBrowserCountUmaName(info), info.systemCount);
            RecordHistogram.recordCount100Histogram(
                    getDefaultBrowserCountUmaName(info), info.browserCount);
            RecordHistogram.recordEnumeratedHistogram("Mobile.DefaultBrowser.State",
                    getDefaultBrowserUmaState(info), MobileDefaultBrowserState.NUM_ENTRIES);
        });
    }

    @VisibleForTesting
    public static void setDefaultInfoForTests(DefaultInfo info) {
        DefaultInfoTask.setDefaultInfoForTests(info);
    }

    @VisibleForTesting
    public static void clearDefaultInfoForTests() {
        DefaultInfoTask.clearDefaultInfoForTests();
    }

    private static class DefaultInfoTask extends AsyncTask<DefaultInfo> {
        private static AtomicReference<DefaultInfo> sTestInfo;

        private final ObserverList<Callback<DefaultInfo>> mObservers = new ObserverList<>();

        @VisibleForTesting
        public static void setDefaultInfoForTests(DefaultInfo info) {
            sTestInfo = new AtomicReference<DefaultInfo>(info);
        }

        public static void clearDefaultInfoForTests() {
            sTestInfo = null;
        }

        /**
         *  Queues up {@code callback} to be notified of the result of this {@link AsyncTask}.  If
         *  the task has not been started, this will start it.  If the task is finished, this will
         *  send the result.  If the task is running this will queue the callback up until the task
         *  is done.
         *
         * @param callback The {@link Callback} to notify with the right {@link DefaultInfo}.
         */
        public void get(Callback<DefaultInfo> callback) {
            ThreadUtils.checkUiThread();

            if (getStatus() == Status.FINISHED) {
                DefaultInfo info = null;
                try {
                    info = sTestInfo == null ? get() : sTestInfo.get();
                } catch (InterruptedException | ExecutionException e) {
                    // Fail silently here since this is not a critical task.
                }

                final DefaultInfo postInfo = info;
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(postInfo));
            } else {
                if (getStatus() == Status.PENDING) {
                    try {
                        executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    } catch (RejectedExecutionException e) {
                        // Fail silently here since this is not a critical task.
                        PostTask.postTask(
                                UiThreadTaskTraits.DEFAULT, () -> callback.onResult(null));
                        return;
                    }
                }
                mObservers.addObserver(callback);
            }
        }

        @Override
        protected DefaultInfo doInBackground() {
            Context context = ContextUtils.getApplicationContext();

            boolean isChromeSystem = false;
            boolean isChromeDefault = false;
            boolean isDefaultSystem = false;
            boolean hasDefault = false;
            int browserCount = 0;
            int systemCount = 0;

            // Query the default handler first.
            ResolveInfo defaultRi = PackageManagerUtils.resolveDefaultWebBrowserActivity();
            if (defaultRi != null && defaultRi.match != 0) {
                hasDefault = true;
                isChromeDefault = isSamePackage(context, defaultRi);
                isDefaultSystem = isSystemPackage(defaultRi);
            }

            // Query all other intent handlers.
            Set<String> uniquePackages = new HashSet<>();
            List<ResolveInfo> ris = PackageManagerUtils.queryAllWebBrowsersInfo();
            if (ris != null) {
                for (ResolveInfo ri : ris) {
                    String packageName = ri.activityInfo.applicationInfo.packageName;
                    if (!uniquePackages.add(packageName)) continue;

                    if (isSystemPackage(ri)) {
                        if (isSamePackage(context, ri)) isChromeSystem = true;
                        systemCount++;
                    }
                }
            }

            browserCount = uniquePackages.size();

            return new DefaultInfo(isChromeSystem, isChromeDefault, isDefaultSystem, hasDefault,
                    browserCount, systemCount);
        }

        @Override
        protected void onPostExecute(DefaultInfo defaultInfo) {
            flushCallbacks(sTestInfo == null ? defaultInfo : sTestInfo.get());
        }

        @Override
        protected void onCancelled() {
            flushCallbacks(null);
        }

        private void flushCallbacks(DefaultInfo info) {
            for (Callback<DefaultInfo> callback : mObservers) callback.onResult(info);
            mObservers.clear();
        }
    }

    private static String getSystemBrowserCountUmaName(DefaultInfo info) {
        if (info.isChromeSystem) return "Mobile.DefaultBrowser.SystemBrowserCount.ChromeSystem";
        return "Mobile.DefaultBrowser.SystemBrowserCount.ChromeNotSystem";
    }

    private static String getDefaultBrowserCountUmaName(DefaultInfo info) {
        if (!info.hasDefault) return "Mobile.DefaultBrowser.BrowserCount.NoDefault";
        if (info.isChromeDefault) return "Mobile.DefaultBrowser.BrowserCount.ChromeDefault";
        return "Mobile.DefaultBrowser.BrowserCount.OtherDefault";
    }

    private static @MobileDefaultBrowserState int getDefaultBrowserUmaState(DefaultInfo info) {
        if (!info.hasDefault) return MobileDefaultBrowserState.NO_DEFAULT;

        if (info.isChromeDefault) {
            if (info.isDefaultSystem) return MobileDefaultBrowserState.CHROME_SYSTEM_DEFAULT;
            return MobileDefaultBrowserState.CHROME_INSTALLED_DEFAULT;
        }

        if (info.isDefaultSystem) return MobileDefaultBrowserState.OTHER_SYSTEM_DEFAULT;
        return MobileDefaultBrowserState.OTHER_INSTALLED_DEFAULT;
    }

    private static boolean isSamePackage(Context context, ResolveInfo info) {
        return TextUtils.equals(
                context.getPackageName(), info.activityInfo.applicationInfo.packageName);
    }

    private static boolean isSystemPackage(ResolveInfo info) {
        return (info.activityInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }
}
