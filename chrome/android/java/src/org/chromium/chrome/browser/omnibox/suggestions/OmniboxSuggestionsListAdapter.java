// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.os.Debug;
import android.view.View;

import org.chromium.ui.modelutil.ModelListAdapter;

/**
 * ModelListAdapter for SuggestionList.
 */
class OmniboxSuggestionsListAdapter extends ModelListAdapter {
    OmniboxSuggestionsListAdapter(ModelList data) {
        super(data);
    }

    @Override
    protected boolean canReuseView(View view, int desiredType) {
        boolean reused = super.canReuseView(view, desiredType);
        SuggestionsMetrics.recordSuggestionViewReused(reused);
        return reused;
    }

    @Override
    protected View createView(int typeId) {
        final long start = Debug.threadCpuTimeNanos();
        View v = super.createView(typeId);
        final long end = Debug.threadCpuTimeNanos();
        SuggestionsMetrics.recordSuggestionViewCreateTime(start, end);
        return v;
    }
}
