// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;

/** Exception that carries a Piet error code */
class PietFatalException extends IllegalArgumentException {
    private final ErrorCode mErrorCode;

    PietFatalException(ErrorCode errorCode, String message) {
        super(message);
        this.mErrorCode = errorCode;
    }

    ErrorCode getErrorCode() {
        return mErrorCode;
    }
}
