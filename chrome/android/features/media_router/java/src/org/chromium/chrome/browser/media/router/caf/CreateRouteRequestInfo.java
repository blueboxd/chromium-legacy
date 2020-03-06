// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import androidx.mediarouter.media.MediaRouter;

import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

/** The information of create route requests. */
public class CreateRouteRequestInfo {
    public final MediaSource source;
    public final MediaSink sink;
    public final String presentationId;
    public final String origin;
    public final int tabId;
    public final boolean isIncognito;
    public final int nativeRequestId;
    public final MediaRouter.RouteInfo routeInfo;

    public CreateRouteRequestInfo(MediaSource source, MediaSink sink, String presentationId,
            String origin, int tabId, boolean isIncognito, int nativeRequestId,
            MediaRouter.RouteInfo routeInfo) {
        this.source = source;
        this.sink = sink;
        this.presentationId = presentationId;
        this.origin = origin;
        this.tabId = tabId;
        this.isIncognito = isIncognito;
        this.nativeRequestId = nativeRequestId;
        this.routeInfo = routeInfo;
    }
}
