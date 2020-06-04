// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteMediator.DropdownItemViewInfo;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Utility methods providing access to package-private methods in {@link AutocompleteCoordinator}
 * for tests.
 */
public class AutocompleteCoordinatorTestUtils {
    /**
     * Sets the autocomplete controller for the location bar.
     *
     * @param controller The controller that will handle autocomplete/omnibox suggestions.
     * @note Only used for testing.
     */
    public static void setAutocompleteController(
            AutocompleteCoordinator coordinator, AutocompleteController controller) {
        ((AutocompleteCoordinatorImpl) coordinator).setAutocompleteController(controller);
    }

    /** Allows injecting autocomplete suggestions for testing. */
    public static AutocompleteController.OnSuggestionsReceivedListener
    getSuggestionsReceivedListenerForTest(AutocompleteCoordinator coordinator) {
        return ((AutocompleteCoordinatorImpl) coordinator).getSuggestionsReceivedListenerForTest();
    }

    /**
     * @return The suggestion dropdown containing the omnibox results (or null if it has not yet
     *         been created).
     */
    public static OmniboxSuggestionsDropdown getSuggestionsDropdown(
            AutocompleteCoordinator coordinator) {
        return ((AutocompleteCoordinatorImpl) coordinator).getSuggestionsDropdown();
    }

    /**
     * @return The {@link OmniboxSuggestion} at the specified index.
     */
    public static OmniboxSuggestion getOmniboxSuggestionAt(
            AutocompleteCoordinator coordinator, int index) {
        return coordinator.getSuggestionAt(index);
    }

    /**
     * @return The index of the first suggestion which is |type|.
     */
    public static int getIndexForFirstSuggestionOfType(
            AutocompleteCoordinator coordinator, @OmniboxSuggestionUiType int type) {
        ModelList currentModels =
                ((AutocompleteCoordinatorImpl) coordinator).getSuggestionModelList();
        for (int i = 0; i < currentModels.size(); i++) {
            DropdownItemViewInfo info = (DropdownItemViewInfo) currentModels.get(i);
            if (info.type == type) {
                return i;
            }
        }
        return -1;
    }
}
