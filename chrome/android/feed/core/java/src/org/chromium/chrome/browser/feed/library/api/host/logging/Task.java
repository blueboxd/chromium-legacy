// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import android.support.annotation.IntDef;

/**
 * IntDef representing the different tasks that can be queued on the {@link
 * org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue}.
 *
 * <p>When adding new values, the value of {@link Task#NEXT_VALUE} should be used and incremented.
 * When removing values, {@link Task#NEXT_VALUE} should not be changed, and those values should not
 * be reused.
 */
@IntDef({
        Task.UNKNOWN,
        Task.CLEAN_UP_SESSION_JOURNALS,
        Task.CLEAR_ALL,
        Task.CLEAR_ALL_WITH_REFRESH,
        Task.CLEAR_PERSISTENT_STORE_TASK,
        Task.COMMIT_TASK,
        Task.CREATE_AND_UPLOAD,
        Task.DETACH_SESSION,
        Task.DISMISS_LOCAL,
        Task.DUMP_EPHEMERAL_ACTIONS,
        Task.EXECUTE_UPLOAD_ACTION_REQUEST,
        Task.GARBAGE_COLLECT_CONTENT,
        Task.GET_EXISTING_SESSION,
        Task.GET_NEW_SESSION,
        Task.GET_STREAM_FEATURES_FROM_HEAD,
        Task.HANDLE_RESPONSE_BYTES,
        Task.HANDLE_SYNTHETIC_TOKEN,
        Task.HANDLE_TOKEN,
        Task.HANDLE_UPLOADABLE_ACTION_RESPONSE_BYTES,
        Task.INVALIDATE_HEAD,
        Task.INVALIDATE_SESSION,
        Task.LOCAL_ACTION_GC,
        Task.NO_CARD_ERROR_CLEAR,
        Task.PERSIST_MUTATION,
        Task.POPULATE_NEW_SESSION,
        Task.REQUEST_FAILURE,
        Task.REQUEST_MANAGER_TRIGGER_REFRESH,
        Task.SEND_REQUEST,
        Task.SESSION_MANAGER_TRIGGER_REFRESH,
        Task.SESSION_MUTATION,
        Task.TASK_QUEUE_INITIALIZE,
        Task.UPLOAD_ALL_ACTIONS_FOR_URL,
        Task.NEXT_VALUE,
})
public @interface Task {
    // Unknown task.
    int UNKNOWN = 0;

    int CLEAN_UP_SESSION_JOURNALS = 1;
    int CLEAR_ALL = 2;
    int CLEAR_ALL_WITH_REFRESH = 3;
    int CLEAR_PERSISTENT_STORE_TASK = 4;
    int COMMIT_TASK = 5;
    int CREATE_AND_UPLOAD = 6;
    int DETACH_SESSION = 7;
    int DISMISS_LOCAL = 8;
    int DUMP_EPHEMERAL_ACTIONS = 9;
    int EXECUTE_UPLOAD_ACTION_REQUEST = 10;
    int GARBAGE_COLLECT_CONTENT = 11;
    int GET_EXISTING_SESSION = 12;
    int GET_NEW_SESSION = 13;
    int GET_STREAM_FEATURES_FROM_HEAD = 14;
    int HANDLE_RESPONSE_BYTES = 15;
    int HANDLE_SYNTHETIC_TOKEN = 16;
    int HANDLE_TOKEN = 17;
    int HANDLE_UPLOADABLE_ACTION_RESPONSE_BYTES = 18;
    int INVALIDATE_HEAD = 19;
    int INVALIDATE_SESSION = 20;
    int LOCAL_ACTION_GC = 21;
    int NO_CARD_ERROR_CLEAR = 22;
    int PERSIST_MUTATION = 23;
    int POPULATE_NEW_SESSION = 24;
    int REQUEST_FAILURE = 25;
    int REQUEST_MANAGER_TRIGGER_REFRESH = 26;
    int SEND_REQUEST = 27;
    int SESSION_MANAGER_TRIGGER_REFRESH = 28;
    int SESSION_MUTATION = 29;
    int TASK_QUEUE_INITIALIZE = 30;
    int UPLOAD_ALL_ACTIONS_FOR_URL = 32;

    // The next value that should be used when adding additional values to the IntDef.
    int NEXT_VALUE = 33;
}
