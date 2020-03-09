// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Preferences used to provide prefetch related notifications.
 *  - Having new pages: boolean indicating whether new pages have been saved or not
 *  - Notification timestamp: the last time a notification is shown
 *  - Offline counter: how many times the task ran and seen that we are offline
 *  - Ignored notification counter: how many times in a row we showed a notification without user
 *    reacting to it
 */
public class PrefetchPrefs {

    /**
     * Sets the flag to tell whether prefetch notifications are enabled in user settings.
     */
    public static void setNotificationEnabled(boolean enabled) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PREFETCH_NOTIFICATION_ENABLED, enabled);
    }

    /**
     * Returns the flag to tell whether prefetch notifications are enabled in user settings.
     */
    public static boolean getNotificationEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.PREFETCH_NOTIFICATION_ENABLED, true);
    }

    /**
     * Sets the flag to tell whether new pages have been saved.
     */
    public static void setHasNewPages(boolean hasNewPages) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PREFETCH_HAS_NEW_PAGES, hasNewPages);
    }

    /**
     * Returns the flag to tell whether new pages have been saved.
     */
    public static boolean getHasNewPages() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.PREFETCH_HAS_NEW_PAGES, false);
    }

    /**
     * Sets the last time a notification is shown, in milliseconds since the epoch.
     */
    public static void setNotificationLastShownTime(long timeInMillis) {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.PREFETCH_NOTIFICATION_TIME, timeInMillis);
    }

    /**
     * Returns the last time a notification is shown, in milliseconds since the epoch.
     */
    public static long getNotificationLastShownTime() {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.PREFETCH_NOTIFICATION_TIME);
    }

    /**
     * Sets the offline counter.
     */
    public static void setOfflineCounter(int counter) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.PREFETCH_OFFLINE_COUNTER, counter);
    }

    /**
     * Increments and returns the offline counter.
     */
    public static int incrementOfflineCounter() {
        return SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.PREFETCH_OFFLINE_COUNTER);
    }

    /**
     * Sets the ignored notification counter.
     */
    public static void setIgnoredNotificationCounter(int counter) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.PREFETCH_IGNORED_NOTIFICATION_COUNTER, counter);
    }

    /**
     * Returns the ignored notification counter.
     */
    public static int getIgnoredNotificationCounter() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.PREFETCH_IGNORED_NOTIFICATION_COUNTER);
    }

    /**
     * Increments and returns the ignored notification counter.
     */
    public static int incrementIgnoredNotificationCounter() {
        return SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.PREFETCH_IGNORED_NOTIFICATION_COUNTER);
    }
}
