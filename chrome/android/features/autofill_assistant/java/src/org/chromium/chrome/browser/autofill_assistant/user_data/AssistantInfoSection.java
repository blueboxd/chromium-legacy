// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;

/**
 * A section to display a text string.
 *
 * This section properly handles embedded links of the form <link0>some text</link0>.
 */
public class AssistantInfoSection {
    private final Context mContext;
    private final TextView mView;
    @Nullable
    private Callback<Integer> mListener;

    AssistantInfoSection(Context context, ViewGroup parent) {
        mContext = context;
        mView = new TextView(context);

        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        parent.addView(mView,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        setHorizontalMargins(mView, horizontalMargin, horizontalMargin);
    }

    void setListener(Callback<Integer> listener) {
        mListener = listener;
    }

    View getView() {
        return mView;
    }

    void setVisible(boolean visible) {
        mView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setText(String text) {
        if (TextUtils.isEmpty(text)) {
            mView.setVisibility(View.GONE);
        } else {
            AssistantTextUtils.applyVisualAppearanceTags(mView, text, this::onLinkClicked);
            mView.setVisibility(View.VISIBLE);
        }
    }

    private void setHorizontalMargins(View view, int marginStart, int marginEnd) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(marginStart);
        lp.setMarginEnd(marginEnd);
        view.setLayoutParams(lp);
    }

    private void onLinkClicked(int link) {
        if (mListener != null) {
            mListener.onResult(link);
        }
    }
}
