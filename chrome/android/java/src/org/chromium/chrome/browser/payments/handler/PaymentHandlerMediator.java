// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.DismissObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator extends EmptyBottomSheetObserver {
    private final PropertyModel mModel;
    private final Runnable mHider;
    private final DismissObserver mDismissObserver;

    /* package */ PaymentHandlerMediator(
            PropertyModel model, Runnable hider, DismissObserver dismissObserver) {
        mModel = model;
        mHider = hider;
        mDismissObserver = dismissObserver;
    }

    // EmptyBottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        switch (newState) {
            case SheetState.HIDDEN:
                mHider.run();
                mDismissObserver.onDismissed();
                break;
        }
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        mModel.set(PaymentHandlerProperties.BOTTOM_SHEET_HEIGHT_FRACTION, heightFraction);
    }
}
