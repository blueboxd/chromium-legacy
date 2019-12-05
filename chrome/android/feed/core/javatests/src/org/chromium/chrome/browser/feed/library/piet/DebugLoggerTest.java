// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.ErrorCodeAndMessage;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link DebugLogger}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DebugLoggerTest {
    private static final String ERROR_TEXT_1 = "Interdimensional rift formation.";
    private static final String ERROR_TEXT_2 = "Exotic particle containment breach.";
    private static final String WARNING_TEXT = "Noncompliant meson entanglement.";

    @Mock
    private FrameContext mFrameContext;

    private Context mContext;

    private DebugLogger mDebugLogger;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mDebugLogger = new DebugLogger();
    }

    @Test
    public void testGetReportView_singleError() {
        mDebugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_1);

        LinearLayout reportView =
                (LinearLayout) mDebugLogger.getReportView(MessageType.ERROR, mContext);

        assertThat(reportView.getOrientation()).isEqualTo(LinearLayout.VERTICAL);
        assertThat(reportView.getLayoutParams().width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(reportView.getLayoutParams().height).isEqualTo(LayoutParams.WRAP_CONTENT);

        assertThat(reportView.getChildCount()).isEqualTo(2);

        // Check the divider
        assertThat(reportView.getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(reportView.getChildAt(0).getLayoutParams().height)
                .isEqualTo((int) LayoutUtils.dpToPx(DebugLogger.ERROR_DIVIDER_WIDTH_DP, mContext));

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
        mDebugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_1);
        mDebugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_2);

        LinearLayout reportView =
                (LinearLayout) mDebugLogger.getReportView(MessageType.ERROR, mContext);

        assertThat(reportView.getChildCount()).isEqualTo(3);

        assertThat(((TextView) reportView.getChildAt(1)).getText().toString())
                .isEqualTo(ERROR_TEXT_1);
        assertThat(((TextView) reportView.getChildAt(2)).getText().toString())
                .isEqualTo(ERROR_TEXT_2);
    }

    @Test
    public void testGetReportView_zeroErrors() {
        LinearLayout errorView =
                (LinearLayout) mDebugLogger.getReportView(MessageType.ERROR, mContext);

        assertThat(errorView).isNull();
    }

    @Test
    public void testGetMessageTypes() {
        assertThat(mDebugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThat(mDebugLogger.getMessages(MessageType.WARNING)).isEmpty();

        mDebugLogger.recordMessage(MessageType.WARNING, WARNING_TEXT);
        assertThat(mDebugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThat(mDebugLogger.getMessages(MessageType.WARNING))
                .containsExactly(new ErrorCodeAndMessage(ErrorCode.ERR_UNSPECIFIED, WARNING_TEXT));

        mDebugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_PROTO_TOO_LARGE, ERROR_TEXT_1);
        assertThat(mDebugLogger.getMessages(MessageType.ERROR))
                .containsExactly(
                        new ErrorCodeAndMessage(ErrorCode.ERR_PROTO_TOO_LARGE, ERROR_TEXT_1));
        assertThat(mDebugLogger.getMessages(MessageType.WARNING))
                .containsExactly(new ErrorCodeAndMessage(ErrorCode.ERR_UNSPECIFIED, WARNING_TEXT));
    }

    @Test
    public void testGetReportView_byType() {
        mDebugLogger.recordMessage(MessageType.ERROR, ERROR_TEXT_1);
        mDebugLogger.recordMessage(MessageType.WARNING, WARNING_TEXT);

        LinearLayout errorView =
                (LinearLayout) mDebugLogger.getReportView(MessageType.ERROR, mContext);

        assertThat(errorView.getChildCount()).isEqualTo(2);
        assertThat(((TextView) errorView.getChildAt(1)).getText().toString())
                .isEqualTo(ERROR_TEXT_1);

        LinearLayout warningView =
                (LinearLayout) mDebugLogger.getReportView(MessageType.WARNING, mContext);

        assertThat(warningView.getChildCount()).isEqualTo(2);
        assertThat(((TextView) warningView.getChildAt(1)).getText().toString())
                .isEqualTo(WARNING_TEXT);
    }

    @Test
    public void testGetErrorCodes() {
        mDebugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_MISSING_FONTS, ERROR_TEXT_1);
        mDebugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_MISSING_FONTS, ERROR_TEXT_2);
        mDebugLogger.recordMessage(MessageType.ERROR, ErrorCode.ERR_DUPLICATE_STYLE, ERROR_TEXT_1);
        mDebugLogger.recordMessage(MessageType.WARNING, ErrorCode.ERR_MISSING_FONTS, WARNING_TEXT);
        mDebugLogger.recordMessage(
                MessageType.WARNING, ErrorCode.ERR_DUPLICATE_TEMPLATE, WARNING_TEXT);

        assertThat(mDebugLogger.getErrorCodes())
                .containsExactly(ErrorCode.ERR_MISSING_FONTS, ErrorCode.ERR_MISSING_FONTS,
                        ErrorCode.ERR_DUPLICATE_STYLE, ErrorCode.ERR_MISSING_FONTS,
                        ErrorCode.ERR_DUPLICATE_TEMPLATE);
    }
}
