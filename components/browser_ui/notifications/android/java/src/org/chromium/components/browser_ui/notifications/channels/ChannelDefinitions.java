// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications.channels;

import android.annotation.TargetApi;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.content.res.Resources;
import android.os.Build;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains the properties of all our pre-definable notification channels on Android O+. See also
 * {@link ChromeChannelDefinitions}. <br/>
 * <br/>
 * See the README.md alongside ChromeChannelDefinitions.java for more information before adding or
 * changing any channels.
 */
@TargetApi(Build.VERSION_CODES.O)
public abstract class ChannelDefinitions {
    /**
     * @return A set of all known channel group ids that can be used for {@link #getChannelGroup}.
     */
    public abstract Set<String> getAllChannelGroupIds();

    /**
     * @return A set of all known channel ids that can be used for {@link #getChannelFromId}.
     */
    public abstract Set<String> getAllChannelIds();

    /**
     * @return A set of channel ids of channels that should be initialized on startup.
     */
    public abstract Set<String> getStartupChannelIds();

    /**
     * @return A set of channel group ids of channel groups that should be initialized on startup.
     */
    public Set<String> getStartupChannelGroupIds() {
        Set<String> groupIds = new HashSet<>();
        for (String id : getStartupChannelIds()) {
            groupIds.add(getChannelFromId(id).mGroupId);
        }
        return groupIds;
    }

    /**
     * @return An array of old ChannelIds that may have been returned by
     *         {@link #getStartupChannelIds} in the past, but are no longer in use.
     */
    public abstract List<String> getLegacyChannelIds();

    public PredefinedChannelGroup getChannelGroupForChannel(PredefinedChannel channel) {
        return getChannelGroup(channel.mGroupId);
    }

    public abstract PredefinedChannelGroup getChannelGroup(String groupId);

    public abstract PredefinedChannel getChannelFromId(String channelId);

    /**
     * @param channelId a channel ID which may or may not correlate to a PredefinedChannel.
     * @return true if the input is a valid channel ID other than one returned in
     *     {@link getAllChannelIds}.
     */
    public boolean isValidNonPredefinedChannelId(String channelId) {
        return false;
    }

    /**
     * Helper class for storing predefined channel properties while allowing the channel name to be
     * lazily evaluated only when it is converted to an actual NotificationChannel.
     */
    public static class PredefinedChannel {
        private final String mId;
        private final int mNameResId;
        private final int mImportance;
        private final String mGroupId;
        private final boolean mShowNotificationBadges;

        public PredefinedChannel(String id, int nameResId, int importance, String groupId) {
            this(id, nameResId, importance, groupId, false /* showNotificationBadges */);
        }

        public PredefinedChannel(String id, int nameResId, int importance, String groupId,
                boolean showNotificationBadges) {
            this.mId = id;
            this.mNameResId = nameResId;
            this.mImportance = importance;
            this.mGroupId = groupId;
            this.mShowNotificationBadges = showNotificationBadges;
        }

        NotificationChannel toNotificationChannel(Resources resources) {
            String name = resources.getString(mNameResId);
            NotificationChannel channel = new NotificationChannel(mId, name, mImportance);
            channel.setGroup(mGroupId);
            channel.setShowBadge(mShowNotificationBadges);
            return channel;
        }
    }

    /**
     * Helper class for storing predefined channel group properties while allowing the group name to
     * be lazily evaluated only when it is converted to an actual NotificationChannelGroup.
     */
    public static class PredefinedChannelGroup {
        public final String mId;
        public final int mNameResId;

        public PredefinedChannelGroup(String id, int nameResId) {
            this.mId = id;
            this.mNameResId = nameResId;
        }

        public NotificationChannelGroup toNotificationChannelGroup(Resources resources) {
            String name = resources.getString(mNameResId);
            return new NotificationChannelGroup(mId, name);
        }
    }
}
