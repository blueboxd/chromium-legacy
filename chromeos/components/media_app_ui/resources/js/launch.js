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
  const writer = await currentlyWritableFile.handle.createWritable();
  await writer.write(overwrite.blob);
  await writer.truncate(overwrite.blob.size);
  await writer.close();
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
  const handle = /** @type{!FileSystemFileHandle} */ (fileSystemHandle);
  const file = await handle.getFile();
  return {file, handle};
}

/**
 * Changes the working directory and initializes file iteration according to
 * the type of the opened file.
 *
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

    // Only allow traversal of matching mime types.
    if (asFile.file.type === focusFile.type) {
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
