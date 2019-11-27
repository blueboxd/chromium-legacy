// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionparser.internal;

import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type;

/** Utility class to convert a {@link Type} to {@link ActionType}. */
public final class ActionTypesConverter {
    private ActionTypesConverter() {}

    @ActionType
    public static int convert(Type type) {
        // LINT.IfChange
        switch (type) {
            case OPEN_URL:
                return ActionType.OPEN_URL;
            case OPEN_URL_INCOGNITO:
                return ActionType.OPEN_URL_INCOGNITO;
            case OPEN_URL_NEW_TAB:
                return ActionType.OPEN_URL_NEW_TAB;
            case OPEN_URL_NEW_WINDOW:
                return ActionType.OPEN_URL_NEW_WINDOW;
            case DOWNLOAD:
                return ActionType.DOWNLOAD;
            case LEARN_MORE:
                return ActionType.LEARN_MORE;
            default:
                return ActionType.UNKNOWN;
        }
        // LINT.ThenChange
    }
}
