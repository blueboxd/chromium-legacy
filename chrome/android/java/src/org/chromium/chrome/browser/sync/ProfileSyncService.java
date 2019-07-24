// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.annotation.Nullable;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.Passphrase;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * JNI wrapper for the native ProfileSyncService.
 *
 * This class purely makes calls to native and contains absolutely  no business logic. It is only
 * usable from the UI thread as the native ProfileSyncService requires its access to be on the
 * UI thread. See chrome/browser/sync/profile_sync_service.h for more details.
 */
public class ProfileSyncService {

    /**
     * Listener for the underlying sync status.
     */
    public interface SyncStateChangedListener {
        // Invoked when the status has changed.
        public void syncStateChanged();
    }

    /**
     * Callback for getAllNodes.
     */
    public static class GetAllNodesCallback {
        private String mNodesString;

        // Invoked when getAllNodes completes.
        public void onResult(String nodesString) {
            mNodesString = nodesString;
        }

        // Returns the result of GetAllNodes as a JSONArray.
        @VisibleForTesting
        public JSONArray getNodesAsJsonArray() throws JSONException {
            return new JSONArray(mNodesString);
        }
    }

    /**
     * Provider for the Android master sync flag.
     */
    interface MasterSyncEnabledProvider {
        // Returns whether master sync is enabled.
        public boolean isMasterSyncEnabled();
    }

    private static final String TAG = "ProfileSyncService";

    private static final int[] ALL_SELECTABLE_TYPES = new int[] {
        ModelType.AUTOFILL,
        ModelType.BOOKMARKS,
        ModelType.PASSWORDS,
        ModelType.PREFERENCES,
        ModelType.PROXY_TABS,
        ModelType.TYPED_URLS
    };

    private static ProfileSyncService sProfileSyncService;
    private static boolean sInitialized;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    /**
     * Native ProfileSyncServiceAndroid object. Cannot be final because it is initialized in
     * {@link init()}.
     */
    private long mNativeProfileSyncServiceAndroid;

    /**
     * An object that knows whether Android's master sync setting is enabled.
     */
    private MasterSyncEnabledProvider mMasterSyncEnabledProvider;

    private int mSetupInProgressCounter;

    /**
     * Retrieves or creates the ProfileSyncService singleton instance. Returns null if sync is
     * disabled (via flag or variation).
     *
     * Can only be accessed on the main thread.
     */
    @Nullable
    public static ProfileSyncService get() {
        ThreadUtils.assertOnUiThread();
        if (!sInitialized) {
            sProfileSyncService = new ProfileSyncService();
            if (sProfileSyncService.mNativeProfileSyncServiceAndroid == 0) {
                sProfileSyncService = null;
            }
            sInitialized = true;
        }
        return sProfileSyncService;
    }

    /**
     * Overrides the initialization for tests. The tests should call resetForTests() at shutdown.
     */
    @VisibleForTesting
    public static void overrideForTests(ProfileSyncService profileSyncService) {
        sProfileSyncService = profileSyncService;
        sInitialized = true;
    }

    /**
     * Resets the ProfileSyncService instance. Calling get() next time will initialize with a new
     * instance.
     */
    @VisibleForTesting
    public static void resetForTests() {
        sInitialized = false;
        sProfileSyncService = null;
    }

    protected ProfileSyncService() {
        init();
    }

    /**
     * This is called pretty early in our application. Avoid any blocking operations here. init()
     * is a separate function to enable a test subclass of ProfileSyncService to completely stub out
     * ProfileSyncService.
     */
    protected void init() {
        ThreadUtils.assertOnUiThread();

        // This may cause us to create ProfileSyncService even if sync has not
        // been set up, but ProfileSyncService::Startup() won't be called until
        // credentials are available.
        mNativeProfileSyncServiceAndroid = nativeInit();
    }

    @CalledByNative
    private static long getProfileSyncServiceAndroid() {
        return get().mNativeProfileSyncServiceAndroid;
    }

    /**
     * Sets the the machine tag used by session sync.
     */
    public void setSessionsId(String sessionTag) {
        ThreadUtils.assertOnUiThread();
        nativeSetSyncSessionsId(mNativeProfileSyncServiceAndroid, sessionTag);
    }

