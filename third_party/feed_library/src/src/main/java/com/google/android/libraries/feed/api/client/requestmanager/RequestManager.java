// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.client.requestmanager;

/** Creates and issues requests to the server. */
public interface RequestManager {
    /** Issues a request to refresh the entire feed. */
    void triggerScheduledRefresh();
}
