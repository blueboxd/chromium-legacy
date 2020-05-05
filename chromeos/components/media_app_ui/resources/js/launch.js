// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{token: number, file: !File, handle: !FileSystemFileHandle}} */
let FileDescriptor;

/**
 * Array of entries available in the current directory.
 *
 * @type {!Array<!FileDescriptor>}
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
 * @type {?FileDescriptor}
 */
let currentlyWritableFile = null;

/**
 * Reference to the directory handle that contains the first file in the most
 * recent launch event.
 * @type {?FileSystemDirectoryHandle}
 */
let currentDirectoryHandle = null;

/** A pipe through which we can send messages to the guest frame. */
const guestMessagePipe = new MessagePipe('chrome-untrusted://media-app');

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  let response = mediaAppPageHandler.openFeedbackDialog();
  if (response === null) {
    response = {errorMessage: 'Null response received'};
  }
  return response;
});

guestMessagePipe.registerHandler(Message.OVERWRITE_FILE, async (message) => {
  const overwrite = /** @type {OverwriteFileMessage} */ (message);
  if (!currentlyWritableFile || overwrite.token !== fileToken) {
    throw new Error('File not current.');
  }

  await saveBlobToFile(currentlyWritableFile.handle, overwrite.blob);
});

guestMessagePipe.registerHandler(Message.DELETE_FILE, async (message) => {
  const deleteMsg = /** @type{DeleteFileMessage} */ (message);
  assertFileAndDirectoryMutable(deleteMsg.token, 'Delete');

  if (!(await isCurrentHandleInCurrentDirectory())) {
    return {deleteResult: DeleteResult.FILE_MOVED};
  }

  // Get the name from the file reference. Handles file renames.
  const currentFilename = (await currentlyWritableFile.handle.getFile()).name;

  await currentDirectoryHandle.removeEntry(currentFilename);
  return {deleteResult: DeleteResult.SUCCESS};
});

/** Handler to rename the currently focused file. */
guestMessagePipe.registerHandler(Message.RENAME_FILE, async (message) => {
  const renameMsg = /** @type{RenameFileMessage} */ (message);
  assertFileAndDirectoryMutable(renameMsg.token, 'Rename');

  if (await filenameExistsInCurrentDirectory(renameMsg.newFilename)) {
    return {renameResult: RenameResult.FILE_EXISTS};
  }

  const originalFile = await currentlyWritableFile.handle.getFile();
  const renamedFileHandle = await currentDirectoryHandle.getFile(
      renameMsg.newFilename, {create: true});
  // Copy file data over to the new file.
  const writer = await renamedFileHandle.createWritable();
  // TODO(b/153021155): Use originalFile.stream().
  await writer.write(await originalFile.arrayBuffer());
  await writer.truncate(originalFile.size);
  await writer.close();

  // Remove the old file since the new file has all the data & the new name.
  // Note even though removing an entry that doesn't exist is considered
  // success, we first check the `currentlyWritableFile.handle` is the same as
  // the handle for the file with that filename in the `currentDirectoryHandle`.
  if (await isCurrentHandleInCurrentDirectory()) {
    await currentDirectoryHandle.removeEntry(originalFile.name);
  }

  // Reload current file so it is in an editable state, this is done before
  // removing the old file so the relaunch starts sooner.
  await launchWithDirectory(currentDirectoryHandle, renamedFileHandle);

  return {renameResult: RenameResult.SUCCESS};
});

guestMessagePipe.registerHandler(Message.NAVIGATE, async (message) => {
  const navigate = /** @type {NavigateMessage} */ (message);

  await advance(navigate.direction);
});

guestMessagePipe.registerHandler(Message.SAVE_COPY, async (message) => {
  const {blob, suggestedName} = /** @type {!SaveCopyMessage} */ (message);
  const extension = suggestedName.split('.').reverse()[0];
  // TODO(b/141587270): Add a default filename when it's supported by the native
  // file api.
  /** @type {!ChooseFileSystemEntriesOptions} */
  const options = {
    type: 'save-file',
    accepts: [{extension, mimeTypes: [blob.type]}]
  };
  /** @type {!FileSystemHandle} */
  let fileSystemHandle;
  // chooseFileSystem is where recoverable errors happen, errors in the write
  // process should be treated as unexpected and propagated through
  // MessagePipe's standard exception handling.
  try {
    fileSystemHandle =
        /** @type {!FileSystemHandle} */ (
            await window.chooseFileSystemEntries(options));
  } catch (/** @type {!DOMException} */ err) {
    if (err.name !== 'SecurityError' && err.name !== 'AbortError') {
      // Unknown error.
      throw err;
    }
    console.log(`Aborting SAVE_COPY: ${err.message}`);
    return err.name;
  }

  const {handle} = await getFileFromHandle(fileSystemHandle);
  if (await handle.isSameEntry(currentlyWritableFile.handle)) {
    return 'attemptedCurrentlyWritableFileOverwrite';
  }

  await saveBlobToFile(handle, blob);
});

/**
 * Saves the provided blob the provided fileHandle. Assumes the handle is
 * writable.
 * @param {!FileSystemFileHandle} handle
 * @param {!Blob} data
 * @return {!Promise<undefined>}
 */
async function saveBlobToFile(handle, data) {
  const writer = await handle.createWritable();
  await writer.write(data);
  await writer.truncate(data.size);
  await writer.close();
}

/**
 * Loads a single file into the guest.
 * @param {{file: !File, handle: !FileSystemFileHandle}} fileHandle
 * @returns {!Promise<undefined>}
 */