    /**
     * Returns the actual passphrase type being used for encryption. The sync engine must be
     * running (isEngineInitialized() returns true) before calling this function.
     * <p/>
     * This method should only be used if you want to know the raw value. For checking whether
     * we should ask the user for a passphrase, use isPassphraseRequiredForDecryption().
     */
    public @Passphrase.Type int getPassphraseType() {
        assert isEngineInitialized();
        int passphraseType = nativeGetPassphraseType(mNativeProfileSyncServiceAndroid);
        if (passphraseType < 0 || passphraseType >= Passphrase.Type.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        return passphraseType;
    }

    /**
     * Returns true if the current explicit passphrase time is defined.
     */
    public boolean hasExplicitPassphraseTime() {
        assert isEngineInitialized();
        return nativeHasExplicitPassphraseTime(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Returns the current explicit passphrase time in milliseconds since epoch.
     */
    public long getExplicitPassphraseTime() {
        assert isEngineInitialized();
        return nativeGetExplicitPassphraseTime(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterGooglePassphraseBodyWithDateText() {
        assert isEngineInitialized();
        return nativeGetSyncEnterGooglePassphraseBodyWithDateText(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterCustomPassphraseBodyWithDateText() {
        assert isEngineInitialized();
        return nativeGetSyncEnterCustomPassphraseBodyWithDateText(mNativeProfileSyncServiceAndroid);
    }

    public String getCurrentSignedInAccountText() {
        assert isEngineInitialized();
        return nativeGetCurrentSignedInAccountText(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterCustomPassphraseBodyText() {
        return nativeGetSyncEnterCustomPassphraseBodyText(mNativeProfileSyncServiceAndroid);
    }

    public int getNumberOfSyncedDevices() {
        return nativeGetNumberOfSyncedDevices(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if sync is currently set to use a custom passphrase. The sync engine must be running
     * (isEngineInitialized() returns true) before calling this function.
     *
     * @return true if sync is using a custom passphrase.
     */
    public boolean isUsingSecondaryPassphrase() {
        assert isEngineInitialized();
        return nativeIsUsingSecondaryPassphrase(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if we need a passphrase to decrypt a currently-enabled data type. This returns false
     * if a passphrase is needed for a type that is not currently enabled.
     *
     * @return true if we need a passphrase.
     */
    public boolean isPassphraseRequiredForDecryption() {
        assert isEngineInitialized();
        return nativeIsPassphraseRequiredForDecryption(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if the sync engine is running.
     *
     * @return true if sync is initialized/running.
     */
    public boolean isEngineInitialized() {
        return nativeIsEngineInitialized(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if encrypting all the data types is allowed.
     *
     * @return true if encrypting all data types is allowed, false if only passwords are allowed to
     * be encrypted.
     */
    public boolean isEncryptEverythingAllowed() {
        assert isEngineInitialized();
        return nativeIsEncryptEverythingAllowed(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if the user has chosen to encrypt all data types. Note that some data types (e.g.
     * DEVICE_INFO) are never encrypted.
     *
     * @return true if all data types are encrypted, false if only passwords are encrypted.
     */
    public boolean isEncryptEverythingEnabled() {
        assert isEngineInitialized();
        return nativeIsEncryptEverythingEnabled(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Turns on encryption of all data types. This only takes effect after sync configuration is
     * completed and setChosenDataTypes() is invoked.
     */
    public void enableEncryptEverything() {
        assert isEngineInitialized();
        nativeEnableEncryptEverything(mNativeProfileSyncServiceAndroid);
    }

    public void setEncryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        nativeSetEncryptionPassphrase(mNativeProfileSyncServiceAndroid, passphrase);
    }

    public boolean setDecryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        return nativeSetDecryptionPassphrase(mNativeProfileSyncServiceAndroid, passphrase);
    }

    public @GoogleServiceAuthError.State int getAuthError() {
        int authErrorCode = nativeGetAuthError(mNativeProfileSyncServiceAndroid);
        if (authErrorCode < 0 || authErrorCode >= GoogleServiceAuthError.State.NUM_ENTRIES) {
            throw new IllegalArgumentException("No state for code: " + authErrorCode);
        }
        return authErrorCode;
    }

    public boolean requiresClientUpgrade() {
        return nativeRequiresClientUpgrade(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Gets the set of data types that are currently syncing.
     *
     * This is affected by whether sync is on.
     *
     * @return Set of active data types.
     */
    public Set<Integer> getActiveDataTypes() {
        int[] activeDataTypes = nativeGetActiveDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(activeDataTypes);
    }

    /**
     * Gets the set of data types that are enabled in sync. This will always
     * return a subset of syncer::UserSelectableTypes().
     *
     * This is unaffected by whether sync is on.
     *
     * @return Set of chosen types.
     */
    public Set<Integer> getChosenDataTypes() {
        int[] modelTypeArray = nativeGetChosenDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(modelTypeArray);
    }

    /**
     * Gets the set of data types that are "preferred" in sync. Those are the
     * "chosen" ones (see above), plus any that are implied by them.
     *
     * This is unaffected by whether sync is on.
     *
     * @return Set of preferred types.
     */
    public Set<Integer> getPreferredDataTypes() {
        int[] modelTypeArray = nativeGetPreferredDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(modelTypeArray);
    }

    private static Set<Integer> modelTypeArrayToSet(int[] modelTypeArray) {
        Set<Integer> modelTypeSet = new HashSet<Integer>();
        for (int i = 0; i < modelTypeArray.length; i++) {
            modelTypeSet.add(modelTypeArray[i]);
        }
        return modelTypeSet;
    }

    private static int[] modelTypeSetToArray(Set<Integer> modelTypeSet) {
        int[] modelTypeArray = new int[modelTypeSet.size()];
        int i = 0;
        for (int modelType : modelTypeSet) {
            modelTypeArray[i++] = modelType;
        }
        return modelTypeArray;
    }

    public boolean hasKeepEverythingSynced() {
        return nativeHasKeepEverythingSynced(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Enables syncing for the passed data types.
     *
     * @param syncEverything Set to true if the user wants to sync all data types
     *                       (including new data types we add in the future).
     * @param enabledTypes   The set of types to enable. Ignored (can be null) if
     *                       syncEverything is true.
     */
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        nativeSetChosenDataTypes(mNativeProfileSyncServiceAndroid, syncEverything,
                syncEverything ? ALL_SELECTABLE_TYPES : modelTypeSetToArray(enabledTypes));
    }

    public void triggerRefresh() {
        nativeTriggerRefresh(mNativeProfileSyncServiceAndroid);
    }

    public void setFirstSetupComplete() {
        nativeSetFirstSetupComplete(mNativeProfileSyncServiceAndroid);
    }

    public boolean isFirstSetupComplete() {
        return nativeIsFirstSetupComplete(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether syncing is requested by the user, i.e. the user has at least started a Sync
     * setup flow, and has not disabled syncing in settings. Note that even if this is true, other
     * reasons might prevent Sync from actually starting up.
     *
     * @return true if the user wants to sync, false otherwise.
     */
    public boolean isSyncRequested() {
        return nativeIsSyncRequested(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether Sync-the-feature can (attempt to) start. This means that there is a primary
     * account and no disable reasons. Note that the Sync machinery may start up in transport-only
     * mode even if this is false.
     *
     * @return true if Sync can start, false otherwise.
     */
    public boolean canSyncFeatureStart() {
        return nativeCanSyncFeatureStart(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether Sync-the-feature is currently active. Note that Sync-the-transport may be
     * active even if this is false.
     *
     * @return true if Sync is active, false otherwise.
     */
    public boolean isSyncActive() {
        return nativeIsSyncActive(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether Sync is disabled by enterprise policy (through prefs) or account policy
     * received from the sync server.
     *
     * @return true if Sync is disabled, false otherwise.
     */
    public boolean isSyncDisabledByEnterprisePolicy() {
        return nativeIsSyncDisabledByEnterprisePolicy(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Instances of this class keep sync paused until {@link #close} is called. Use
     * {@link ProfileSyncService#getSetupInProgressHandle} to create. Please note that
     * {@link #close} should be called on every instance of this class.
     */
    public final class SyncSetupInProgressHandle {
        private boolean mClosed;

        private SyncSetupInProgressHandle() {
            ThreadUtils.assertOnUiThread();
            if (++mSetupInProgressCounter == 1) {
                setSetupInProgress(true);
            }
        }

        public void close() {
            ThreadUtils.assertOnUiThread();
            if (mClosed) return;
            mClosed = true;

            assert mSetupInProgressCounter > 0;
            if (--mSetupInProgressCounter == 0) {
                setSetupInProgress(false);
                // The user has finished setting up sync at least once.
                setFirstSetupComplete();
            }
        }
    }

    /**
     * Called by the UI to prevent changes in sync settings from taking effect while these settings
     * are being modified by the user. When sync settings UI is no longer visible,
     * {@link SyncSetupInProgressHandle#close} has to be invoked for sync settings to be applied.
     * Sync settings will remain paused as long as there are unclosed objects returned by this
     * method. Please note that the behavior of SyncSetupInProgressHandle is slightly different from
     * the equivalent C++ object, as Java instances don't commit sync settings as soon as any
     * instance of SyncSetupInProgressHandle is closed.
     */
    public SyncSetupInProgressHandle getSetupInProgressHandle() {
        return new SyncSetupInProgressHandle();
    }

    private void setSetupInProgress(boolean inProgress) {
        nativeSetSetupInProgress(mNativeProfileSyncServiceAndroid, inProgress);
    }

    public void addSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.add(listener);
    }

    public void removeSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.remove(listener);
    }

    public boolean hasUnrecoverableError() {
        return nativeHasUnrecoverableError(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Called when the state of the native sync engine has changed, so various
     * UI elements can update themselves.
     */
    @CalledByNative
    public void syncStateChanged() {
        for (SyncStateChangedListener listener : mListeners) {
            listener.syncStateChanged();
        }
    }

    /**
     * Starts the sync engine.
     */
    public void requestStart() {
        nativeRequestStart(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Stops the sync engine.
     */
    public void requestStop() {
        nativeRequestStop(mNativeProfileSyncServiceAndroid);
    }

    public void setSyncAllowedByPlatform(boolean allowed) {
        nativeSetSyncAllowedByPlatform(mNativeProfileSyncServiceAndroid, allowed);
    }

    /**
     * Flushes the sync directory.
     */
    public void flushDirectory() {
        nativeFlushDirectory(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Returns the time when the last sync cycle was completed.
     *
     * @return The difference measured in microseconds, between last sync cycle completion time
     * and 1 January 1970 00:00:00 UTC.
     */
    @VisibleForTesting
    public long getLastSyncedTimeForTest() {
        return nativeGetLastSyncedTimeForTest(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Overrides the Sync engine's NetworkResources. This is used to set up the Sync FakeServer for
     * testing.
     *
     * @param networkResources the pointer to the NetworkResources created by the native code. It
     *                         is assumed that the Java caller has ownership of this pointer;
     *                         ownership is transferred as part of this call.
     */
    @VisibleForTesting
    public void overrideNetworkResourcesForTest(long networkResources) {
        nativeOverrideNetworkResourcesForTest(mNativeProfileSyncServiceAndroid, networkResources);
    }

    /**
     * Returns whether this client has previously prompted the user for a
     * passphrase error via the android system notifications.
     *
     * Can be called whether or not sync is initialized.
     *
     * @return Whether client has prompted for a passphrase error previously.
     */
    public boolean isPassphrasePrompted() {
        return nativeIsPassphrasePrompted(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Sets whether this client has previously prompted the user for a
     * passphrase error via the android system notifications.
     *
     * Can be called whether or not sync is initialized.
     *
     * @param prompted whether the client has prompted the user previously.
     */
    public void setPassphrasePrompted(boolean prompted) {
        nativeSetPassphrasePrompted(mNativeProfileSyncServiceAndroid,
                                    prompted);
    }

    /**
     * Set the MasterSyncEnabledProvider for ProfileSyncService.
     *
     * This method is intentionally package-scope and should only be called once.
     */
    void setMasterSyncEnabledProvider(MasterSyncEnabledProvider masterSyncEnabledProvider) {
        ThreadUtils.assertOnUiThread();
        assert mMasterSyncEnabledProvider == null;
        mMasterSyncEnabledProvider = masterSyncEnabledProvider;
    }

    /**
     * Returns whether Android's master sync setting is enabled.
     */
    @CalledByNative
    public boolean isMasterSyncEnabled() {
        ThreadUtils.assertOnUiThread();
        // TODO(maxbogue): ensure that this method is never called before
        // setMasterSyncEnabledProvider() and change the line below to an assert.
        // See http://crbug.com/570569
        if (mMasterSyncEnabledProvider == null) return true;
        return mMasterSyncEnabledProvider.isMasterSyncEnabled();
    }

    /**
     * Invokes the onResult method of the callback from native code.
     */
    @CalledByNative
    private static void onGetAllNodesResult(GetAllNodesCallback callback, String nodes) {
        callback.onResult(nodes);
    }

    /**
     * Retrieves a JSON version of local Sync data via the native GetAllNodes method.
     * This method is asynchronous; the result will be sent to the callback.
     */
    @VisibleForTesting
    public void getAllNodes(GetAllNodesCallback callback) {
        nativeGetAllNodes(mNativeProfileSyncServiceAndroid, callback);
    }

    // Native methods
    private native long nativeInit();
    private native void nativeRequestStart(long nativeProfileSyncServiceAndroid);
    private native void nativeRequestStop(long nativeProfileSyncServiceAndroid);
    private native void nativeSetSyncAllowedByPlatform(
            long nativeProfileSyncServiceAndroid, boolean allowed);
    private native void nativeFlushDirectory(long nativeProfileSyncServiceAndroid);
    private native void nativeSetSyncSessionsId(long nativeProfileSyncServiceAndroid, String tag);
    private native int nativeGetAuthError(long nativeProfileSyncServiceAndroid);
    private native boolean nativeRequiresClientUpgrade(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsEngineInitialized(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsEncryptEverythingAllowed(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsEncryptEverythingEnabled(long nativeProfileSyncServiceAndroid);
    private native void nativeEnableEncryptEverything(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPassphraseRequiredForDecryption(
            long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsUsingSecondaryPassphrase(long nativeProfileSyncServiceAndroid);

    private native boolean nativeSetDecryptionPassphrase(
            long nativeProfileSyncServiceAndroid, String passphrase);
    private native void nativeSetEncryptionPassphrase(
            long nativeProfileSyncServiceAndroid, String passphrase);
    private native int nativeGetPassphraseType(long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasExplicitPassphraseTime(long nativeProfileSyncServiceAndroid);
    private native long nativeGetExplicitPassphraseTime(long nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterGooglePassphraseBodyWithDateText(
            long nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterCustomPassphraseBodyWithDateText(
            long nativeProfileSyncServiceAndroid);
    private native String nativeGetCurrentSignedInAccountText(long nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterCustomPassphraseBodyText(
            long nativeProfileSyncServiceAndroid);
    private native int nativeGetNumberOfSyncedDevices(long nativeProfileSyncServiceAndroid);
    private native int[] nativeGetActiveDataTypes(long nativeProfileSyncServiceAndroid);
    private native int[] nativeGetChosenDataTypes(long nativeProfileSyncServiceAndroid);
    private native int[] nativeGetPreferredDataTypes(long nativeProfileSyncServiceAndroid);
    private native void nativeSetChosenDataTypes(
            long nativeProfileSyncServiceAndroid, boolean syncEverything, int[] modelTypeArray);
    private native void nativeTriggerRefresh(long nativeProfileSyncServiceAndroid);
    private native void nativeSetSetupInProgress(
            long nativeProfileSyncServiceAndroid, boolean inProgress);
    private native void nativeSetFirstSetupComplete(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsFirstSetupComplete(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncRequested(long nativeProfileSyncServiceAndroid);
    private native boolean nativeCanSyncFeatureStart(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncActive(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncDisabledByEnterprisePolicy(
            long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasKeepEverythingSynced(long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasUnrecoverableError(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPassphrasePrompted(long nativeProfileSyncServiceAndroid);
    private native void nativeSetPassphrasePrompted(long nativeProfileSyncServiceAndroid,
                                                    boolean prompted);
    private native long nativeGetLastSyncedTimeForTest(long nativeProfileSyncServiceAndroid);
    private native void nativeOverrideNetworkResourcesForTest(
            long nativeProfileSyncServiceAndroid, long networkResources);
    private native void nativeGetAllNodes(
            long nativeProfileSyncServiceAndroid, GetAllNodesCallback callback);
}
