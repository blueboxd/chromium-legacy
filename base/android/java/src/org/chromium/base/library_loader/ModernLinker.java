// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JniIgnoreNatives;

import java.util.HashMap;
import java.util.Locale;

import javax.annotation.concurrent.GuardedBy;

/**
 * Provides a concrete implementation of the Chromium Linker.
 *
 * This Linker implementation uses the Android M and later system linker to map Chrome and call
 * |JNI_OnLoad()|.
 *
 * For more on the operations performed by the Linker, see {@link Linker}.
 */
@JniIgnoreNatives
class ModernLinker extends Linker {
    // Log tag for this class.
    private static final String TAG = "ModernLinker";

    // Becomes true once prepareLibraryLoad() has been called.
    private boolean mPrepareLibraryLoadCalled;

    // Whether the libraries are loaded from the APK directly.
    private boolean mLoadingFromApk;

    // Library load path -> relro info.
    private HashMap<String, LibInfo> mSharedRelros;

    ModernLinker() {}

    /**
     * Call this method just before loading any native shared libraries in this process.
     * Loads the Linker's JNI library, and initializes the variables involved in the
     * implementation of shared RELROs.
     */
    @Override
    void prepareLibraryLoad(@Nullable String apkFilePath) {
        if (DEBUG) Log.i(TAG, "prepareLibraryLoad() called");

        synchronized (sLock) {
            assert !mPrepareLibraryLoadCalled;
            ensureInitializedLocked();

            // If in the browser, generate a random base load address for service processes
            // and create an empty shared RELROs map. For service processes, the shared
            // RELROs map must remain null until set by useSharedRelros().
            if (mInBrowserProcess) {
                setupBaseLoadAddressLocked();
                mSharedRelros = new HashMap<String, LibInfo>();
            }

            // Create an empty loaded libraries map.
            mLoadedLibraries = new HashMap<String, LibInfo>();

            // Start the current load address at the base load address.
            mCurrentLoadAddress = mBaseLoadAddress;

            mLoadingFromApk = apkFilePath != null;

            mPrepareLibraryLoadCalled = true;
        }
    }

    /**
     * Call this method just after loading all native shared libraries in this process.
     * If not in the browser, closes the LibInfo entries used for RELRO sharing.
     */
    @Override
    void finishLibraryLoad() {
        if (DEBUG) Log.i(TAG, "finishLibraryLoad() called");

        synchronized (sLock) {
            assert mPrepareLibraryLoadCalled;

            // Close shared RELRO file descriptors if not in the browser.
            if (!mInBrowserProcess && mSharedRelros != null) {
                closeLibInfoMap(mSharedRelros);
                mSharedRelros = null;
            }

            // If testing, run tests now that all libraries are loaded and initialized.
            if (NativeLibraries.sEnableLinkerTests) runTestRunnerClassForTesting(mInBrowserProcess);
        }
        if (DEBUG) Log.i(TAG, "finishLibraryLoad() done");
    }

