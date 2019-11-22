// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.actionparser;

import android.view.View;

import com.google.android.libraries.feed.api.host.action.StreamActionApi;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.LogDataProto.LogData;

/** Parses actions from Piet and directs the Stream to handle the action. */
public interface ActionParser {
    void parseAction(Action action, StreamActionApi streamActionApi, View view, LogData logData,
            @ActionSource int actionSource);

    void parseFeedActionPayload(FeedActionPayload feedActionPayload,
            StreamActionApi streamActionApi, View view, @ActionSource int actionSource);

    boolean canPerformAction(FeedActionPayload feedActionPayload, StreamActionApi streamActionApi);
}
