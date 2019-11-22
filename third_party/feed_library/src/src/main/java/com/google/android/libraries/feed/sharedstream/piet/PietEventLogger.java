// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;

import java.util.ArrayList;
import java.util.List;

/** Logger that logs to the {@link BasicLoggingApi} any Piet errors. */
public class PietEventLogger {
    private final BasicLoggingApi basicLoggingApi;

    public PietEventLogger(BasicLoggingApi basicLoggingApi) {
        this.basicLoggingApi = basicLoggingApi;
    }

    public void logEvents(List<ErrorCode> pietErrors) {
        basicLoggingApi.onPietFrameRenderingEvent(convertErrorCodes(pietErrors));
    }

    private List<Integer> convertErrorCodes(List<ErrorCode> errorCodes) {
        List<Integer> errorCodeValues = new ArrayList<>(errorCodes.size());
        for (ErrorCode errorCode : errorCodes) {
            errorCodeValues.add(errorCode.getNumber());
        }
        return errorCodeValues;
    }
}
