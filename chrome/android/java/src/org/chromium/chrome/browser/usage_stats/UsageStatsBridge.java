// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.usage_stats.WebsiteEventProtos.WebsiteEvent;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Provides access to native implementation of usage stats storage.
 */
@JNINamespace("usage_stats")
public class UsageStatsBridge {
    private final UsageStatsService mUsageStatsService;

    private long mNativeUsageStatsBridge;

    /**
     * Creates a {@link UsageStatsBridge} for accessing native usage stats storage implementation
     * for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user for whom we are tracking usage stats.
     */
    public UsageStatsBridge(Profile profile, UsageStatsService usageStatsService) {
        mNativeUsageStatsBridge = nativeInit(profile);
        mUsageStatsService = usageStatsService;
    }

    /** Cleans up native side of this bridge. */
    public void destroy() {
        assert mNativeUsageStatsBridge != 0;
        nativeDestroy(mNativeUsageStatsBridge);
        mNativeUsageStatsBridge = 0;
    }

    public void getAllEvents(Callback<List<WebsiteEvent>> callback) {
        assert mNativeUsageStatsBridge != 0;
        nativeGetAllEvents(mNativeUsageStatsBridge, callback);
    }

    public void queryEventsInRange(
            long startMs, long endMs, Callback<List<WebsiteEvent>> callback) {
        assert mNativeUsageStatsBridge != 0;
        long startSeconds = TimeUnit.MILLISECONDS.toSeconds(startMs);
        long endSeconds = TimeUnit.MILLISECONDS.toSeconds(endMs);
        nativeQueryEventsInRange(mNativeUsageStatsBridge, startSeconds, endSeconds, callback);
    }

    public void addEvents(List<WebsiteEvent> events, Callback<Boolean> callback) {
        assert mNativeUsageStatsBridge != 0;

        byte[][] serializedEvents = new byte[events.size()][];

        for (int i = 0; i < events.size(); i++) {
            WebsiteEvent event = events.get(i);
            serializedEvents[i] = event.toByteArray();
        }

        nativeAddEvents(mNativeUsageStatsBridge, serializedEvents, callback);
    }

    public void deleteAllEvents(Callback<Boolean> callback) {
        assert mNativeUsageStatsBridge != 0;
        nativeDeleteAllEvents(mNativeUsageStatsBridge, callback);
    }

    public void deleteEventsInRange(long startMs, long endMs, Callback<Boolean> callback) {
        assert mNativeUsageStatsBridge != 0;
        long startSeconds = TimeUnit.MILLISECONDS.toSeconds(startMs);
        long endSeconds = TimeUnit.MILLISECONDS.toSeconds(endMs);
        nativeDeleteEventsInRange(mNativeUsageStatsBridge, startSeconds, endSeconds, callback);
    }

    public void deleteEventsWithMatchingDomains(String[] domains, Callback<Boolean> callback) {
        assert mNativeUsageStatsBridge != 0;
        nativeDeleteEventsWithMatchingDomains(mNativeUsageStatsBridge, domains, callback);
    }

    public void getAllSuspensions(Callback<List<String>> callback) {
        assert mNativeUsageStatsBridge != 0;
        nativeGetAllSuspensions(mNativeUsageStatsBridge,
                arr -> { callback.onResult(new ArrayList<>(Arrays.asList(arr))); });
    }

    public void setSuspensions(String[] domains, Callback<Boolean> callback) {
        assert mNativeUsageStatsBridge != 0;
        nativeSetSuspensions(mNativeUsageStatsBridge, domains, callback);
    }

    public void getAllTokenMappings(Callback<Map<String, String>> callback) {
        assert mNativeUsageStatsBridge != 0;
        nativeGetAllTokenMappings(mNativeUsageStatsBridge, callback);
    }

    public void setTokenMappings(Map<String, String> mappings, Callback<Boolean> callback) {
        assert mNativeUsageStatsBridge != 0;

        // Split map into separate String arrays of tokens (keys) and FQDNs (values) for passing
        // over JNI bridge.
        String[] tokens = new String[mappings.size()];
        String[] fqdns = new String[mappings.size()];

        int i = 0;
        for (Map.Entry<String, String> entry : mappings.entrySet()) {
            tokens[i] = entry.getKey();
            fqdns[i] = entry.getValue();
            i++;
        }

        nativeSetTokenMappings(mNativeUsageStatsBridge, tokens, fqdns, callback);
    }

    @CalledByNative
    private static void createMapAndRunCallback(
            String[] keys, String[] values, Callback<Map<String, String>> callback) {
        assert keys.length == values.length;
        Map<String, String> map = new HashMap<>(keys.length);
        for (int i = 0; i < keys.length; i++) {
            map.put(keys[i], values[i]);
        }

        callback.onResult(map);
    }

    @CalledByNative
    private static void createEventListAndRunCallback(
            byte[][] serializedEvents, Callback<List<WebsiteEvent>> callback) {
        List<WebsiteEvent> events = new ArrayList<>(serializedEvents.length);

        for (byte[] serialized : serializedEvents) {
            try {
                WebsiteEvent event = WebsiteEvent.parseFrom(serialized);
                events.add(event);
            } catch (com.google.protobuf.InvalidProtocolBufferException e) {
                // Consume exception for now, ignoring unparseable events.
            }
        }

        callback.onResult(events);
    }

    @CalledByNative
    private void onAllHistoryDeleted() {
        mUsageStatsService.onAllHistoryDeleted();
    }

    @CalledByNative
    private void onHistoryDeletedInRange(long startTimeMs, long endTimeMs) {
        mUsageStatsService.onHistoryDeletedInRange(startTimeMs, endTimeMs);
    }

    @CalledByNative
    private void onHistoryDeletedForDomains(String[] fqdns) {
        mUsageStatsService.onHistoryDeletedForDomains(new ArrayList<>(Arrays.asList(fqdns)));
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeUsageStatsBridge);
    private native void nativeGetAllEvents(
            long nativeUsageStatsBridge, Callback<List<WebsiteEvent>> callback);
    private native void nativeQueryEventsInRange(long nativeUsageStatsBridge, long start, long end,
            Callback<List<WebsiteEvent>> callback);
    private native void nativeAddEvents(
            long nativeUsageStatsBridge, byte[][] events, Callback<Boolean> callback);
    private native void nativeDeleteAllEvents(
            long nativeUsageStatsBridge, Callback<Boolean> callback);
    private native void nativeDeleteEventsInRange(
            long nativeUsageStatsBridge, long start, long end, Callback<Boolean> callback);
    private native void nativeDeleteEventsWithMatchingDomains(
            long nativeUsageStatsBridge, String[] domains, Callback<Boolean> callback);
    private native void nativeGetAllSuspensions(
            long nativeUsageStatsBridge, Callback<String[]> callback);
    private native void nativeSetSuspensions(
            long nativeUsageStatsBridge, String[] domains, Callback<Boolean> callback);
    private native void nativeGetAllTokenMappings(
            long nativeUsageStatsBridge, Callback<Map<String, String>> callback);
    private native void nativeSetTokenMappings(long nativeUsageStatsBridge, String[] tokens,
            String[] fqdns, Callback<Boolean> callback);
}
