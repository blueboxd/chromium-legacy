// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wrapper around a file handle that allows the privileged context to arbitrate
 * read and write access as well as file navigation. `token` uniquely identifies
 * the file, `file` temporarily holds the object passed over postMessage, and
 * `handle` allows it to be reopened upon navigation. If an error occurred on
 * the last attempt to open `handle`, `lastError` holds the error name.
 * @typedef {{
 *     token: number,
 *     file: ?File,
 *     handle: !FileSystemFileHandle,
 *     lastError: (string|undefined),
 * }}
 */
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
  const {file, directory} =
      assertFileAndDirectoryMutable(deleteMsg.token, 'Delete');

  if (!(await isCurrentHandleInCurrentDirectory())) {
    return {deleteResult: DeleteResult.FILE_MOVED};
  }

  // Get the name from the file reference. Handles file renames.
  const currentFilename = (await file.handle.getFile()).name;

  await directory.removeEntry(currentFilename);

  // Remove the file that was deleted.
  currentFiles.splice(entryIndex, 1);

  // Attempts to load the file to the right which is at now at
  // `currentFiles[entryIndex]`, where `entryIndex` was previously the index of
  // the deleted file.
  await advance(0);

  return {deleteResult: DeleteResult.SUCCESS};
});

/** Handler to rename the currently focused file. */
guestMessagePipe.registerHandler(Message.RENAME_FILE, async (message) => {
  const renameMsg = /** @type{RenameFileMessage} */ (message);
  const {file, directory} =
      assertFileAndDirectoryMutable(renameMsg.token, 'Rename');

  if (await filenameExistsInCurrentDirectory(renameMsg.newFilename)) {
    return {renameResult: RenameResult.FILE_EXISTS};
  }

  const originalFile = await file.handle.getFile();
  const renamedFileHandle =
      await directory.getFile(renameMsg.newFilename, {create: true});
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
    await directory.removeEntry(originalFile.name);
  }

  // Reload current file so it is in an editable state, this is done before
  // removing the old file so the relaunch starts sooner.
  await launchWithDirectory(directory, renamedFileHandle);

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
  // Note there may be no currently writable file (e.g. save from clipboard).
  if (currentlyWritableFile &&
      await handle.isSameEntry(currentlyWritableFile.handle)) {
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
 * Warns if a given exception is "uncommon". That is, one that the guest might
 * not provide UX for and should be dumped to console to give additional
 * context.
 * @param {!DOMException} e
 * @param {string} fileName
 */
function warnIfUncommon(e, fileName) {
  if (e.name === 'NotFoundError' || e.name === 'NotAllowedError') {
    return;
  }
  console.warn(`Unexpected ${e.name} on ${fileName}: ${e.message}`);
}

/**
 * If `fd.file` is null, re-opens the file handle in `fd`.
 * @param {!FileDescriptor} fd
 */
async function refreshFile(fd) {
  if (fd.file) {
    return;
  }
  fd.lastError = '';
  try {
    fd.file = (await getFileFromHandle(fd.handle)).file;
  } catch (/** @type{!DOMException} */ e) {
    fd.lastError = e.name;
    // A failure here is only a problem for the "current" file (and that needs
    // to be handled in the unprivileged context), so ignore known errors.
    warnIfUncommon(e, fd.handle.name);
  }
}

/**
 * Loads the current file list into the guest, enabling writes.
 * @return {!Promise<undefined>}
 */
async function sendFilesToGuest() {
  // Before sending to guest ensure writableFileIndex is set to be writable,
  // also clear the old token.
  if (currentlyWritableFile) {
    currentlyWritableFile.token = -1;
  }
  if (currentFiles.length) {
    currentlyWritableFile = currentFiles[entryIndex];
    currentlyWritableFile.token = ++fileToken;
  } else {
    currentlyWritableFile = null;
  }

  return sendSnapshotToGuest([...currentFiles]);  // Shallow copy.
}

/**
 * Loads the provided file list into the guest without making any file writable.
 * @param {!Array<!FileDescriptor>} snapshot
 * @return {!Promise<undefined>}
 */
async function sendSnapshotToGuest(snapshot) {
  // On first launch, files are opened to determine navigation candidates. Don't
  // reopen in that case. Otherwise, attempt to reopen here. Some files may be
  // assigned null, e.g., if they have been moved to a different folder.
  await Promise.all(snapshot.map(refreshFile));

  /** @type {!LoadFilesMessage} */
  const loadFilesMessage = {
    writableFileIndex: entryIndex,
    // Handle can't be passed through a message pipe.
    files: snapshot.map(fd => ({
                          token: fd.token,
                          file: fd.file,
                          name: fd.handle.name,
                          error: fd.lastError,
                        }))
  };
  // Clear handles to the open files in the privileged context so they are
  // refreshed on a navigation request. The refcount to the File will be alive
  // in the postMessage object until the guest takes its own reference.
  for (const fd of snapshot) {
    fd.file = null;
  }
  await guestMessagePipe.sendMessage(Message.LOAD_FILES, loadFilesMessage);
}

/**
 * Throws an error if the file or directory handles don't exist or the token for
 * the file to be mutated is incorrect.
 * @param {number} editFileToken
 * @param {string} operation
 * @return {{file: !FileDescriptor, directory: !FileSystemDirectoryHandle}}
 */
function assertFileAndDirectoryMutable(editFileToken, operation) {
  if (!currentlyWritableFile || editFileToken !== fileToken) {
    throw new Error(`${operation} failed. File not current.`);
  }

  if (!currentDirectoryHandle) {
    throw new Error(`${operation} failed. File without launch directory.`);
  }
  return {file: currentlyWritableFile, directory: currentDirectoryHandle};
}

/**
 * Returns whether `currentlyWritableFile.handle` is in`currentDirectoryHandle`.
 * Prevents mutating the wrong file or a file that doesn't exist.
 * @return {!Promise<!boolean>}
 */
async function isCurrentHandleInCurrentDirectory() {
  if (!currentlyWritableFile) {
    return false;
  }
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
  if (!currentDirectoryHandle) {
    return null;
  }
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
 * Gets a file from a handle received via the fileHandling API. Only handles
 * expected to be files should be passed to this function. Throws a DOMException
 * if opening the file fails - usually because the handle is stale.
 * @param {?FileSystemHandle} fileSystemHandle
 * @return {!Promise<!{file: !File, handle: !FileSystemFileHandle}>}
 */
async function getFileFromHandle(fileSystemHandle) {
  if (!fileSystemHandle || !fileSystemHandle.isFile) {
    // Invent our own exception for this corner case. It might happen if a file
    // is deleted and replaced with a directory with the same name.
    throw new DOMException('Not a file.', 'NotAFile');
  }
  const handle = /** @type {!FileSystemFileHandle} */ (fileSystemHandle);
  const file = await handle.getFile();  // Note: throws DOMException.
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
  if (!focusFile || !focusFile.name) {
    return;
  }
  currentFiles.length = 0;
  for await (const /** !FileSystemHandle */ handle of directory.getEntries()) {
    if (!handle.isFile) {
      continue;
    }
    let entry = null;
    try {
      entry = await getFileFromHandle(handle);
    } catch (/** @type{!DOMException} */ e) {
      // Ignore exceptions thrown trying to open "other" files in the folder,
      // and skip adding that file to `currentFiles`.
      // Note the focusFile is passed in as `File`, so should be openable.
      warnIfUncommon(e, handle.name);
    }

    // Only allow traversal of related file types.
    if (entry && isFileRelated(focusFile, entry.file)) {
      currentFiles.push({token: -1, file: entry.file, handle: entry.handle});
    }
  }
  const name = focusFile.name;
  entryIndex = currentFiles.findIndex(i => !!i.file && i.file.name === name);
  currentDirectoryHandle = directory;
}

/**
 * Launch the media app with the files in the provided directory, using `handle`
 * as the initial launch entry.
 * @param {!FileSystemDirectoryHandle} directory
 * @param {!FileSystemHandle} handle
 */
async function launchWithDirectory(directory, handle) {
  let asFile;
  try {
    asFile = await getFileFromHandle(handle);
  } catch (/** @type{!DOMException} */ e) {
    console.warn(`${handle.name}: ${e.message}`);
    sendSnapshotToGuest([{token: -1, file: null, handle, error: e.name}]);
    return;
  }
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
  if (currentFiles.length) {
    entryIndex = (entryIndex + direction) % currentFiles.length;
    if (entryIndex < 0) {
      entryIndex += currentFiles.length;
    }
  } else {
    entryIndex = -1;
  }

  await sendFilesToGuest();
}

// Wait for 'load' (and not DOMContentLoaded) to ensure the subframe has been
// loaded and is ready to respond to postMessage.
window.addEventListener('load', () => {
  if (!window.launchQueue) {
    console.error('FileHandling API missing.');
    return;
  }
  window.launchQueue.setConsumer(params => {
    if (!params || !params.files || params.files.length < 2) {
      console.error('Invalid launch (missing files): ', params);
      return;
    }

    if (!assertCast(params.files[0]).isDirectory) {
      console.error('Invalid launch: files[0] is not a directory: ', params);
      return;
    }
    const directory =
        /** @type{!FileSystemDirectoryHandle} */ (params.files[0]);
    const focusEntry = assertCast(params.files[1]);
    launchWithDirectory(directory, focusEntry);
  });
});
