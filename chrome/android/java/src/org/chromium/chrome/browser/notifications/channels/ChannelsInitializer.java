// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.annotation.TargetApi;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.content.res.Resources;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.CollectionUtil;
import org.chromium.chrome.browser.webapps.WebApkServiceClient;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * Initializes our notification channels.
 */
@TargetApi(Build.VERSION_CODES.O)
public class ChannelsInitializer {
    private final NotificationManagerProxy mNotificationManager;
    private Resources mResources;

    public ChannelsInitializer(
            NotificationManagerProxy notificationManagerProxy, Resources resources) {
        mNotificationManager = notificationManagerProxy;
        mResources = resources;
    }

    /**
     * Creates all the channels on the notification manager that we want to appear in our
     * channel settings from first launch onwards.
     */
    void initializeStartupChannels() {
        Set<String> groupIds = ChannelDefinitions.getStartupChannelGroupIds();
        Set<String> channelIds = ChannelDefinitions.getStartupChannelIds();
        ensureInitialized(groupIds, channelIds);
    }

    /**
     * Updates all the channels to reflect the correct locale.
     *
     * @param resources The new resources to use.
     */
    void updateLocale(Resources resources) {
        mResources = resources;
        Set<String> groupIds = new HashSet<>();
        Set<String> channelIds = new HashSet<>();
        for (NotificationChannelGroup group : mNotificationManager.getNotificationChannelGroups()) {
            groupIds.add(group.getId());
        }
        for (NotificationChannel channel : mNotificationManager.getNotificationChannels()) {
            channelIds.add(channel.getId());
        }
        // only re-initialize known channel ids, as we only want to update known & existing channels
        groupIds.retainAll(ChannelDefinitions.getAllChannelGroupIds());
        channelIds.retainAll(ChannelDefinitions.getAllChannelIds());
        ensureInitialized(groupIds, channelIds);
    }

    /**
     * Cleans up any old channels that are no longer required from previous versions of the app.
     * It's safe to call this multiple times since deleting an already-deleted channel is a no-op.
     */
    void deleteLegacyChannels() {
        for (String channelId : ChannelDefinitions.getLegacyChannelIds()) {
            mNotificationManager.deleteNotificationChannel(channelId);
        }
    }

    /**
     * Ensures the given channel has been created on the notification manager so a notification
     * can be safely posted to it. This should only be used for channel ids with an entry in
     * {@link ChannelDefinitions.PredefinedChannels}, or that start with a known prefix.
     *
     * Calling this is a (potentially lengthy) no-op if the channel has already been created.
     *
     * @param channelId The ID of the channel to be initialized.
     */
    public void ensureInitialized(String channelId) {
        ensureInitializedWithEnabledState(channelId, true);
    }

    /**
     * Ensures the given channels have been created on the notification manager so a notification
     * can be safely posted to them. This should only be used for channel ids with an entry in
     * {@link ChannelDefinitions.PredefinedChannels}, or that start with a known prefix.
     *
     * Calling this is a (potentially lengthy) no-op if the channels have already been created.
     *
     * @param groupIds The IDs of the channel groups to be initialized.
     * @param channelIds The IDs of the channel to be initialized.
     */
    public void ensureInitialized(Collection<String> groupIds, Collection<String> channelIds) {
        ensureInitializedWithEnabledState(groupIds, channelIds, true);
    }

    /**
     * As ensureInitialized, but create the channel in disabled mode. The channel's importance will
     * be set to IMPORTANCE_NONE, instead of using the value from
     * {@link ChannelDefinitions.PredefinedChannels}.
     */
    public void ensureInitializedAndDisabled(String channelId) {
        ensureInitializedWithEnabledState(channelId, false);
    }

    private ChannelDefinitions.PredefinedChannel getPredefinedChannel(String channelId) {
        if (channelId.startsWith(ChannelDefinitions.CHANNEL_ID_PREFIX_SITES)) {
            // If we have a valid site channel ID at this point, it is safe to assume a channel
            // has already been created, since the only way to obtain a site channel ID is by
            // creating a channel.
            assert SiteChannelsManager.isValidSiteChannelId(channelId);
            return null;
        }
        ChannelDefinitions.PredefinedChannel predefinedChannel =
                ChannelDefinitions.getChannelFromId(channelId);
        if (predefinedChannel == null) {
            throw new IllegalStateException("Could not initialize channel: " + channelId);
        }
        return predefinedChannel;
    }

    private void ensureInitializedWithEnabledState(String channelId, boolean enabled) {
        Collection<String> groupIds = Collections.emptyList();
        Collection<String> channelIds = CollectionUtil.newArrayList(channelId);
        ensureInitializedWithEnabledState(groupIds, channelIds, enabled);
    }

    private void ensureInitializedWithEnabledState(
            Collection<String> groupIds, Collection<String> channelIds, boolean enabled) {
        HashMap<String, NotificationChannelGroup> channelGroups = new HashMap<>();
        HashMap<String, NotificationChannel> channels = new HashMap<>();

        for (String groupId : groupIds) {
            ChannelDefinitions.PredefinedChannelGroup predefinedChannelGroup =
                    ChannelDefinitions.getChannelGroup(groupId);
            if (predefinedChannelGroup == null) continue;
            NotificationChannelGroup channelGroup =
                    predefinedChannelGroup.toNotificationChannelGroup(mResources);
            channelGroups.put(channelGroup.getId(), channelGroup);
        }

        for (String channelId : channelIds) {
            ChannelDefinitions.PredefinedChannel predefinedChannel =
                    getPredefinedChannel(channelId);
            if (predefinedChannel == null) continue;
            NotificationChannelGroup channelGroup =
                    ChannelDefinitions.getChannelGroupForChannel(predefinedChannel)
                            .toNotificationChannelGroup(mResources);
            NotificationChannel channel = predefinedChannel.toNotificationChannel(mResources);
            if (!enabled) {
                channel.setImportance(NotificationManager.IMPORTANCE_NONE);
            }
            channelGroups.put(channelGroup.getId(), channelGroup);
            channels.put(channel.getId(), channel);
        }

        // Channel groups must be created before the channels.
        CollectionUtil.forEach(
                channelGroups.values(), mNotificationManager::createNotificationChannelGroup);
        CollectionUtil.forEach(channels.values(), mNotificationManager::createNotificationChannel);
    }

    /**
     * This calls ensureInitialized after checking this isn't a WebAPK channel ID or null.
     * @param channelId Id of the channel to be initialized.
     */
    public void safeInitialize(String channelId) {
        // The channelId may be null if the notification will be posted by another app that
        // does not target O or sets its own channels. E.g. WebAPK notifications.
        if (channelId == null) {
            return;
        }
        // If the channel ID matches {@link WebApkServiceClient#CHANNEL_ID_WEBAPKS}, we don't create
        // the channel in Chrome. Instead, the channel will be created in WebAPKs.
        if (TextUtils.equals(channelId, WebApkServiceClient.CHANNEL_ID_WEBAPKS)) {
            return;
        }
        ensureInitialized(channelId);
    }
}
