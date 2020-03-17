// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Array of entries available in the current directory.
 *
 * @type {Array<{file: !File, handle: !FileSystemFileHandle}>}
 */
const currentFiles = [];

/**
 * Index into `currentFiles` of the current file.
 *
 * @type {number}
 */
let entryIndex = -1;

/**
 * Token that identifies the file that is currently writable. Incremented each
 * time a new file is given focus.
 * @type {number}
 */
let fileToken = 0;

/**
 * The file currently writable.
 * @type {?FileSystemFileHandle}
 */
let currentlyWritableFileHandle = null;

/** A pipe through which we can send messages to the guest frame. */
const guestMessagePipe = new MessagePipe('chrome://media-app-guest');

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  let response = media_app.handler.openFeedbackDialog();
  if (response === null) {
    response = {errorMessage: 'Null response received'};
  }
  return response;
});

guestMessagePipe.registerHandler(Message.OVERWRITE_FILE, async (message) => {
  const overwrite = /** @type{OverwriteFileMessage} */ (message);
  if (!currentlyWritableFileHandle || overwrite.token != fileToken) {
    throw new Error('File not current.');
  }
  const writer = await currentlyWritableFileHandle.createWritable();
  await writer.write(overwrite.blob);
  await writer.truncate(overwrite.blob.size);
  await writer.close();
});

/**
 * Loads a file in the guest.
 *
 * @param {?File} file
 * @param {!FileSystemFileHandle} handle
 */
function loadFile(file, handle) {
  const token = ++fileToken;
  currentlyWritableFileHandle = handle;
  guestMessagePipe.sendMessage(Message.LOAD_FILE, {token, file});
}

/**
 * Loads a file from a handle received via the fileHandling API.
 *
 * @param {?FileSystemHandle} handle
 * @return {Promise<?File>}
 */
async function loadFileFromHandle(handle) {
  if (!handle || !handle.isFile) {
    return null;
  }

  const fileHandle = /** @type{!FileSystemFileHandle} */ (handle);
  const file = await fileHandle.getFile();
  loadFile(file, fileHandle);
  return file;
}

/**
 * Changes the working directory and initializes file iteration according to
 * the type of the opened file.
 *
 * @param {FileSystemDirectoryHandle} directory
 * @param {?File} focusFile
 */
async function setCurrentDirectory(directory, focusFile) {
  if (!focusFile) {
    return;
  }
  currentFiles.length = 0;
  for await (const /** !FileSystemHandle */ handle of directory.getEntries()) {
    if (!handle.isFile) {
      continue;
    }
    const fileHandle = /** @type{FileSystemFileHandle} */ (handle);
    const file = await fileHandle.getFile();

    // Only allow traversal of matching mime types.
    if (file.type === focusFile.type) {
      currentFiles.push({file, handle: fileHandle});
    }
  }
  entryIndex = currentFiles.findIndex(i => i.file.name == focusFile.name);
}

/**
 * Advance to another file.
 *
 * @param {number} direction How far to advance (e.g. +/-1).
 */
async function advance(direction) {
  if (!currentFiles.length || entryIndex < 0) {
    return;
  }
  entryIndex = (entryIndex + direction) % currentFiles.length;
  if (entryIndex < 0) {
    entryIndex += currentFiles.length;
  }
  const entry = currentFiles[entryIndex];
  loadFile(entry.file, entry.handle);
}

document.getElementById('prev-container')
    .addEventListener('click', () => advance(-1));
document.getElementById('next-container')
    .addEventListener('click', () => advance(1));

// Wait for 'load' (and not DOMContentLoaded) to ensure the subframe has been
// loaded and is ready to respond to postMessage.
window.addEventListener('load', () => {
  window.launchQueue.setConsumer(params => {
    if (!params || !params.files || params.files.length < 2) {
      console.error('Invalid launch (missing files): ', params);
      return;
    }

    if (!params.files[0].isDirectory) {
      console.error('Invalid launch: files[0] is not a directory: ', params);
      return;
    }

    const directory = /** @type{FileSystemDirectoryHandle} */ (params.files[0]);
    loadFileFromHandle(params.files[1])
        .then(file => setCurrentDirectory(directory, file));
  });
});
