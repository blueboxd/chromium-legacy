// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.actionparser;

import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;

/** Utility class to convert a {@link ActionType} to {@link ActionSource}. */
public final class ActionSourceConverter {
    private ActionSourceConverter() {}

    @ActionSource
    public static int convertPietAction(@ActionType int type) {
        switch (type) {
            case ActionType.VIEW:
                return ActionSource.VIEW;
            case ActionType.CLICK:
                return ActionSource.CLICK;
            case ActionType.LONG_CLICK:
                return ActionSource.LONG_CLICK;
            default:
                return ActionSource.UNKNOWN;
        }
    }
}
