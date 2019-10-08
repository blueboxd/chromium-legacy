// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class TouchToFillView implements BottomSheet.BottomSheetContent {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ListView mCredentialListView;
    private final LinearLayout mContentView;
    private TouchToFillProperties.ViewEventListener mEventListener;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(int reason) {
            super.onSheetClosed(reason);
            assert mEventListener != null;
            mEventListener.onDismissed();
            mBottomSheetController.getBottomSheet().removeObserver(mBottomSheetObserver);
        }

        @Override
        public void onSheetFullyPeeked() {
            super.onSheetFullyPeeked();
            // Since isPeekStateEnabled doesn't seem to skip the Peek state, force-expand the sheet.
            mBottomSheetController.expandSheet();
        }
    };

    /**
     * Constructs a TouchToFillView which creates, modifies, and shows the bottom sheet.
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillView(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.touch_to_fill_sheet, null);
        mCredentialListView = mContentView.findViewById(R.id.credential_list);
        mCredentialListView.setOnItemClickListener(this::onItemSelected);
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     * @param viewEventListener A {@link TouchToFillProperties.ViewEventListener}.
     */
    void setEventListener(TouchToFillProperties.ViewEventListener viewEventListener) {
        mEventListener = viewEventListener;
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     */
    void setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.getBottomSheet().addObserver(mBottomSheetObserver);
            mBottomSheetController.requestShowContent(this, false);
        } else {
            mBottomSheetController.hideContent(this, false);
        }
    }

    /**
     * Renders the given url into the subtitle.
     * @param formattedUrl A {@link String} containing a URL already formatted to display.
     */
    void setFormattedUrl(String formattedUrl) {
        TextView sheetSubtitleText = mContentView.findViewById(R.id.touch_to_fill_sheet_subtitle);
        sheetSubtitleText.setText(String.format(
                mContext.getString(R.string.touch_to_fill_sheet_subtitle), formattedUrl));
    }

    void setCredentialListAdapter(ListAdapter adapter) {
        mCredentialListView.setAdapter(adapter);
    }

    private void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
        assert adapterView == mCredentialListView : "Use this click handler only for credentials!";
        assert mEventListener != null;
        mEventListener.onSelectItemAt(i);
    }

    @Override
    public void destroy() {
        mBottomSheetController.getBottomSheet().removeObserver(mBottomSheetObserver);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public int getPriority() {
        return BottomSheet.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean isPeekStateEnabled() {
        return true; // For some reason, false isn't working properly. Extend it explicitly!
    }

    @Override
    public boolean wrapContentEnabled() {
        return true;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.touch_to_fill_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_closed;
    }
}