async function loadSingleFile(fileHandle) {
  /** @type {!FileDescriptor} */
  const fd = {token: -1, file: fileHandle.file, handle: fileHandle.handle};
  currentFiles.length = 0;
  currentFiles.push(fd);
  entryIndex = 0;
  await sendFilesToGuest();
}

/**
 * Loads the current file list into the guest.
 * @return {!Promise<undefined>}
 */
async function sendFilesToGuest() {
  // Before sending to guest ensure writableFileIndex is set to be writable,
  // also clear the old token.
  if (currentlyWritableFile) {
    currentlyWritableFile.token = -1;
  }
  currentlyWritableFile = currentFiles[entryIndex];
  currentlyWritableFile.token = ++fileToken;

  /** @type {!LoadFilesMessage} */
  const loadFilesMessage = {
    writableFileIndex: entryIndex,
    // Handle can't be passed through a message pipe.
    files: currentFiles.map(fd => ({token: fd.token, file: fd.file}))
  };
  await guestMessagePipe.sendMessage(Message.LOAD_FILES, loadFilesMessage);
}

/**
 * Throws an error if the file or directory handles don't exist or the token for
 * the file to be mutated is incorrect.
 * @param {number} editFileToken
 * @param {string} operation
 */
function assertFileAndDirectoryMutable(editFileToken, operation) {
  if (!currentlyWritableFile || editFileToken !== fileToken) {
    throw new Error(`${operation} failed. File not current.`);
  }

  if (!currentDirectoryHandle) {
    throw new Error(`${operation} failed. File without launch directory.`);
  }
}

/**
 * Returns whether `currentlyWritableFile.handle` is in`currentDirectoryHandle`.
 * Prevents mutating the wrong file or a file that doesn't exist.
 * @return {!Promise<!boolean>}
 */
async function isCurrentHandleInCurrentDirectory() {
  // Get the name from the file reference. Handles file renames.
  const currentFilename = (await currentlyWritableFile.handle.getFile()).name;
  const fileHandle = await getFileHandleFromCurrentDirectory(currentFilename);
  return fileHandle ? fileHandle.isSameEntry(currentlyWritableFile.handle) :
                      false;
}

/**
 * Returns if a`filename` exists in `currentDirectoryHandle`.
 * @param {string} filename
 * @return {!Promise<!boolean>}
 */
async function filenameExistsInCurrentDirectory(filename) {
  return (await getFileHandleFromCurrentDirectory(filename, true)) !== null;
}

/**
 * Returns the `FileSystemFileHandle` for `filename` if it exists in the current
 * directory, otherwise null.
 * @param {string} filename
 * @param {boolean} suppressError
 * @return {!Promise<!FileSystemHandle|null>}
 */
async function getFileHandleFromCurrentDirectory(
    filename, suppressError = false) {
  try {
    return (await currentDirectoryHandle.getFile(filename, {create: false}));
  } catch (/** @type {Object} */ e) {
    if (!suppressError) {
      console.error(e);
    }
    return null;
  }
}

/**
 * Gets a file from a handle received via the fileHandling API.
 * @param {?FileSystemHandle} fileSystemHandle
 * @return {Promise<?{file: !File, handle: !FileSystemFileHandle}>}
 */
async function getFileFromHandle(fileSystemHandle) {
  if (!fileSystemHandle || !fileSystemHandle.isFile) {
    return null;
  }
  const handle = /** @type {!FileSystemFileHandle} */ (fileSystemHandle);
  const file = await handle.getFile();
  return {file, handle};
}

/**
 * Returns whether `file` is a video or image file.
 * @param {!File} file
 * @return {boolean}
 */
function isVideoOrImage(file) {
  // Check for .mkv explicitly because it is not a web-supported type, but is in
  // common use on ChromeOS.
  return /^(image)|(video)\//.test(file.type) || /\.mkv$/.test(file.name);
}

/**
 * Returns whether `siblingFile` is related to `focusFile`. That is, whether
 * they should be traversable from one another. Usually this means they share a
 * similar (non-empty) MIME type.
 * @param {!File} focusFile The file selected by the user.
 * @param {!File} siblingFile A file in the same directory as `focusFile`.
 * @return {boolean}
 */
function isFileRelated(focusFile, siblingFile) {
  return focusFile.name === siblingFile.name ||
      (!!focusFile.type && focusFile.type === siblingFile.type) ||
      (isVideoOrImage(focusFile) && isVideoOrImage(siblingFile));
}

/**
 * Changes the working directory and initializes file iteration according to
 * the type of the opened file.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {?File} focusFile
 */
async function setCurrentDirectory(directory, focusFile) {
  if (!focusFile) {
    return;
  }
  currentFiles.length = 0;
  for await (const /** !FileSystemHandle */ handle of directory.getEntries()) {
    const asFile = await getFileFromHandle(handle);
    if (!asFile) {
      continue;
    }

    // Only allow traversal of related file types.
    if (isFileRelated(focusFile, asFile.file)) {
      currentFiles.push({token: -1, file: asFile.file, handle: asFile.handle});
    }
  }
  entryIndex = currentFiles.findIndex(i => i.file.name === focusFile.name);
  currentDirectoryHandle = directory;
}

/**
 * Launch the media app with the files in the provided directory.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {?FileSystemHandle} initialFileEntry
 */
async function launchWithDirectory(directory, initialFileEntry) {
  const asFile = await getFileFromHandle(initialFileEntry);
  await setCurrentDirectory(directory, asFile.file);

  // Load currentFiles into the guest.
  await sendFilesToGuest();
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

  await sendFilesToGuest();
}

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
    const directory =
        /** @type{!FileSystemDirectoryHandle} */ (params.files[0]);
    launchWithDirectory(directory, params.files[1]);
  });
});
