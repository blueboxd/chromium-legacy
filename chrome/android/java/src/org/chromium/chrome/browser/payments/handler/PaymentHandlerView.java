// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;

/** PaymentHandler UI. */
/* package */ class PaymentHandlerView implements BottomSheetContent {
    private final View mToolbarView;
    private final View mContentView;

    /* package */ PaymentHandlerView(Context context) {
        mToolbarView = LayoutInflater.from(context).inflate(R.layout.payment_handler_toolbar, null);
        mContentView = LayoutInflater.from(context).inflate(R.layout.payment_handler_content, null);
    }

    // BottomSheetContent:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    @BottomSheet.ContentPriority
    public int getPriority() {
        // If multiple bottom sheets are queued up to be shown, prioritize payment-handler, because
        // it's triggered by a user gesture, such as a click on <button>Buy this article</button>.
        return BottomSheet.ContentPriority.HIGH;
    }

    @Override
    public boolean isPeekStateEnabled() {
        return false;
    }

    @Override
    public boolean wrapContentEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // flinging down hard enough will close the sheet.
        return true;
    }
}
