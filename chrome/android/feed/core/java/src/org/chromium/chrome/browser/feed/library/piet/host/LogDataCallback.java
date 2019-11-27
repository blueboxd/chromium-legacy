// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.view.View;

import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;

public interface LogDataCallback {
    void onBind(LogData logData, View view);

    void onUnbind(LogData logData, View view);
}
