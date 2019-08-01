// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Represents a view that works as the entrance for IPH in {@link TabListRecyclerView}.
 */
public class TabGridIphItemView extends FrameLayout {
    private TextView mShowIPHButton;
    private ChromeImageView mCloseIphEntranceButton;
    private PopupWindow mIphWindow;
    private ScrimView mScrimView;
    private ScrimView.ScrimParams mScrimParams;
    private Drawable mIphDrawable;

    public TabGridIphItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        FrameLayout backgroundView = new FrameLayout(getContext());
        View iphDialogView =
                LayoutInflater.from(getContext())
                        .inflate(R.layout.iph_drag_and_drop_dialog_layout, backgroundView, false);
        mShowIPHButton = findViewById(R.id.show_me_button);
        mCloseIphEntranceButton = findViewById(R.id.close_iph_button);
        mIphDrawable =
                ((ImageView) iphDialogView.findViewById(R.id.animation_drawable)).getDrawable();
        TextView closeIphDialogButton =
                iphDialogView.findViewById(R.id.iph_drag_and_drop_dialog_close_button);
        backgroundView.addView(iphDialogView);
        mIphWindow = new PopupWindow(backgroundView, ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        mScrimView = new ScrimView(getContext(), null, backgroundView);
        ScrimView.ScrimObserver scrimObserver = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                mIphWindow.dismiss();
                mScrimView.hideScrim(true);
            }
            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mScrimParams = new ScrimView.ScrimParams(iphDialogView, false, true, 0, scrimObserver);
        closeIphDialogButton.setOnClickListener(View -> scrimObserver.onScrimClick());
    }

    /**
     * Setup the {@link View.OnClickListener} for ShowIPH button.
     */
    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        View parent = (View) getParent();
        mShowIPHButton.setOnClickListener((View view) -> {
            // When show me button is clicked, the entrance should be closed.
            mCloseIphEntranceButton.performClick();
            mScrimView.showScrim(mScrimParams);
            mIphWindow.showAtLocation(parent, Gravity.CENTER, 0, 0);
            ((Animatable) mIphDrawable).start();
        });
    }

    /**
     * Setup the {@link View.OnClickListener} for close button in the entrance of IPH.
     *
     * @param listener The {@link View.OnClickListener} used to setup close button in IPH entrance.
     */
    void setupCloseIphEntranceButtonOnclickListener(View.OnClickListener listener) {
        mCloseIphEntranceButton.setOnClickListener(listener);
    }
}
