// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements mediaApp.AbstractFileList */
class SingleArrayBufferFileList {
  /** @param {!mediaApp.AbstractFile} file */
  constructor(file) {
    this.file = file;
    this.length = 1;
  }
  /** @override */
  item(index) {
    return index === 0 ? this.file : null;
  }
}

/**
 * Loads files associated with a message received from the host.
 * @param {!File} file
 */
async function loadFile(file) {
  const fileList = new SingleArrayBufferFileList({
    blob: file,
    size: file.size,
    mimeType: file.type,
    name: file.name,
  });

  const app = /** @type {?mediaApp.ClientApi} */ (
      document.querySelector('backlight-app'));
  if (app) {
    app.loadFiles(fileList);
  } else {
    window.customLaunchData = {files: fileList};
  }
}

function receiveMessage(/** Event */ e) {
  const event = /** @type{MessageEvent<Object>} */ (e);
  if (event.origin !== 'chrome://media-app') {
    return;
  }

  // First ensure the message is our MessageEventData type, then act on it
  // appropriately. Note test messages won't have a file (and are not handled by
  // this listener), so it's currently sufficient to just check for `file`.
  if ('file' in event.data) {
    const message =
        /** @type{MessageEvent<mediaApp.MessageEventData>}*/ (event);
    if (message.data.file) {
      loadFile(message.data.file);
    } else {
      console.error('Unknown message:', message);
    }
  }
}

// Attempting to execute chooseFileSystemEntries is guaranteed to result in a
// SecurityError due to the fact that we are running in a unprivileged iframe.
// Note, we can not do window.chooseFileSystemEntries due to the fact that
// closure does not yet know that 'chooseFileSystemEntries' is on the window.
// TODO(crbug/1040328): Remove this when we have a polyfill that allows us to
// talk to the privileged frame.
window['chooseFileSystemEntries'] = null;

window.addEventListener('message', receiveMessage, false);
