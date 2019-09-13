// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.res.Resources;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.TextView;

import androidx.annotation.LayoutRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.download.R;

/** An adapter that allows selecting an item from a dropdown spinner. */
class FilterAdapter extends BaseAdapter implements AdapterView.OnItemSelectedListener {
    private final int mIconColor;
    private DownloadManagerUi mManagerUi;

    /**
     * @param resources The {@link Resources} used to retrieve resources.
     */
    FilterAdapter(Resources resources) {
        super();
        mIconColor = ApiCompatibilityUtils.getColor(
                resources, org.chromium.chrome.R.color.default_icon_color);
    }

    @Override
    public int getCount() {
        return DownloadFilter.getFilterCount();
    }

    @Override
    public Object getItem(int position) {
        return DownloadFilter.FILTER_LIST[position];
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getDropDownView(int position, View convertView, ViewGroup parent) {
        TextView labelView =
                getTextViewFromResource(convertView, R.layout.download_manager_spinner_drop_down);
        labelView.setText(DownloadFilter.getStringIdForFilter(position));
        int iconId = DownloadFilter.getDrawableForFilter(position);
        VectorDrawableCompat iconDrawable =
                VectorDrawableCompat.create(mManagerUi.getActivity().getResources(), iconId,
                        mManagerUi.getActivity().getTheme());
        DrawableCompat.setTint(iconDrawable, mIconColor);
        labelView.setCompoundDrawablesWithIntrinsicBounds(iconDrawable, null, null, null);

        return labelView;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TextView labelView =
                getTextViewFromResource(convertView, R.layout.download_manager_spinner);

        CharSequence title = mManagerUi.getActivity().getResources().getText(position == 0
                        ? R.string.menu_downloads
                        : DownloadFilter.getStringIdForFilter(position));
        labelView.setText(title);

        return labelView;
    }

    private TextView getTextViewFromResource(View convertView, @LayoutRes int resId) {
        TextView labelView = null;
        if (convertView instanceof TextView) {
            labelView = (TextView) convertView;
        } else {
            labelView =
                    (TextView) LayoutInflater.from(mManagerUi.getActivity()).inflate(resId, null);
        }

        return labelView;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        mManagerUi.onFilterChanged(position);
    }

    public void initialize(DownloadManagerUi manager) {
        mManagerUi = manager;
    }

    /** Called when this object should be destroyed. */
    public void destroy() {
        mManagerUi = null;
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}
}
