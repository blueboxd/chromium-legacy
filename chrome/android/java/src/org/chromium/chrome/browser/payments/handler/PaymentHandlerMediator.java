// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.DismissObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator extends EmptyBottomSheetObserver {
    private final Runnable mHider;
    private final DismissObserver mDismissObserver;

    /* package */ PaymentHandlerMediator(Runnable hider, DismissObserver dismissObserver) {
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
}
