// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.List;

/**
 * Exposes information about the current accessibility state
 */
public class AccessibilityUtil {
    /**
     * An observer to be notified of accessibility status changes.
     */
    public interface Observer {
        /**
         * @param enabled Whether a touch exploration or an accessibility service that performs can
         *        perform gestures is enabled. Indicates that the UI must be fully navigable using
         *        the accessibility view tree.
         */
        void onAccessibilityModeChanged(boolean enabled);
    }

    private static Boolean sIsAccessibilityEnabled;
    private static ActivityStateListener sActivityStateListener;
    private static ObserverList<Observer> sObservers;
    private static class ModeChangeHandler
            implements AccessibilityStateChangeListener, TouchExplorationStateChangeListener {
        // AccessibilityStateChangeListener

        @Override
        public final void onAccessibilityStateChanged(boolean enabled) {
            resetAccessibilityEnabled();
            notifyModeChange();
        }

        // TouchExplorationStateChangeListener

        @Override
        public void onTouchExplorationStateChanged(boolean enabled) {
            resetAccessibilityEnabled();
            notifyModeChange();
        }
    }

    private static ModeChangeHandler sModeChangeHandler;

    private AccessibilityUtil() {}

    /**
     * Checks to see that this device has accessibility and touch exploration enabled.
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    public static boolean isAccessibilityEnabled() {
        if (sModeChangeHandler == null) registerModeChangeListeners();
        if (sIsAccessibilityEnabled != null) return sIsAccessibilityEnabled;

        TraceEvent.begin("AccessibilityManager::isAccessibilityEnabled");

        AccessibilityManager manager = getAccessibilityManager();
        boolean accessibilityEnabled =
                manager != null && manager.isEnabled() && manager.isTouchExplorationEnabled();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && manager != null
                && manager.isEnabled() && !accessibilityEnabled) {
            List<AccessibilityServiceInfo> services = manager.getEnabledAccessibilityServiceList(
                    AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
            for (AccessibilityServiceInfo service : services) {
                if (canPerformGestures(service)) {
                    accessibilityEnabled = true;
                    break;
                }
            }
        }

        sIsAccessibilityEnabled = accessibilityEnabled;

        sActivityStateListener = new ApplicationStatus.ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                // If an activity is being resumed, it's possible the user changed accessibility
                // settings while not in a Chrome activity. Reset the accessibility enabled state
                // so that the next time #isAccessibilityEnabled is called the accessibility state
                // is recalculated. Also, if all activities are destroyed, remove the activity
                // state listener to avoid leaks.
                if (newState == ActivityState.RESUMED
                        || ApplicationStatus.isEveryActivityDestroyed()) {
                    resetAccessibilityEnabled();
                }
                if (ApplicationStatus.isEveryActivityDestroyed()) cleanUp();
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(sActivityStateListener);

        TraceEvent.end("AccessibilityManager::isAccessibilityEnabled");
        return sIsAccessibilityEnabled;
    }

    /**
     * Add {@link Observer} object. The observer will be notified of the current accessibility
     * mode immediately.
     * @param observer Observer object monitoring a11y mode change.
     */
    public static void addObserver(Observer observer) {
        getObservers().addObserver(observer);

        // Notify mode change to a new observer so things are initialized correctly when Chrome
        // has been re-started after closing due to the last tab being closed when homepage is
        // enabled. See crbug.com/541546.
        observer.onAccessibilityModeChanged(isAccessibilityEnabled());
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring a11y mode change.
     */
    public static void removeObserver(Observer observer) {
        getObservers().removeObserver(observer);
    }

    /**
     * @return True if a hardware keyboard is detected.
     */
    public static boolean isHardwareKeyboardAttached(Configuration c) {
        return c.keyboard != Configuration.KEYBOARD_NOKEYS;
    }

    private static AccessibilityManager getAccessibilityManager() {
        return (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                Context.ACCESSIBILITY_SERVICE);
    }

    private static void registerModeChangeListeners() {
        assert sModeChangeHandler == null;
        sModeChangeHandler = new ModeChangeHandler();
        AccessibilityManager manager = getAccessibilityManager();
        manager.addAccessibilityStateChangeListener(sModeChangeHandler);
        manager.addTouchExplorationStateChangeListener(sModeChangeHandler);
    }

    private static void cleanUp() {
        if (sObservers != null) sObservers.clear();
        if (sModeChangeHandler == null) return;
        AccessibilityManager manager = getAccessibilityManager();
        manager.removeAccessibilityStateChangeListener(sModeChangeHandler);
        manager.removeTouchExplorationStateChangeListener(sModeChangeHandler);
    }

    /**
     * Reset the static used to determine whether accessibility is enabled.
     */
    private static void resetAccessibilityEnabled() {
        ApplicationStatus.unregisterActivityStateListener(sActivityStateListener);
        sActivityStateListener = null;
        sIsAccessibilityEnabled = null;
    }

    private static ObserverList<Observer> getObservers() {
        if (sObservers == null) sObservers = new ObserverList<>();
        return sObservers;
    }

    /**
     * Notify all the observers of the mode change.
     */
    private static void notifyModeChange() {
        boolean enabled = isAccessibilityEnabled();
        for (Observer observer : getObservers()) {
            observer.onAccessibilityModeChanged(enabled);
        }
    }

    /**
     * Checks whether the given {@link AccessibilityServiceInfo} can perform gestures.
     * @param service The service to check.
     * @return Whether the {@code service} can perform gestures. On N+, this relies on the
     *         capabilities the service can perform. On L & M, this looks specifically for
     *         Switch Access.
     */
    private static boolean canPerformGestures(AccessibilityServiceInfo service) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return (service.getCapabilities()
                           & AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES)
                    != 0;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return service.getResolveInfo() != null
                    && service.getResolveInfo().toString().contains("switchaccess");
        }
        return false;
    }

    /**
     * Set whether the device has accessibility enabled. Should be reset back to null after the test
     * has finished.
     * @param isEnabled whether the device has accessibility enabled.
     */
    @VisibleForTesting
    public static void setAccessibilityEnabledForTesting(@Nullable Boolean isEnabled) {
        sIsAccessibilityEnabled = isEnabled;
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, AccessibilityUtil::notifyModeChange);
    }
}
