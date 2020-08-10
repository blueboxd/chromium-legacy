// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * @type {!MockProgressCenter}
 */
let progressCenter;

/**
 * @type {!DriveSyncHandlerImpl}
 */
let driveSyncHandler;

/**
 * Mock chrome APIs.
 * @type {Object}
 */
const mockChrome = {};

mockChrome.fileManagerPrivate = {
  onFileTransfersUpdated: {
    addListener: function(callback) {
      mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_ = null;
    },
    listener_: null
  },
  onDriveSyncError: {
    addListener: function(callback) {
      mockChrome.fileManagerPrivate.onDriveSyncError.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onDriveSyncError.listener_ = null;
    },
    listener_: null
  },
  onPreferencesChanged: {
    addListener: function(callback) {
      mockChrome.fileManagerPrivate.onPreferencesChanged.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onPreferencesChanged.listener_ = null;
    },
    listener_: null
  },
  onDriveConnectionStatusChanged: {
    addListener: function(callback) {
      mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged.listener_ =
          callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged.listener_ =
          null;
    },
    listener_: null
  },
  getPreferences: function() {},
  setPreferences: function() {},

  getDriveConnectionState: function(callback) {
    callback({type: 'offline', reason: 'no_network'});
  },
};

mockChrome.notifications = {
  onButtonClicked: {
    addListener: function(callback) {
      mockChrome.notifications.onButtonClicked.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.notifications.onButtonClicked.listener_ = null;
    },
    listener_: null
  },
};

/**
 * Stub out file URLs handling.
 *
 * @param {string} url
 * @param {function(!Entry)} successCallback
 * @param {function(!FileError)=} opt_errorCallback
 */
window.webkitResolveLocalFileSystemURL =
    (url, successCallback, opt_errorCallback) => {
      successCallback(/** @type {!Entry} */ ({name: url}));
    };

// Mock window.str|strf string calls from drive sync handler.
window.str = (...args) => {
  return args.join(' ');
};
window.strf = window.str;

// Set up the test components.
function setUp() {
  // Install mock chrome APIs.
  installMockChrome(mockChrome);

  // Create a mock ProgressCenter.
  progressCenter = new MockProgressCenter();

  // Create DriveSyncHandlerImpl.
  driveSyncHandler = new DriveSyncHandlerImpl(progressCenter);

  // Check: Drive sync is enabled at creation time.
  assertFalse(driveSyncHandler.isSyncSuppressed());
}

// Test that in general case item IDs produced for errors are unique.
function testUniqueErrorIds() {
  // Dispatch an event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'service_unavailable',
    fileUrl: '',
  });

  // Check that this created one item.
  assertEquals(1, progressCenter.getItemCount());

  // Dispatch another event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'service_unavailable',
    fileUrl: '',
  });

  // Check that this created a second item.
  assertEquals(2, progressCenter.getItemCount());
}

// Test that item IDs produced for quota errors are same.
function testErrorDedupe() {
  // Dispatch an event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'no_server_space',
    fileUrl: '',
  });

  // Check that this created one item.
  assertEquals(1, progressCenter.getItemCount());

  // Dispatch another event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'no_server_space',
    fileUrl: '',
  });

  // Check that this did not create a second item.
  assertEquals(1, progressCenter.getItemCount());
}

function testErrorWithoutPath() {
  const originalStub = window.webkitResolveLocalFileSystemURL;
  /**
   * Temporary stub the entry resolving to always fail.
   *
   * @param {string} url
   * @param {function(!Entry)} successCallback
   * @param {function(!FileError)=} errorCallback
   */
  window.webkitResolveLocalFileSystemURL =
      (url, successCallback, errorCallback) => {
        errorCallback(/** @type {!FileError} */ ({}));
      };

  try {
    // Dispatch an event.
    mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
      type: 'service_unavailable',
      fileUrl: '',
    });

    // Check that this created one item.
    assertEquals(1, progressCenter.getItemCount());
  } finally {
    window.webkitResolveLocalFileSystemURL = originalStub;
  }
}

// Test offline.
function testOffline() {
  // Start a transfer.
  mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_({
    fileUrl: 'name',
    transferState: 'in_progress',
    processed: 50.0,
    total: 100.0,
    numTotalJobs: 1,
    hideWhenZeroJobs: true,
  });

  // Check that this created one item.
  assertEquals(1, progressCenter.getItemCount());
  let item = progressCenter.items['drive-sync'];
  assertEquals(ProgressItemState.PROGRESSING, item.state);
  assertTrue(driveSyncHandler.syncing);

  // Go offline.
  mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged.listener_();

  // Check that this item was cancelled.
  assertEquals(1, progressCenter.getItemCount());
  item = progressCenter.items['drive-sync'];
  assertEquals(ProgressItemState.CANCELED, item.state);
  assertFalse(driveSyncHandler.syncing);
}
