// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://media-app', window.parent);

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

  /**
   * @override
   * @param {string} newName
   * @return {!Promise<number>}
   */
  async renameOriginalFile(newName) {
    const renameResponse =
        /** @type {!RenameFileResponse} */ (await parentMessagePipe.sendMessage(
            Message.RENAME_FILE, {token: this.token, newFilename: newName}));
    return renameResponse.renameResult;
  }
}

/**
 * A file list consisting of all files received from the parent. Exposes the
 * currently writable file and all other readable files in the current
 * directory.
 * @implements mediaApp.AbstractFileList
 */
class ReceivedFileList {
  /** @param {!LoadFilesMessage} filesMessage */
  constructor(filesMessage) {
    // We make sure the 0th item in the list is the writable one so we
    // don't break older versions of the media app which uses item(0) instead
    // of getCurrentlyWritable()
    // TODO(b/151880563): remove this.
    let writableFileIndex = filesMessage.writableFileIndex;
    const files = filesMessage.files;
    while (writableFileIndex > 0) {
      files.push(files.shift());
      writableFileIndex--;
    }

    this.length = files.length;
    /** @type {!Array<!ReceivedFile>} */
    this.files = files.map(f => new ReceivedFile(f.file, f.token));
    /** @type {number} */
    this.writableFileIndex = 0;
  }

  /** @override */
  item(index) {
    return this.files[index] || null;
  }

  /**
   * Returns the file which is currently writable or null if there isn't one.
   * @return {?mediaApp.AbstractFile}
   */
  getCurrentlyWritable() {
    return this.item(this.writableFileIndex);
  }

  /**
   * Loads in the next file in the list as a writable.
   * @return {!Promise<undefined>}
   */
  async loadNext() {
    // Awaiting this message send allows callers to wait for the full effects of
    // the navigation to complete. This may include a call to load a new set of
    // files, and the initial decode, which replaces this AbstractFileList and
    // alters other app state.
    await parentMessagePipe.sendMessage(Message.NAVIGATE, {direction: 1});
  }

  /**
   * Loads in the previous file in the list as a writable.
   * @return {!Promise<undefined>}
   */
  async loadPrev() {
    await parentMessagePipe.sendMessage(Message.NAVIGATE, {direction: -1});
  }
}

parentMessagePipe.registerHandler(Message.LOAD_FILES, async (message) => {
  const filesMessage = /** @type {!LoadFilesMessage} */ (message);
  await loadFiles(new ReceivedFileList(filesMessage));
});

/**
 * A delegate which exposes privileged WebUI functionality to the media
 * app.
 * @type {!mediaApp.ClientApiDelegate}
 */
const DELEGATE = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return /** @type {?string} */ (response['errorMessage']);
  },
  async saveCopy(/** !mediaApp.AbstractFile */ abstractFile) {
    /** @type {!SaveCopyMessage} */
    const msg = {blob: abstractFile.blob, suggestedName: abstractFile.name};
    const response =
        /** @type {!SaveCopyResponse} */ (
            await parentMessagePipe.sendMessage(Message.SAVE_COPY, msg));
    return response.errorMessage;
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
 * Loads a file list into the media app.
 * @param {!ReceivedFileList} fileList
 * @return {!Promise<undefined>}
 */
async function loadFiles(fileList) {
  const app = getApp();
  if (app) {
    await app.loadFiles(fileList);
  } else {
    // Note we don't await in this case, which may affect b/152729704.
    window.customLaunchData = {files: fileList};
  }
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
