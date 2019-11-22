// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.DebugLogger.ErrorCodeAndMessage;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link DebugLogger}. */
@RunWith(RobolectricTestRunner.class)
public class DebugLoggerTest {
    private static final String ERROR_TEXT_1 = "Interdimensional rift formation.";
    private static final String ERROR_TEXT_2 = "Exotic particle containment breach.";
    private static final String WARNING_TEXT = "Noncompliant meson entanglement.";

    @Mock
    private FrameContext frameContext;

    private Context context;

    private DebugLogger debugLogger;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        debugLogger = new DebugLogger();
    }

    @Test
    public void testGetReportView_singleError() {
        debugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_1);

        LinearLayout reportView =
                (LinearLayout) debugLogger.getReportView(MessageType.ERROR, context);

        assertThat(reportView.getOrientation()).isEqualTo(LinearLayout.VERTICAL);
        assertThat(reportView.getLayoutParams().width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(reportView.getLayoutParams().height).isEqualTo(LayoutParams.WRAP_CONTENT);

        assertThat(reportView.getChildCount()).isEqualTo(2);

        // Check the divider
        assertThat(reportView.getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(reportView.getChildAt(0).getLayoutParams().height)
                .isEqualTo((int) LayoutUtils.dpToPx(DebugLogger.ERROR_DIVIDER_WIDTH_DP, context));

        // Check the error box
        assertThat(reportView.getChildAt(1)).isInstanceOf(TextView.class);
        TextView textErrorView = (TextView) reportView.getChildAt(1);
        assertThat(textErrorView.getText().toString()).isEqualTo(ERROR_TEXT_1);

        // Check that padding has been set (but don't check specific values)
        assertThat(textErrorView.getPaddingBottom()).isNotEqualTo(0);
        assertThat(textErrorView.getPaddingTop()).isNotEqualTo(0);
        assertThat(textErrorView.getPaddingStart()).isNotEqualTo(0);
        assertThat(textErrorView.getPaddingEnd()).isNotEqualTo(0);
    }

    @Test
    public void testGetReportView_multipleErrors() {
        debugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_1);
        debugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_2);

        LinearLayout reportView =
                (LinearLayout) debugLogger.getReportView(MessageType.ERROR, context);

        assertThat(reportView.getChildCount()).isEqualTo(3);

        assertThat(((TextView) reportView.getChildAt(1)).getText().toString())
                .isEqualTo(ERROR_TEXT_1);
        assertThat(((TextView) reportView.getChildAt(2)).getText().toString())
                .isEqualTo(ERROR_TEXT_2);
    }

    @Test
    public void testGetReportView_zeroErrors() {
        LinearLayout errorView =
                (LinearLayout) debugLogger.getReportView(MessageType.ERROR, context);

        assertThat(errorView).isNull();
    }

    @Test
    public void testGetMessageTypes() {
        assertThat(debugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThat(debugLogger.getMessages(MessageType.WARNING)).isEmpty();

        debugLogger.recordMessage(MessageType.WARNING, WARNING_TEXT);
        assertThat(debugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThat(debugLogger.getMessages(MessageType.WARNING))
                .containsExactly(new ErrorCodeAndMessage(ErrorCode.ERR_UNSPECIFIED, WARNING_TEXT));

        debugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_PROTO_TOO_LARGE, ERROR_TEXT_1);
        assertThat(debugLogger.getMessages(MessageType.ERROR))
                .containsExactly(
                        new ErrorCodeAndMessage(ErrorCode.ERR_PROTO_TOO_LARGE, ERROR_TEXT_1));
        assertThat(debugLogger.getMessages(MessageType.WARNING))
                .containsExactly(new ErrorCodeAndMessage(ErrorCode.ERR_UNSPECIFIED, WARNING_TEXT));
    }

    @Test
    public void testGetReportView_byType() {
        debugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_1);
        debugLogger.recordMessage(MessageType.WARNING, WARNING_TEXT);

        LinearLayout errorView =
                (LinearLayout) debugLogger.getReportView(MessageType.ERROR, context);

        assertThat(errorView.getChildCount()).isEqualTo(2);
        assertThat(((TextView) errorView.getChildAt(1)).getText().toString())
                .isEqualTo(ERROR_TEXT_1);

        LinearLayout warningView =
                (LinearLayout) debugLogger.getReportView(MessageType.WARNING, context);

        assertThat(warningView.getChildCount()).isEqualTo(2);
        assertThat(((TextView) warningView.getChildAt(1)).getText().toString())
                .isEqualTo(WARNING_TEXT);
    }

    @Test
    public void testGetErrorCodes() {
        debugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_MISSING_FONTS, ERROR_TEXT_1);
        debugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_MISSING_FONTS, ERROR_TEXT_2);
        debugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_DUPLICATE_STYLE, ERROR_TEXT_1);
        debugLogger.recordMessage(MessageType.WARNING, ErrorCode.ERR_MISSING_FONTS, WARNING_TEXT);
        debugLogger.recordMessage(
                MessageType.WARNING, ErrorCode.ERR_DUPLICATE_TEMPLATE, WARNING_TEXT);

        assertThat(debugLogger.getErrorCodes())
                .containsExactly(ErrorCode.ERR_MISSING_FONTS, ErrorCode.ERR_MISSING_FONTS,
                        ErrorCode.ERR_DUPLICATE_STYLE, ErrorCode.ERR_MISSING_FONTS,
                        ErrorCode.ERR_DUPLICATE_TEMPLATE);
    }
}
