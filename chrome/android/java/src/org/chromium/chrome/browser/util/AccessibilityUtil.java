// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.view.accessibility.AccessibilityManager;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;
import org.chromium.ui.widget.Toast;

import java.util.List;

/**
 * Exposes information about the current accessibility state
 */
public class AccessibilityUtil {
    private static Boolean sIsAccessibilityEnabled;
    private static ActivityStateListener sActivityStateListener;

    private AccessibilityUtil() { }

    /**
     * Checks to see that this device has accessibility and touch exploration enabled.
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    public static boolean isAccessibilityEnabled() {
        if (sIsAccessibilityEnabled != null) return sIsAccessibilityEnabled;

        TraceEvent.begin("AccessibilityManager::isAccessibilityEnabled");

        AccessibilityManager manager =
                (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);
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
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(sActivityStateListener);

        TraceEvent.end("AccessibilityManager::isAccessibilityEnabled");
        return sIsAccessibilityEnabled;
    }

    /**
     * Reset the static used to determine whether accessibility is enabled.
     * TODO(twellington): Make this private and have classes that care about accessibility state
     *                    observe this class rather than observing the AccessibilityManager
     *                    directly.
     */
    public static void resetAccessibilityEnabled() {
        ApplicationStatus.unregisterActivityStateListener(sActivityStateListener);
        sActivityStateListener = null;
        sIsAccessibilityEnabled = null;
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
     * Shows the content description toast for items on the toolbar.
     * @param context The context to use for the toast.
     * @param view The view to anchor the toast.
     * @param description The string shown in the toast.
     * @return Whether a toast has been shown successfully.
     */
    @SuppressLint("RtlHardcoded")
    public static boolean showAccessibilityToast(
            Context context, View view, CharSequence description) {
        if (description == null) return false;

        final int screenWidth = context.getResources().getDisplayMetrics().widthPixels;
        final int screenHeight = context.getResources().getDisplayMetrics().heightPixels;
        final int[] screenPos = new int[2];
        view.getLocationOnScreen(screenPos);
        final int width = view.getWidth();
        final int height = view.getHeight();

        final int horizontalGravity =
                (screenPos[0] < screenWidth / 2) ? Gravity.LEFT : Gravity.RIGHT;
        final int xOffset = (screenPos[0] < screenWidth / 2)
                ? screenPos[0] + width / 2
                : screenWidth - screenPos[0] - width / 2;
        final int yOffset = (screenPos[1] < screenHeight / 2) ? screenPos[1] + height / 2
                                                              : screenPos[1] - height * 3 / 2;

        Toast toast = Toast.makeText(context, description, Toast.LENGTH_SHORT);
        toast.setGravity(Gravity.TOP | horizontalGravity, xOffset, yOffset);
        toast.show();
        return true;
    }
}
