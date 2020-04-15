// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Interface that provides embedder-level information to PageInfoController.
 */
public interface PageInfoControllerDelegate {
    // Returns the tracker.
    Tracker getTracker();

    // Create an AutoCompleteClassifier.
    AutocompleteSchemeClassifier createAutocompleteSchemeClassifier();

    // Whether cookie controls should be shown in Page Info UI.
    boolean cookieControlsShown();

    // Return the ModalDialogManager to be used.
    ModalDialogManager getModalDialogManager();

    // Whether Page Info UI should use dark colors.
    boolean useDarkColors();
}
