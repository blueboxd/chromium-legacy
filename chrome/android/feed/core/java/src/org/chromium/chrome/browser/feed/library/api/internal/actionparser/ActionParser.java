// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.actionparser;

import android.view.View;

import org.chromium.chrome.browser.feed.library.api.host.action.StreamActionApi;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;

/** Parses actions from Piet and directs the Stream to handle the action. */
public interface ActionParser {
    void parseAction(Action action, StreamActionApi streamActionApi, View view, LogData logData,
            @ActionSource int actionSource);

    void parseFeedActionPayload(FeedActionPayload feedActionPayload,
            StreamActionApi streamActionApi, View view, @ActionSource int actionSource);

    boolean canPerformAction(FeedActionPayload feedActionPayload, StreamActionApi streamActionApi);
}
