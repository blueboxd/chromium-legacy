// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.nfc.NfcAdapter;
import android.os.Bundle;
import android.os.Process;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.app.Fragment;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;

/**
 * The Chrome settings activity.
 *
 * This activity displays a single Fragment, typically a PreferenceFragment. As the user navigates
 * through settings, a separate Preferences activity is created for each screen. Thus each fragment
 * may freely modify its activity's action bar or title. This mimics the behavior of
 * android.preference.PreferenceActivity.
 *
 * If the preference overrides the root layout (e.g. {@link HomepageEditor}), add the following:
 * 1) preferences_action_bar_shadow.xml to the custom XML hierarchy and
 * 2) an OnScrollChangedListener to the main content's view's view tree observer via
 *    PreferenceUtils.getShowShadowOnScrollListener(...).
 */
public class Preferences extends ChromeBaseAppCompatActivity
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
    /**
     * Preference fragments may implement this interface to intercept "Back" button taps in this
     * activity.
     */
    public interface OnBackPressedListener {
        /**
         * Called when the user taps "Back".
         * @return Whether "Back" button was handled by the fragment. If this method returns false,
         *         the activity should handle the event itself.
         */
        boolean onBackPressed();
    }

    static final String EXTRA_SHOW_FRAGMENT = "show_fragment";
    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";

    private static final String TAG = "Preferences";

    /** The current instance of Preferences in the resumed state, if any. */
    private static Preferences sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ensureActivityNotExported();

        // The browser process must be started here because this Activity may be started explicitly
        // from Android notifications, when Android is restoring Preferences after Chrome was
        // killed, or for tests. This should happen before super.onCreate() because it might
        // recreate a fragment, and a fragment might depend on the native library.
        try {
            ChromeBrowserInitializer.getInstance(this).handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            // This can only ever happen, if at all, when the activity is started from an Android
            // notification (or in tests). As such we don't want to show an error messsage to the
            // user. The application is completely broken at this point, so close it down
            // completely (not just the activity).
            System.exit(-1);
            return;
        }

        super.onCreate(savedInstanceState);

        mIsNewlyCreated = savedInstanceState == null;

        String initialFragment = getIntent().getStringExtra(EXTRA_SHOW_FRAGMENT);
        Bundle initialArguments = getIntent().getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        getSupportActionBar().setElevation(0);

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            if (initialFragment == null) initialFragment = MainPreferences.class.getName();

            Fragment fragment = Fragment.instantiate(this, initialFragment, initialArguments);
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(android.R.id.content, fragment)
                    .commit();
        }

        if (ApiCompatibilityUtils.checkPermission(
                this, Manifest.permission.NFC, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED) {
            // Disable Android Beam on JB and later devices.
            // In ICS it does nothing - i.e. we will send a Play Store link if NFC is used.
            NfcAdapter nfcAdapter = NfcAdapter.getDefaultAdapter(this);
            if (nfcAdapter != null) nfcAdapter.setNdefPushMessage(null, this);
        }

        Resources res = getResources();
        ApiCompatibilityUtils.setTaskDescription(this, res.getString(R.string.app_name),
                BitmapFactory.decodeResource(res, R.mipmap.app_icon),
                ApiCompatibilityUtils.getColor(res, R.color.default_primary_color));
    }

    // OnPreferenceStartFragmentCallback:

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        startFragment(preference.getFragment(), preference.getExtras());
        return true;
    }

    /**
     * Starts a new Preferences activity showing the desired fragment.
     *
     * @param fragmentClass The Class of the fragment to show.
     * @param args Arguments to pass to Fragment.instantiate(), or null.
     */
    public void startFragment(String fragmentClass, Bundle args) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClass(this, getClass());
        intent.putExtra(EXTRA_SHOW_FRAGMENT, fragmentClass);
        intent.putExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS, args);
        startActivity(intent);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        Fragment fragment = getMainFragmentCompat();
        if (fragment == null || fragment.getView() == null
                || fragment.getView().findViewById(R.id.list) == null) {
            return;
        }
        View contentView = fragment.getActivity().findViewById(android.R.id.content);
        if (contentView == null || !(contentView instanceof FrameLayout)) {
            return;
        }
        View inflatedView = View.inflate(getApplicationContext(),
                R.layout.preferences_action_bar_shadow, (ViewGroup) contentView);
        RecyclerView recyclerView = fragment.getView().findViewById(R.id.list);
        recyclerView.getViewTreeObserver().addOnScrollChangedListener(
                PreferenceUtils.getShowShadowOnScrollListener(
                        recyclerView, inflatedView.findViewById(R.id.shadow)));
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of Preferences at the same time
        // (e.g. in multi-instance mode on a Samsung device), which would cause many fun bugs.
        if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()
                && !mIsNewlyCreated) {
            // This activity was unpaused or recreated while another instance of Preferences was
            // already showing. The existing instance takes precedence.
            finish();
        } else {
            // This activity was newly created and takes precedence over sResumedInstance.
            if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()) {
                sResumedInstance.finish();
            }

            sResumedInstance = this;
            mIsNewlyCreated = false;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        ProfileManagerUtils.flushPersistentDataForAllProfiles();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (sResumedInstance == this) sResumedInstance = null;
    }

    /**
     * Returns the fragment showing as this activity's main content, typically a {@link
     * PreferenceFragmentCompat}. This does not include dialogs or other {@link Fragment}s shown on
     * top of the main content.
     */
    @VisibleForTesting
    public Fragment getMainFragmentCompat() {
        return getSupportFragmentManager().findFragmentById(android.R.id.content);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        // By default, every screen in Settings shows a "Help & feedback" menu item.
        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_general_help, Menu.CATEGORY_SECONDARY, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getTheme()));
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        if (menu.size() == 1) {
            MenuItem item = menu.getItem(0);
            if (item.getIcon() != null) item.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Fragment mainFragmentCompat = getMainFragmentCompat();
        if (mainFragmentCompat != null && mainFragmentCompat.onOptionsItemSelected(item)) {
            return true;
        }

        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            HelpAndFeedback.getInstance(this).show(this, getString(R.string.help_context_settings),
                    Profile.getLastUsedProfile(), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed() {
        Fragment activeFragment = getMainFragmentCompat();
        if (!(activeFragment instanceof OnBackPressedListener)) {
            super.onBackPressed();
            return;
        }
        OnBackPressedListener listener = (OnBackPressedListener) activeFragment;
        if (!listener.onBackPressed()) {
            // Fragment hasn't handled this event, fall back to AppCompatActivity handling.
            super.onBackPressed();
        }
    }

    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If Preferences is exported, then it's vulnerable to a fragment injection exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("Preferences must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }
}
