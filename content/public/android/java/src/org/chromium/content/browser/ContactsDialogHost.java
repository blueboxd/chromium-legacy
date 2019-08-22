// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.Manifest;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.ContactsPickerListener;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * The host class for the ContactsDialog. Handles communication between the Java side and the C++
 * side.
 */
@JNINamespace("content")
public class ContactsDialogHost implements ContactsPickerListener {
    private long mNativeContactsProviderAndroid;
    private final WindowAndroid mWindowAndroid;

    @CalledByNative
    static ContactsDialogHost create(
            WindowAndroid windowAndroid, long nativeContactsProviderAndroid) {
        return new ContactsDialogHost(windowAndroid, nativeContactsProviderAndroid);
    }

    private ContactsDialogHost(WindowAndroid windowAndroid, long nativeContactsProviderAndroid) {
        mNativeContactsProviderAndroid = nativeContactsProviderAndroid;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    void destroy() {
        mNativeContactsProviderAndroid = 0;
    }

    @CalledByNative
    private void showDialog(boolean multiple, boolean includeNames, boolean includeEmails,
            boolean includeTel, String formattedOrigin) {
        if (mWindowAndroid.getActivity().get() == null) {
            nativeEndWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        if (mWindowAndroid.hasPermission(Manifest.permission.READ_CONTACTS)) {
            if (!UiUtils.showContactsPicker(mWindowAndroid.getActivity().get(), this, multiple,
                        includeNames, includeEmails, includeTel, formattedOrigin)) {
                nativeEndWithPermissionDenied(mNativeContactsProviderAndroid);
            }
            return;
        }

        if (!mWindowAndroid.canRequestPermission(Manifest.permission.READ_CONTACTS)) {
            nativeEndWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        mWindowAndroid.requestPermissions(
                new String[] {Manifest.permission.READ_CONTACTS}, (permissions, grantResults) -> {
                    if (permissions.length == 1 && grantResults.length == 1
                            && TextUtils.equals(permissions[0], Manifest.permission.READ_CONTACTS)
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        if (!UiUtils.showContactsPicker(mWindowAndroid.getActivity().get(), this,
                                    multiple, includeNames, includeEmails, includeTel,
                                    formattedOrigin)) {
                            nativeEndWithPermissionDenied(mNativeContactsProviderAndroid);
                        }
                    } else {
                        nativeEndWithPermissionDenied(mNativeContactsProviderAndroid);
                    }
                });
    }

    @Override
    public void onContactsPickerUserAction(
            @ContactsPickerAction int action, List<Contact> contacts, int percentageShared) {
        switch (action) {
            case ContactsPickerAction.CANCEL:
                nativeEndContactsList(mNativeContactsProviderAndroid, 0);
                break;

            case ContactsPickerAction.CONTACTS_SELECTED:
                for (Contact contact : contacts) {
                    nativeAddContact(mNativeContactsProviderAndroid, contact.names != null,
                            contact.emails != null, contact.tel != null,
                            contact.names != null
                                    ? contact.names.toArray(new String[contact.names.size()])
                                    : null,
                            contact.emails != null
                                    ? contact.emails.toArray(new String[contact.emails.size()])
                                    : null,
                            contact.tel != null
                                    ? contact.tel.toArray(new String[contact.tel.size()])
                                    : null);
                }
                nativeEndContactsList(mNativeContactsProviderAndroid, percentageShared);
                break;

            case ContactsPickerAction.SELECT_ALL:
            case ContactsPickerAction.UNDO_SELECT_ALL:
                break;
        }
    }

    private static native void nativeAddContact(long nativeContactsProviderAndroid,
            boolean includeNames, boolean includeEmails, boolean includeTel, String[] names,
            String[] emails, String[] tel);
    private static native void nativeEndContactsList(
            long nativeContactsProviderAndroid, int percentageShared);
    private static native void nativeEndWithPermissionDenied(long nativeContactsProviderAndroid);
}