    // Used internally to wait for shared RELROs. Returns once useSharedRelros() has been
    // called to supply a valid shared RELROs bundle.
    @GuardedBy("sLock")
    private void waitForSharedRelrosLocked() {
        if (DEBUG) Log.i(TAG, "waitForSharedRelros called");

        // Return immediately if shared RELROs are already available.
        if (mSharedRelros != null) return;

        // Wait until notified by useSharedRelros() that shared RELROs have arrived.
        long startTime = DEBUG ? SystemClock.uptimeMillis() : 0;
        while (mSharedRelros == null) {
            try {
                sLock.wait();
            } catch (InterruptedException e) {
                // Continue waiting even if we were just interrupted.
            }
        }

        if (DEBUG) {
            Log.i(TAG, "Time to wait for shared RELRO: %d ms",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    /**
     * Call this to send a Bundle containing the shared RELRO sections to be
     * used in this process. If initServiceProcess() was previously called,
     * libraryLoad() will wait until this method is called in another
     * thread with a non-null value.
     *
     * @param bundle The Bundle instance containing a map of shared RELRO sections
     * to use in this process.
     */
    @Override
    public void useSharedRelros(Bundle bundle) {
        if (DEBUG) Log.i(TAG, "useSharedRelros() called with " + bundle);

        synchronized (sLock) {
            mSharedRelros = createLibInfoMapFromBundle(bundle);
            sLock.notifyAll();
        }
    }

    /**
     * Call this to retrieve the shared RELRO sections created in this process,
     * after loading all libraries.
     *
     * @return a new Bundle instance, or null if RELRO sharing is disabled on
     * this system, or if initServiceProcess() was called previously.
     */
    @Override
    public Bundle getSharedRelros() {
        if (DEBUG) Log.i(TAG, "getSharedRelros() called");
        synchronized (sLock) {
            if (!mInBrowserProcess) {
                if (DEBUG) Log.i(TAG, "Not in browser, so returning null Bundle");
                return null;
            }

            // Create a new Bundle containing RELRO section information for all the shared
            // RELROs created while loading libraries.
            if (mSharedRelrosBundle == null && mSharedRelros != null) {
                mSharedRelrosBundle = createBundleFromLibInfoMap(mSharedRelros);
                if (DEBUG) {
                    Log.i(TAG, "Shared RELRO bundle created from map: %s", mSharedRelrosBundle);
                }
            }
            if (DEBUG) Log.i(TAG, "Returning " + mSharedRelrosBundle);
            return mSharedRelrosBundle;
        }
    }

    /**
     * Retrieve the base load address for libraries that share RELROs.
     *
     * @return a common, random base load address, or 0 if RELRO sharing is
     * disabled.
     */
    @Override
    public long getBaseLoadAddress() {
        synchronized (sLock) {
            ensureInitializedLocked();
            setupBaseLoadAddressLocked();
            if (DEBUG) Log.i(TAG, "getBaseLoadAddress() returns 0x%x", mBaseLoadAddress);

            return mBaseLoadAddress;
        }
    }

    /**
     * Load a native shared library with the Chromium linker. If the zip file
     * is not null, the shared library must be uncompressed and page aligned
     * inside the zipfile. The library must not be the Chromium linker library.
     *
     * If asked to wait for shared RELROs, this function will block library loads
     * until the shared RELROs bundle is received by useSharedRelros().
     *
     * @param libFilePath The path of the library (possibly in the zip file).
     * @param isFixedAddressPermitted If true, uses a fixed load address if one was
     * supplied, otherwise ignores the fixed address and loads wherever available.
     */
    @Override
    void loadLibraryImpl(String libFilePath, boolean isFixedAddressPermitted) {
        if (DEBUG) Log.i(TAG, "loadLibraryImpl: " + libFilePath + ", " + isFixedAddressPermitted);

        synchronized (sLock) {
            assert mPrepareLibraryLoadCalled;

            // When ChromeModern is loaded on N+, the library has the prefix "crazy."
            // added to it. Make sure that it is set correctly.
            String dlopenExtPath = mLoadingFromApk ? "crazy." + libFilePath : libFilePath;

            if (mLoadedLibraries.containsKey(dlopenExtPath)) {
                if (DEBUG) Log.i(TAG, "Not loading %s twice", libFilePath);
                return;
            }

            // Cannot use System.loadLibrary(), as the library name is transformed (adding the
            // "lib" prefix and ".so" suffix), making the name incorrect.
            if (!isFixedAddressPermitted) {
                boolean ok = nativeLoadLibraryNoRelros(dlopenExtPath);
                if (!ok) {
                    throw new UnsatisfiedLinkError(
                            "Unable to load " + dlopenExtPath + " without relro sharing");
                }
                return;
            }

            // If not in the browser and shared RELROs are not disabled, load the library at a fixed
            // address. Otherwise, load anywhere.
            long loadAddress = 0;
            if (mWaitForSharedRelros) {
                loadAddress = mCurrentLoadAddress;

                // For multiple libraries, ensure we stay within reservation range.
                if (loadAddress > mBaseLoadAddress + ADDRESS_SPACE_RESERVATION) {
                    String errorMessage = "Load address outside reservation, for: " + libFilePath;
                    Log.e(TAG, errorMessage);
                    throw new UnsatisfiedLinkError(errorMessage);
                }
            }

            LibInfo libInfo = new LibInfo();
            boolean alreadyLoaded = false;
            if (mInBrowserProcess && mCurrentLoadAddress != 0) {
                // We are in the browser, and with a current load address that indicates that
                // there is enough address space for shared RELRO to operate. Create the
                // shared RELRO, and store it in the map.
                String relroPath = PathUtils.getDataDirectory() + "/RELRO:" + libFilePath;
                if (nativeLoadLibraryCreateRelros(
                            dlopenExtPath, mCurrentLoadAddress, relroPath, libInfo)) {
                    mSharedRelros.put(dlopenExtPath, libInfo);
                    alreadyLoaded = true;
                } else {
                    String errorMessage = "Unable to create shared relro: " + relroPath;
                    Log.w(TAG, errorMessage);
                }
            } else if (!mInBrowserProcess && mCurrentLoadAddress != 0 && mWaitForSharedRelros) {
                // We are in a service process, again with a current load address that is
                // suitable for shared RELRO, and we are to wait for shared RELROs. So
                // do that, then use the map we receive to provide libinfo for library load.
                waitForSharedRelrosLocked();
                if (mSharedRelros.containsKey(dlopenExtPath)) {
                    libInfo = mSharedRelros.get(dlopenExtPath);
                }
            }

            // Load the library. In the browser, loadAddress is 0, so nativeLoadLibrary()
            // will load without shared RELRO. Otherwise, it uses shared RELRO if the attached
            // libInfo is usable.
            if (!alreadyLoaded) {
                if (!nativeLoadLibraryUseRelros(dlopenExtPath, loadAddress, libInfo.mRelroFd)) {
                    String errorMessage = "Unable to load library: " + dlopenExtPath;
                    Log.e(TAG, errorMessage);
                    throw new UnsatisfiedLinkError(errorMessage);
                }
            }
            // Print the load address to the logcat when testing the linker. The format
            // of the string is expected by the Python test_runner script as one of:
            //    BROWSER_LIBRARY_ADDRESS: <library-name> <address>
            //    RENDERER_LIBRARY_ADDRESS: <library-name> <address>
            // Where <library-name> is the library name, and <address> is the hexadecimal load
            // address.
            if (NativeLibraries.sEnableLinkerTests) {
                String tag =
                        mInBrowserProcess ? "BROWSER_LIBRARY_ADDRESS" : "RENDERER_LIBRARY_ADDRESS";
                Log.i(TAG,
                        String.format(
                                Locale.US, "%s: %s %x", tag, libFilePath, libInfo.mLoadAddress));
            }

            if (loadAddress != 0 && mCurrentLoadAddress != 0) {
                // Compute the next current load address. If mCurrentLoadAddress
                // is not 0, this is an explicit library load address.
                mCurrentLoadAddress = libInfo.mLoadAddress + libInfo.mLoadSize;
            }

            mLoadedLibraries.put(dlopenExtPath, libInfo);
            if (DEBUG) Log.i(TAG, "Library details %s", libInfo.toString());
        }
    }

    /**
     * Native method to return the CPU ABI.
     * Obtaining it from the linker's native code means that we always correctly
     * match the loaded library's ABI to the linker's ABI.
     *
     * @return CPU ABI string.
     */
    private static native String nativeGetCpuAbi();

    private static native boolean nativeLoadLibraryCreateRelros(
            String dlopenExtPath, long loadAddress, String relroPath, LibInfo libInfo);
    private static native boolean nativeLoadLibraryUseRelros(
            String dlopenExtPath, long loadAddress, int fd);
    private static native boolean nativeLoadLibraryNoRelros(String dlopenExtPath);
}
