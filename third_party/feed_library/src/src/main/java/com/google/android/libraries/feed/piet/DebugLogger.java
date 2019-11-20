// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.piet;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.support.v4.widget.TextViewCompat;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
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

  @VisibleForTesting static final int ERROR_DIVIDER_WIDTH_DP = 1;

  /** What kind of error are we reporting when calling {@link #recordMessage(int, String)}. */
  @IntDef({MessageType.ERROR, MessageType.WARNING})
  @interface MessageType {
    int ERROR = 1;
    int WARNING = 2;
  }

  private final SparseArray<List<ErrorCodeAndMessage>> messages;
  private final SparseIntArray backgroundColors;

  DebugLogger() {
    messages = new SparseArray<>();
    messages.put(MessageType.ERROR, new ArrayList<>());
    messages.put(MessageType.WARNING, new ArrayList<>());

    backgroundColors = new SparseIntArray();
    backgroundColors.put(MessageType.ERROR, ERROR_BACKGROUND_COLOR);
    backgroundColors.put(MessageType.WARNING, WARNING_BACKGROUND_COLOR);
  }

  // TODO: Deprecate this version to reduce the use of ERR_UNSPECIFIED.
  void recordMessage(@MessageType int messageType, String error) {
    recordMessage(messageType, ErrorCode.ERR_UNSPECIFIED, error);
  }

  void recordMessage(@MessageType int messageType, ErrorCode errorCode, String error) {
    messages.get(messageType).add(new ErrorCodeAndMessage(errorCode, error));
  }

  /** Create a {@code View} containing all the messages of a certain type; null for no messages. */
  /*@Nullable*/
  View getReportView(@MessageType int messageType, Context context) {
    List<ErrorCodeAndMessage> errors = this.messages.get(messageType);
    if (errors.isEmpty()) {
      return null;
    }
    LinearLayout view = new LinearLayout(context);
    view.setOrientation(LinearLayout.VERTICAL);
    LayoutParams layoutParams =
        new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
    view.setLayoutParams(layoutParams);
    view.setBackgroundColor(backgroundColors.get(messageType));
    view.addView(getDivider(context));
    for (ErrorCodeAndMessage error : errors) {
      view.addView(getMessageTextView(error.message, context));
    }
    return view;
  }

  @VisibleForTesting
  List<ErrorCodeAndMessage> getMessages(@MessageType int messageType) {
    return messages.get(messageType);
  }

  List<ErrorCode> getErrorCodes() {
    ArrayList<ErrorCode> errorCodes = new ArrayList<>();
    for (int i = 0; i < messages.size(); i++) {
      for (ErrorCodeAndMessage errorCodeAndMessage : messages.valueAt(i)) {
        errorCodes.add(errorCodeAndMessage.errorCode);
      }
    }
    errorCodes.trimToSize();
    return Collections.unmodifiableList(errorCodes);
  }

  private View getDivider(Context context) {
    View v = new View(context);
    LayoutParams layoutParams =
        new LinearLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, (int) LayoutUtils.dpToPx(ERROR_DIVIDER_WIDTH_DP, context));
    v.setLayoutParams(layoutParams);
    v.setBackgroundColor(DIVIDER_COLOR);
    return v;
  }

  private TextView getMessageTextView(String message, Context context) {
    TextView textView = new TextView(context);
    TextViewCompat.setTextAppearance(textView, R.style.gm_font_weight_regular);
    textView.setPadding(
        (int) LayoutUtils.dpToPx(SIDE_PADDING, context),
        (int) LayoutUtils.dpToPx(PADDING, context),
        (int) LayoutUtils.dpToPx(SIDE_PADDING, context),
        (int) LayoutUtils.dpToPx(PADDING, context));
    textView.setText(message);
    return textView;
  }

  /** Simple class to hold an error code and message pair. */
  static class ErrorCodeAndMessage {
    final ErrorCode errorCode;
    final String message;

    ErrorCodeAndMessage(ErrorCode errorCode, String message) {
      this.errorCode = errorCode;
      this.message = message;
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
      if (this == o) {
        return true;
      }
      if (!(o instanceof ErrorCodeAndMessage)) {
        return false;
      }

      ErrorCodeAndMessage that = (ErrorCodeAndMessage) o;

      if (errorCode != that.errorCode) {
        return false;
      }
      return message.equals(that.message);
    }

    @Override
    public int hashCode() {
      int result = errorCode.hashCode();
      result = 31 * result + message.hashCode();
      return result;
    }
  }
}
