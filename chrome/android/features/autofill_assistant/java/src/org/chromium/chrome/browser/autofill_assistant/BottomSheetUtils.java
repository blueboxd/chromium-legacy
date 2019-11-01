// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.view.View;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

class BottomSheetUtils {
    /** Request {@code controller} to show {@code content} and expand the sheet when it is shown. */
    static void showContentAndExpand(BottomSheetController controller,
            AssistantBottomSheetContent content, boolean animate) {
        // Add an observer that makes sure the bottom sheet content is always shown, even in the
        // peek state.
        controller.addObserver(new EmptyBottomSheetObserver() {
            private boolean mEnabled;

            @Override
            public void onSheetContentChanged(BottomSheetContent newContent) {
                if (newContent == content) {
                    mEnabled = true;
                } else if (mEnabled) {
                    // Content was shown then hidden: remove this observer.
                    mEnabled = false;
                    controller.removeObserver(this);
                }
            }

            @Override
            public void onSheetClosed(int reason) {
                if (!mEnabled) return;

                // When scrolling with y < peekHeight, the BottomSheet will make the content
                // invisible. This is a workaround to prevent that as our toolbar is transparent and
                // we want to sheet content to stay visible.
                if ((controller.getSheetState() == BottomSheetController.SheetState.SCROLLING
                            || controller.getSheetState() == BottomSheetController.SheetState.PEEK)
                        && controller.getCurrentSheetContent() == content) {
                    controller.getBottomSheet()
                            .findViewById(R.id.bottom_sheet_content)
                            .setVisibility(View.VISIBLE);
                }
            }

            @Override
            public void onSheetStateChanged(int newState) {
                if (!mEnabled) return;

                if (newState == BottomSheetController.SheetState.PEEK
                        && controller.getCurrentSheetContent() == content) {
                    // When in the peek state, the BottomSheet hides the content view. We override
                    // that because we artificially increase the height of the transparent toolbar
                    // to show parts of the content view.
                    controller.getBottomSheet()
                            .findViewById(R.id.bottom_sheet_content)
                            .setVisibility(View.VISIBLE);
                }
            }
        });

        // Show the content.
        if (controller.requestShowContent(content, animate)) {
            controller.expandSheet();
        } else {
            // If the content is not directly shown, add an observer that will expand the sheet when
            // it is.
            controller.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent == content) {
                        controller.removeObserver(this);
                        controller.expandSheet();
                    }
                }
            });
        }
    }

    private BottomSheetUtils() {}
}
