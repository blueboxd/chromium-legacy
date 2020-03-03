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

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://media-app', window.parent);

parentMessagePipe.registerHandler('file', (message) => {
  const fileMessage = /** @type mediaApp.MessageEventData */ (message);
  if (fileMessage.file) {
    loadFile(fileMessage.file);
  }
})

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
    await app.loadFiles(fileList);
  } else {
    window.customLaunchData = {files: fileList};
  }
}

// Attempting to execute chooseFileSystemEntries is guaranteed to result in a
// SecurityError due to the fact that we are running in a unprivileged iframe.
// Note, we can not do window.chooseFileSystemEntries due to the fact that
// closure does not yet know that 'chooseFileSystemEntries' is on the window.
// TODO(crbug/1040328): Remove this when we have a polyfill that allows us to
// talk to the privileged frame.
window['chooseFileSystemEntries'] = null;
