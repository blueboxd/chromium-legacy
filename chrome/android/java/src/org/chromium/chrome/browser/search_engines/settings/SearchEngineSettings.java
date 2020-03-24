// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.ListFragment;

import org.chromium.chrome.R;

/**
 * A preference fragment for selecting a default search engine.
 * ATTENTION: User can't change search engine if it is controlled by an enterprise policy. Check
 * TemplateUrlServiceFactory.get().isDefaultSearchManaged() before launching this fragment.
 *
 * TODO(crbug.com/988877): Add on scroll shadow to action bar.
 */
public class SearchEngineSettings extends ListFragment {
    private SearchEngineAdapter mSearchEngineAdapter;

    @VisibleForTesting
    String getValueForTesting() {
        return mSearchEngineAdapter.getValueForTesting();
    }

    @VisibleForTesting
    String setValueForTesting(String value) {
        return mSearchEngineAdapter.setValueForTesting(value);
    }

    @VisibleForTesting
    String getKeywordFromIndexForTesting(int index) {
        return mSearchEngineAdapter.getKeywordForTesting(index);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.search_engine_settings);
        mSearchEngineAdapter = new SearchEngineAdapter(getActivity());
        setListAdapter(mSearchEngineAdapter);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ListView listView = getListView();
        listView.setDivider(null);
        listView.setItemsCanFocus(true);
    }

    @Override
    public void onStart() {
        super.onStart();
        mSearchEngineAdapter.start();
    }

    @Override
    public void onStop() {
        super.onStop();
        mSearchEngineAdapter.stop();
    }
}
