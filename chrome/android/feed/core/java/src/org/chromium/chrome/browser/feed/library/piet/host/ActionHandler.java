// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.support.annotation.IntDef;
import android.view.View;

import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;

/**
 * Interface from the Piet host which provides handling of {@link Action} from the UI. An instance
 * of this is provided by the host. When an action is raised by the client {@link
 * #handleAction(Action, int, Frame, View, String)} will be called.
 */
public interface ActionHandler {
    /** Called on a event, such as a click of a view, on a UI element */
    // TODO: Do we need the veLoggingToken, one is defined in the Action
    void handleAction(
            Action action, @ActionType int actionType, Frame frame, View view, LogData logData);

    /** Possible action types. */
    @IntDef({ActionType.VIEW, ActionType.CLICK, ActionType.LONG_CLICK})
    @interface ActionType {
        /** View action type */
        int VIEW = 0;

        /** Click action */
        int CLICK = 1;

        /** Long click action */
        int LONG_CLICK = 2;
    }
}
