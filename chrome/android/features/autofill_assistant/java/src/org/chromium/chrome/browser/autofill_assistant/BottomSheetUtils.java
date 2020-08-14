// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.task.PostTask;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.UiThreadTaskTraits;

class BottomSheetUtils {
    /** Request {@code controller} to show {@code content} and expand the sheet when it is shown. */
    static void showContentAndExpand(BottomSheetController controller,
            AssistantBottomSheetContent content, boolean animate) {
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
                        PostTask.postTask(
                                UiThreadTaskTraits.DEFAULT, () -> controller.expandSheet());
                    }
                }
            });
        }
    }

    private BottomSheetUtils() {}
}
