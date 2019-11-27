// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

import java.util.ArrayList;
import java.util.List;

/** Logger that logs to the {@link BasicLoggingApi} any Piet errors. */
public class PietEventLogger {
    private final BasicLoggingApi mBasicLoggingApi;

    public PietEventLogger(BasicLoggingApi basicLoggingApi) {
        this.mBasicLoggingApi = basicLoggingApi;
    }

    public void logEvents(List<ErrorCode> pietErrors) {
        mBasicLoggingApi.onPietFrameRenderingEvent(convertErrorCodes(pietErrors));
    }

    private List<Integer> convertErrorCodes(List<ErrorCode> errorCodes) {
        List<Integer> errorCodeValues = new ArrayList<>(errorCodes.size());
        for (ErrorCode errorCode : errorCodes) {
            errorCodeValues.add(errorCode.getNumber());
        }
        return errorCodeValues;
    }
}
