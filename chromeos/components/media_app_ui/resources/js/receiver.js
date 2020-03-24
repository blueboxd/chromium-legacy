// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://media-app', window.parent);

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
 * A file received from the privileged context.
 * @implements {mediaApp.AbstractFile}
 */
class ReceivedFile {
  /**
   * @param {!File} file The received file.
   * @param {number} token A token that identifies the file.
   */
  constructor(file, token) {
    this.blob = file;
    this.name = file.name;
    this.size = file.size;
    this.mimeType = file.type;
    this.token = token;
  }

  /**
   * @override
   * @param{!Blob} blob
   */
  async overwriteOriginal(blob) {
    /** @type{OverwriteFileMessage} */
    const message = {token: this.token, blob: blob};
    const reply =
        parentMessagePipe.sendMessage(Message.OVERWRITE_FILE, message);
    try {
      await reply;
    } catch (/** @type{GenericErrorResponse} */ errorResponse) {
      if (errorResponse.message === 'File not current.') {
        const domError = new DOMError();
        domError.name = 'NotAllowedError';
        throw domError;
      }
      throw errorResponse;
    }
    // Note the following are skipped if an exception is thrown above.
    this.blob = blob;
    this.size = blob.size;
    this.mimeType = blob.type;
  }

  /**
   * @override
   * @return {!Promise<number>}
   */
  async deleteOriginalFile() {
    const deleteResponse =
        /** @type {!DeleteFileResponse} */ (await parentMessagePipe.sendMessage(
            Message.DELETE_FILE, {token: this.token}));
    return deleteResponse.deleteResult;
  }
}

parentMessagePipe.registerHandler(Message.LOAD_FILE, (message) => {
  const fileMessage = /** @type{!OpenFileMessage} */ (message);
  loadFile(fileMessage.token, fileMessage.file);
})

/**
 * A delegate which exposes privileged WebUI functionality to the media
 * app.
 * @type {!mediaApp.ClientApiDelegate}
 */
const DELEGATE = {
  /** @override */
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return /** @type {?string} */ (response['errorMessage']);
  }
};

/**
 * Returns the media app if it can find it in the DOM.
 * @return {?mediaApp.ClientApi}
 */
function getApp() {
  return /** @type {?mediaApp.ClientApi} */ (
      document.querySelector('backlight-app'));
}

/**
 * Loads files associated with a message received from the host.
 * @param {number} token
 * @param {!File} file
 * @return {!Promise<!ReceivedFile>} The received file (for testing).
 */
async function loadFile(token, file) {
  const receivedFile = new ReceivedFile(file, token);
  const fileList = new SingleArrayBufferFileList(receivedFile);

  const app = getApp();
  if (app) {
    await app.loadFiles(fileList);
  } else {
    window.customLaunchData = {files: fileList};
  }
  return receivedFile;
}

/**
 * Runs any initialization code on the media app once it is in the dom.
 * @param {!mediaApp.ClientApi} app
 */
function initializeApp(app) {
  app.setDelegate(DELEGATE);
}

/**
 * Called when a mutation occurs on document.body to check if the media app is
 * available.
 * @param {!Array<!MutationRecord>} mutationsList
 * @param {!MutationObserver} observer
 */
function mutationCallback(mutationsList, observer) {
  const app = getApp();
  if (!app) {
    return;
  }
  // The media app now exists so we can initialize it.
  initializeApp(app);
  observer.disconnect();
}

window.addEventListener('DOMContentLoaded', () => {
  const app = getApp();
  if (app) {
    initializeApp(app);
    return;
  }
  // If translations need to be fetched, the app element may not be added yet.
  // In that case, observe <body> until it is.
  const observer = new MutationObserver(mutationCallback);
  observer.observe(document.body, {childList: true});
});

// Attempting to execute chooseFileSystemEntries is guaranteed to result in a
// SecurityError due to the fact that we are running in a unprivileged iframe.
// Note, we can not do window.chooseFileSystemEntries due to the fact that
// closure does not yet know that 'chooseFileSystemEntries' is on the window.
// TODO(crbug/1040328): Remove this when we have a polyfill that allows us to
// talk to the privileged frame.
window['chooseFileSystemEntries'] = null;
