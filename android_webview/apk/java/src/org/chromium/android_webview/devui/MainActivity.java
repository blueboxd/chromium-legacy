// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.android_webview.devui.util.NavigationMenuHelper;
import org.chromium.ui.widget.Toast;

import java.util.Locale;

/**
 * Dev UI main activity.
 * It shows a summary about WebView package and the device.
 * It helps to navigate to other WebView developer tools.
 */
public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        PackageInfo webViewPackage = getPackageInfo();
        InfoItem[] infoItems = new InfoItem[] {
                new InfoItem("WebView package",
                        webViewPackage == null ? null : webViewPackage.packageName),
                new InfoItem("WebView version",
                        String.format(Locale.US, "%s (%s)",
                                webViewPackage == null ? null : webViewPackage.versionName,
                                webViewPackage.versionCode)),
                new InfoItem("Device info",
                        String.format(Locale.US, "%s - %s", Build.MODEL, Build.FINGERPRINT)),
        };

        ListView infoListView = findViewById(R.id.main_info_list);
        ArrayAdapter<InfoItem> itemsArrayAdapter = new InfoListAdapter(infoItems);
        infoListView.setAdapter(itemsArrayAdapter);

        // Copy item's text to clipboard on tapping a list item.
        infoListView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                InfoItem item = infoItems[position];
                ClipboardManager clipboard =
                        (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newPlainText(item.title, item.subtitle);
                clipboard.setPrimaryClip(clip);
                // Show a toast that the text has been copied.
                Toast.makeText(MainActivity.this, "Copied " + item.title, Toast.LENGTH_SHORT)
                        .show();
            }
        });
    }

    /**
     * A model class for a key-value piece of information to be displayed as a title (key) and
     * subtitle (value).
     */
    private static class InfoItem {
        public static final String UNKNOWN = "Unknown";
        public final String title;
        public final String subtitle;

        public InfoItem(String title, String subtitle) {
            this.title = title;
            this.subtitle = subtitle == null ? UNKNOWN : subtitle;
        }
    }

    /**
     * An ArrayAdapter to show a list of {@code InfoItem} objects.
     *
     * It uses android stock {@code android.R.layout.simple_list_item_2} which has two {@code
     * TextView}; {@code text1} acts as the item title and {@code text2} as the item subtitle.
     */
    private class InfoListAdapter extends ArrayAdapter<InfoItem> {
        private final InfoItem[] mItems;

        public InfoListAdapter(InfoItem[] items) {
            super(MainActivity.this, android.R.layout.simple_list_item_2, items);
            mItems = items;
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(android.R.layout.simple_list_item_2, null, true);
            }

            InfoItem item = mItems[position];
            TextView title = view.findViewById(android.R.id.text1);
            TextView subtitle = view.findViewById(android.R.id.text2);

            title.setText(item.title);
            subtitle.setText(item.subtitle);

            return view;
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        NavigationMenuHelper.inflate(this, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (NavigationMenuHelper.onOptionsItemSelected(this, item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private PackageInfo getPackageInfo() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0);
        } catch (NameNotFoundException e) {
            return null;
        }
    }
}
