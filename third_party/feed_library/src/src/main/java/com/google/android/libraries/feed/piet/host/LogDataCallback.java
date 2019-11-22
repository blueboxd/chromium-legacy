// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.host;

import android.view.View;

import com.google.search.now.ui.piet.LogDataProto.LogData;

public interface LogDataCallback {
    void onBind(LogData logData, View view);

    void onUnbind(LogData logData, View view);
}
