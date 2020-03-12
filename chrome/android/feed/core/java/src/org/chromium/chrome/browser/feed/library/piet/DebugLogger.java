// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.widget.TextViewCompat;

import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Utility class for collecting and displaying debug information. */
class DebugLogger {
    // Formatting parameters for report views:
    private static final int PADDING = 4;
    private static final int SIDE_PADDING = 16;
    private static final int DIVIDER_COLOR = 0x65000000;

    private static final int ERROR_BACKGROUND_COLOR = 0xFFEF9A9A;
    private static final int WARNING_BACKGROUND_COLOR = 0xFFFFFF66;

    @VisibleForTesting
    static final int ERROR_DIVIDER_WIDTH_DP = 1;

    /** What kind of error are we reporting when calling {@link #recordMessage(int, String)}. */
    @IntDef({MessageType.ERROR, MessageType.WARNING})
    @interface MessageType {
        int ERROR = 1;
        int WARNING = 2;
    }

    private final SparseArray<List<ErrorCodeAndMessage>> mMessages;
    private final SparseIntArray mBackgroundColors;

    DebugLogger() {
        mMessages = new SparseArray<>();
        mMessages.put(MessageType.ERROR, new ArrayList<>());
        mMessages.put(MessageType.WARNING, new ArrayList<>());

        mBackgroundColors = new SparseIntArray();
        mBackgroundColors.put(MessageType.ERROR, ERROR_BACKGROUND_COLOR);
        mBackgroundColors.put(MessageType.WARNING, WARNING_BACKGROUND_COLOR);
    }

    // TODO: Deprecate this version to reduce the use of ERR_UNSPECIFIED.
    void recordMessage(@MessageType int messageType, String error) {
        recordMessage(messageType, ErrorCode.ERR_UNSPECIFIED, error);
    }

    void recordMessage(@MessageType int messageType, ErrorCode errorCode, String error) {
        mMessages.get(messageType).add(new ErrorCodeAndMessage(errorCode, error));
    }

    /**
     * Create a {@code View} containing all the messages of a certain type; null for no messages.
     */
    @Nullable
    View getReportView(@MessageType int messageType, Context context) {
        List<ErrorCodeAndMessage> errors = this.mMessages.get(messageType);
        if (errors.isEmpty()) {
            return null;
        }
        LinearLayout view = new LinearLayout(context);
        view.setOrientation(LinearLayout.VERTICAL);
        LayoutParams layoutParams =
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        view.setLayoutParams(layoutParams);
        view.setBackgroundColor(mBackgroundColors.get(messageType));
        view.addView(getDivider(context));
        for (ErrorCodeAndMessage error : errors) {
            view.addView(getMessageTextView(error.mMessage, context));
        }
        return view;
    }

    @VisibleForTesting
    List<ErrorCodeAndMessage> getMessages(@MessageType int messageType) {
        return mMessages.get(messageType);
    }

    List<ErrorCode> getErrorCodes() {
        ArrayList<ErrorCode> errorCodes = new ArrayList<>();
        for (int i = 0; i < mMessages.size(); i++) {
            for (ErrorCodeAndMessage errorCodeAndMessage : mMessages.valueAt(i)) {
                errorCodes.add(errorCodeAndMessage.mErrorCode);
            }
        }
        errorCodes.trimToSize();
        return Collections.unmodifiableList(errorCodes);
    }

    private View getDivider(Context context) {
        View v = new View(context);
        LayoutParams layoutParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
                (int) LayoutUtils.dpToPx(ERROR_DIVIDER_WIDTH_DP, context));
        v.setLayoutParams(layoutParams);
        v.setBackgroundColor(DIVIDER_COLOR);
        return v;
    }

    private TextView getMessageTextView(String message, Context context) {
        TextView textView = new TextView(context);
        TextViewCompat.setTextAppearance(textView, R.style.gm_font_weight_regular);
        textView.setPadding((int) LayoutUtils.dpToPx(SIDE_PADDING, context),
                (int) LayoutUtils.dpToPx(PADDING, context),
                (int) LayoutUtils.dpToPx(SIDE_PADDING, context),
                (int) LayoutUtils.dpToPx(PADDING, context));
        textView.setText(message);
        return textView;
    }

    /** Simple class to hold an error code and message pair. */
    static class ErrorCodeAndMessage {
        final ErrorCode mErrorCode;
        final String mMessage;

        ErrorCodeAndMessage(ErrorCode errorCode, String message) {
            this.mErrorCode = errorCode;
            this.mMessage = message;
        }

        @Override
        public boolean equals(@Nullable Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof ErrorCodeAndMessage)) {
                return false;
            }

            ErrorCodeAndMessage that = (ErrorCodeAndMessage) o;

            if (mErrorCode != that.mErrorCode) {
                return false;
            }
            return mMessage.equals(that.mMessage);
        }

        @Override
        public int hashCode() {
            int result = mErrorCode.hashCode();
            result = 31 * result + mMessage.hashCode();
            return result;
        }
    }
}
