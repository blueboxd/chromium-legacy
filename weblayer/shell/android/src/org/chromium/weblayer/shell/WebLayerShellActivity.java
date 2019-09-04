// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.base.CommandLine;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;

import java.io.File;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerShellActivity extends FragmentActivity {
    private static final String TAG = "WebLayerShell";

    private Profile mProfile;
    private BrowserController mBrowserController;
    private EditText mUrlView;

    public static class ShellFragment extends Fragment {
        private BrowserController mBrowserController;

        ShellFragment(BrowserController browserController) {
            mBrowserController = browserController;
        }

        @Override
        public View onCreateView(
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            return mBrowserController.onCreateView();
        }
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // TODO: move this to WebLayer.
        // Initializing the command line must occur before loading the library.
        if (!CommandLine.isInitialized()) {
            ((WebLayerShellApplication) getApplication()).initCommandLine();
        }

        LinearLayout mainView = new LinearLayout(this);
        int viewId = View.generateViewId();
        mainView.setId(viewId);
        setContentView(mainView);

        mUrlView = new EditText(this);
        mUrlView.setSelectAllOnFocus(true);
        mUrlView.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        mUrlView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO)
                        && (event == null || event.getKeyCode() != KeyEvent.KEYCODE_ENTER
                                || event.getAction() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }
                loadUrl(mUrlView.getText().toString());
                return true;
            }
        });

        mProfile = WebLayer.getInstance().createProfile(new File(""));
        mBrowserController = mProfile.createBrowserController(this);

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.add(viewId, new ShellFragment(mBrowserController));
        transaction.commit();

        mBrowserController.setTopView(mUrlView);
        loadUrl("http://google.com");
        mBrowserController.addObserver(new BrowserObserver() {
            @Override
            public void displayURLChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        });
    }

    @Override
    protected void onDestroy() {
        if (mProfile != null) mProfile.destroy();
        if (mBrowserController != null) mBrowserController.destroy();
        super.onDestroy();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    private void loadUrl(String url) {
        mBrowserController.getNavigationController().navigate(Uri.parse(url));
    }
}
