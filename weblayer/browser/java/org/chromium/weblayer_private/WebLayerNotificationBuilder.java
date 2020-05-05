// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.webkit.WebViewFactory;

import org.chromium.components.browser_ui.notifications.ChromeNotificationBuilder;
import org.chromium.components.browser_ui.notifications.NotificationBuilder;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/** A notification builder for WebLayer which has extra logic to make icons work correctly. */
final class WebLayerNotificationBuilder extends NotificationBuilder {
    public WebLayerNotificationBuilder(Context context, String channelId,
            ChannelsInitializer channelsInitializer, NotificationMetadata metadata) {
        super(context, channelId, channelsInitializer, metadata);
    }

    @Override
    public ChromeNotificationBuilder setSmallIcon(int icon) {
        if (WebLayerImpl.isAndroidResource(icon)) {
            super.setSmallIcon(icon);
            return this;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            super.setSmallIcon(
                    Icon.createWithResource(WebViewFactory.getLoadedPackageInfo().packageName,
                            WebLayerImpl.getResourceIdForSystemUi(icon)));
        } else {
            // Some fallback is required, or the notification won't appear.
            super.setSmallIcon(android.R.drawable.radiobutton_on_background);
        }
        return this;
    }
}
