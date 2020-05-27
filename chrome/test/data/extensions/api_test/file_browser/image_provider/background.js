// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// A 1x1 transparent GIF in 42 bytes.
const GIF_DATA = new Uint8Array([
  71, 73, 70,  56,  57,  97, 1,   0, 1, 0, 128, 0,  0, 0,
  0,  0,  255, 255, 255, 33, 249, 4, 1, 0, 0,   0,  0, 44,
  0,  0,  0,   0,   1,   0,  1,   0, 0, 2, 1,   68, 0, 59
]);
const GIF_FILE = new File([GIF_DATA], 'pixel.gif', {type: 'image/gif'});

const GIF_ENTRY = Object.freeze({
  isDirectory: false,
  name: GIF_FILE.name,
  size: GIF_FILE.size,
  modificationTime: new Date(),
  mimeType: GIF_FILE.type,
});
const ROOT_ENTRY = Object.freeze({
  isDirectory: true,
  name: '',
  size: 0,
  modificationTime: new Date(),
  mimeType: 'text/directory',
});

const ENTRY_PATHS = {
  ['/']: ROOT_ENTRY,
  [`/${GIF_ENTRY.name}`]: GIF_ENTRY
};

function trace(...args) {
  console.log(...args);
}

function mountFileSystem() {
  chrome.fileSystemProvider.mount({
    fileSystemId: 'test-image-provider-fs',
    displayName: 'Test Image Provider FS'
  });
}

/** Copies and adjusts `entryTemplate` to suit the request in `options`. */
function makeEntry(entryTemplate, options) {
  const entry = {...entryTemplate};
  for (const prop
           of ['name', 'mimeType', 'modificationTime', 'isDirectory', 'size']) {
    if (!options[prop]) {
      delete entry[prop];
    }
  }
  return entry;
}

/** Find an entry. Invokes `onError('NOT_FOUND')` if `entry` is unknown. */
function findEntry(entryPath, onError, options, operation) {
  trace(operation, entryPath, options);
  const entry = ENTRY_PATHS[entryPath];
  if (!entry) {
    console.log(
        `Request for '${entryPath}': NOT_FOUND. ${JSON.stringify(options)}`);
    onError('NOT_FOUND');
  }
  return entry;
}

chrome.fileSystemProvider.onGetMetadataRequested.addListener(function(
    options, onSuccess, onError) {
  let entry = findEntry(options.entryPath, onError, options, 'metadata');
  if (entry) {
    onSuccess(makeEntry(entry, options));
  }
});

chrome.fileSystemProvider.onOpenFileRequested.addListener(function(
    options, onSuccess, onError) {
  if (findEntry(options.filePath, onError, options, 'open')) {
    trace('open-success');
    onSuccess();
  }
});

chrome.fileSystemProvider.onCloseFileRequested.addListener(function(
    options, onSuccess, onError) {
  trace('close-file', options);
  onSuccess();
  trace('close-success');
});

async function readGif(onSuccess) {
  const arrayBuffer = await GIF_FILE.arrayBuffer();
  onSuccess(arrayBuffer, false /* hasMore*/);
  trace('read-success');
}

chrome.fileSystemProvider.onReadFileRequested.addListener(function(
    options, onSuccess, onError) {
  trace('read-file', options);
  // Assume it's GIF for now.
  readGif(onSuccess);
});

chrome.fileSystemProvider.onReadDirectoryRequested.addListener(function(
    options, onSuccess, onError) {
  trace('open, ', options);
  // For anything other than root, return no entries.
  if (options.directoryPath !== '/') {
    onSuccess([], false /* hasMore */);
    return;
  }
  const entries = [makeEntry(GIF_ENTRY, options)];
  onSuccess(entries, false /* hasMore */);
});

chrome.fileSystemProvider.onUnmountRequested.addListener(function(
    options, onSuccess, onError) {
  trace('unmount', options);
  chrome.fileSystemProvider.unmount(
      {fileSystemId: options.fileSystemId}, onSuccess)
});

chrome.fileSystemProvider.onGetActionsRequested.addListener(function(
    options, onSuccess, onError) {
  trace('actions-requested', options);
  onSuccess([]);
});

// Hook onInstalled rather than onLaunched so it appears immediately.
chrome.runtime.onInstalled.addListener(mountFileSystem);
