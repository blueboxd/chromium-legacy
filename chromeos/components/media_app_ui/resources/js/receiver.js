// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://media-app', window.parent);

/**
 * A file received from the privileged context, and decorated with IPC methods
 * added in the untrusted (this) context to communicate back.
 * @implements {mediaApp.AbstractFile}
 */
class ReceivedFile {
  /** @param {!FileContext} file */
  constructor(file) {
    this.blob = file.file || new File([], file.name);
    this.name = this.blob.name;
    this.size = this.blob.size;
    this.mimeType = this.blob.type;
    this.token = file.token;
    this.error = file.error;
  }

  /**
   * @override
   * @param{!Blob} blob
   */
  async overwriteOriginal(blob) {
    /** @type{OverwriteFileMessage} */
    const message = {token: this.token, blob: blob};

    await parentMessagePipe.sendMessage(Message.OVERWRITE_FILE, message);

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
    try {
      const deleteResponse =
          /** @type {!DeleteFileResponse} */ (
              await parentMessagePipe.sendMessage(
                  Message.DELETE_FILE, {token: this.token}));
      return deleteResponse.deleteResult;
    } catch (/** @type {!Error} */ errorResponse) {
      // TODO(b/156205603): use internal `chrome.crashReportPrivate.reportError`
      // to report the error.
      console.warn(errorResponse);
      return DeleteResult.UNKNOWN_ERROR;
    }
  }

  /**
   * @override
   * @param {string} newName
   * @return {!Promise<number>}
   */
  async renameOriginalFile(newName) {
    try {
      const renameResponse =
          /** @type {!RenameFileResponse} */ (
              await parentMessagePipe.sendMessage(
                  Message.RENAME_FILE,
                  {token: this.token, newFilename: newName}));
      return renameResponse.renameResult;
    } catch (/** @type {!Error} */ errorResponse) {
      // TODO(b/156205603): use internal `chrome.crashReportPrivate.reportError`
      // to report the error.
      console.warn(errorResponse);
      return RenameResult.UNKNOWN_ERROR;
    }
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
    this.files = files.map(f => new ReceivedFile(f));
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
