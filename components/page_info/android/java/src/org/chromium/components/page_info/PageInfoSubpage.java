// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.ui.widget.ChromeImageButton;

/**
 * Represents a particular page info subpage.
 */
public class PageInfoSubpage extends FrameLayout {
    /**  Parameters to configure the view of page info subpage. */
    public static class Params {
        // The URL to be shown at the top of the page.
        public CharSequence url;
        // The length of the URL's origin in number of characters.
        public int urlOriginLength;
        // The name of the subpage to be displayed.
        public String subpageTitle;
    }

    public PageInfoSubpage(Context context, Params params) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info_subpage, this, true);
        // Set the url title.
        PageInfoView.ElidedUrlTextView urlTitle = findViewById(R.id.subpage_url);
        urlTitle.setUrl(params.url, params.urlOriginLength);
        // Set the back button.
        // Set the page title.
        TextView subpageTitle = findViewById(R.id.subpage_title);
        subpageTitle.setText(params.subpageTitle);
    }

    public void setBackButtonOnClickListener(View.OnClickListener listener) {
        ChromeImageButton backButton = findViewById(R.id.subpage_back_button);
        backButton.setOnClickListener(listener);
    }
}
