// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.text.Spannable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.util.ColorUtils;

/**
 * Container view for omnibox answer suggestions.
 */
public class AnswerSuggestionView extends RelativeLayout {
    private SuggestionViewDelegate mSuggestionDelegate;
    private View mAnswerView;
    private TextView mTextView1;
    private TextView mTextView2;
    private ImageView mAnswerIconView;
    private ImageView mRefineView;

    /**
     * Container view for omnibox suggestions allowing soft focus from keyboard.
     */
    public static class FocusableView extends RelativeLayout {
        /** Creates new instance of FocusableView. */
        public FocusableView(Context context, AttributeSet attributes) {
            super(context, attributes);
        }

        @Override
        public boolean isFocused() {
            return super.isFocused() || (isSelected() && !isInTouchMode());
        }
    }

    /** Creates new instance of AnswerSuggestionView. */
    public AnswerSuggestionView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTextView1 = findViewById(R.id.omnibox_answer_line_1);
        mTextView2 = findViewById(R.id.omnibox_answer_line_2);
        mAnswerIconView = findViewById(R.id.omnibox_answer_icon);
        mAnswerView = findViewById(R.id.omnibox_answer);
        mRefineView = findViewById(R.id.omnibox_answer_refine_icon);
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        mAnswerView.setSelected(selected);
        if (selected && !isInTouchMode()) {
            postDelegateAction(() -> mSuggestionDelegate.onSetUrlToSuggestion());
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        // Whenever the suggestion dropdown is touched, we dispatch onGestureDown which is
        // used to let autocomplete controller know that it should stop updating suggestions.
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                mSuggestionDelegate.onGestureDown();
                break;
            case MotionEvent.ACTION_UP:
                mSuggestionDelegate.onGestureUp(ev.getEventTime());
                break;
        }
        return super.dispatchTouchEvent(ev);
    }

    /** Specify delegate receiving click/refine events. */
    void setDelegate(SuggestionViewDelegate delegate) {
        mSuggestionDelegate = delegate;
        mAnswerView.setOnClickListener(
                (View v) -> postDelegateAction(() -> mSuggestionDelegate.onSelection()));
        mAnswerView.setOnLongClickListener((View v) -> {
            postDelegateAction(() -> mSuggestionDelegate.onLongPress());
            return true;
        });
        mRefineView.setOnClickListener(
                (View v) -> postDelegateAction(() -> mSuggestionDelegate.onRefineSuggestion()));
    }

    /**
     * Toggles theme.
     * @param useDarkColors specifies whether UI should use dark theme.
     */
    void setUseDarkColors(boolean useDarkColors) {
        Drawable drawable = mRefineView.getDrawable();
        DrawableCompat.setTint(
                drawable, ColorUtils.getIconTint(getContext(), !useDarkColors).getDefaultColor());
    }

    /**
     * Specifies text accessibility description of the first text line.
     * @param text Text to be announced.
     */
    void setLine1AccessibilityDescription(String text) {
        mTextView1.setContentDescription(text);
    }

    /**
     * Specifies text accessibility description of the second text line.
     * @param text Text to be announced.
     */
    void setLine2AccessibilityDescription(String text) {
        mTextView2.setContentDescription(text);
    }

    /**
     * Specifies text content of the first text line.
     * @param text Text to be displayed.
     */
    void setLine1TextContent(Spannable text) {
        mTextView1.setText(text);
    }

    /**
     * Specifies text content of the second text line.
     * @param text Text to be displayed.
     */
    void setLine2TextContent(Spannable text) {
        mTextView2.setText(text);
    }

    /**
     * Specifies image bitmap to be displayed as an answer icon.
     * @param bitmap Decoded image to be displayed as an answer icon.
     */
    void setIconBitmap(Bitmap bitmap) {
        BitmapDrawable drawable = new BitmapDrawable(bitmap);
        mAnswerIconView.setImageDrawable(drawable);
    }

    /**
     * Specifies fallback image to be presented if no image is available.
     * @param res Drawable resource ID to be used in place of answer image.
     */
    void setFallbackIconRes(@DrawableRes int res) {
        mAnswerIconView.setImageResource(res);
    }

    /**
     * Post delegate action to main thread. Invoked only if delegate is specified.
     * @param action Delegate action to invoke on the UI thread.
     */
    private void postDelegateAction(Runnable action) {
        if (mSuggestionDelegate == null) return;
        if (!post(action)) action.run();
    }
}
