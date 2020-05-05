// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {assert} from '../chrome_util.js';
import {Filenamer, IMAGE_PREFIX, VIDEO_PREFIX} from './filenamer.js';

/**
 * The prefix of thumbnail files.
 * @type {string}
 */
const THUMBNAIL_PREFIX = 'thumb-';

/**
 * Checks if the entry's name has the video prefix.
 * @param {!FileEntry} entry File entry.
 * @return {boolean} Has the video prefix or not.
 */
export function hasVideoPrefix(entry) {
  return entry.name.startsWith(VIDEO_PREFIX);
}

/**
 * Checks if the entry's name has the image prefix.
 * @param {!FileEntry} entry File entry.
 * @return {boolean} Has the image prefix or not.
 */
function hasImagePrefix(entry) {
  return entry.name.startsWith(IMAGE_PREFIX);
}

/**
 * Checks if the entry's name has the thumbnail prefix.
 * @param {!FileEntry} entry File entry.
 * @return {boolean} Has the thumbnail prefix or not.
 */
function hasThumbnailPrefix(entry) {
  return entry.name.startsWith(THUMBNAIL_PREFIX);
}

/**
 * Directory in the internal file system.
 * @type {?DirectoryEntry}
 */
let internalDir = null;

/**
 * Temporary directory in the internal file system.
 * @type {?DirectoryEntry}
 */
let internalTempDir = null;

/**
 * Directory in the external file system.
 * @type {?DirectoryEntry}
 */
let externalDir = null;

/**
 * Gets global external directory used by CCA.
 * @return {?DirectoryEntry}
 */
export function getExternalDirectory() {
  return externalDir;
}

/**
 * Initializes the directory in the internal file system.
 * @return {!Promise<!DirectoryEntry>} Promise for the directory result.
 */
function initInternalDir() {
  return new Promise((resolve, reject) => {
    webkitRequestFileSystem(
        window.PERSISTENT, 768 * 1024 * 1024 /* 768MB */,
        (fs) => resolve(fs.root), reject);
  });
}

/**
 * Initializes the temporary directory in the internal file system.
 * @return {!Promise<!DirectoryEntry>} Promise for the directory result.
 */
function initInternalTempDir() {
  return new Promise((resolve, reject) => {
    webkitRequestFileSystem(
        window.TEMPORARY, 768 * 1024 * 1024 /* 768MB */,
        (fs) => resolve(fs.root), reject);
  });
}

/**
 * Reads file entries from the directory.
 * @param {?DirectoryEntry} dir Directory entry to be read.
 * @return {!Promise<!Array<!Entry>>} Promise for the read file entries.
 */
function readDir(dir) {
  return !dir ? Promise.resolve([]) : new Promise((resolve, reject) => {
    const dirReader = dir.createReader();
    const entries = [];
    const readEntries = () => {
      dirReader.readEntries((inEntries) => {
        if (inEntries.length === 0) {
          resolve(entries);
          return;
        }
        entries.push(...inEntries);
        readEntries();
      }, reject);
    };
    readEntries();
  });
}

/**
 * Initializes the directory in the external file system.
 * @return {!Promise<?DirectoryEntry>} Promise for the directory result.
 */
async function initExternalDir() {
  const volumes = await browserProxy.getVolumeList();
  if (volumes === null) {
    return null;
  }

  const getFileSystemRoot = async (volume) => {
    const fs = await browserProxy.requestFileSystem(volume);
    return fs === null ? null : fs.root;
  };

  for (const volume of volumes) {
    if (!volume.volumeId.includes('downloads:MyFiles')) {
      continue;
    }
    const root = await getFileSystemRoot(volume);
    const entries = await readDir(root);
    const downloadsDir = entries.find(
        (entry) => entry.name === 'Downloads' && entry.isDirectory);
    return downloadsDir ? /** @type {!DirectoryEntry} */ (downloadsDir) : null;
  }
  return null;
}

/**
 * Regulates the picture name to the desired format if it's in legacy formats.
 * @param {!FileEntry} entry Picture entry whose name to be regulated.
 * @return {string} Name in the desired format.
 */
function regulatePictureName(entry) {
  if (hasVideoPrefix(entry) || hasImagePrefix(entry)) {
    const match = entry.name.match(/(\w{3}_\d{8}_\d{6})(?:_(\d+))?(\..+)?$/);
    if (match) {
      const idx = match[2] ? ' (' + match[2] + ')' : '';
      const ext = match[3] ? match[3].replace(/\.webm$/, '.mkv') : '';
      return match[1] + idx + ext;
    }
  } else {
    // Early pictures are in legacy file name format (crrev.com/c/310064).
    const match = entry.name.match(/(\d+).(?:\d+)/);
    if (match) {
      return (new Filenamer(parseInt(match[1], 10))).newImageName();
    }
  }
  return entry.name;
}

/**
 * Gets the thumbnail name of the given picture.
 * @param {!FileEntry} entry Picture's file entry.
 * @return {string} Thumbnail name.
 */
function getThumbnailName(entry) {
  const thumbnailName = THUMBNAIL_PREFIX + entry.name;
  return (thumbnailName.substr(0, thumbnailName.lastIndexOf('.')) ||
          thumbnailName) +
      '.jpg';
}

/**
 * Parses and filters the internal entries to thumbnail and picture entries.
 * @param {!Array<!FileEntry>} internalEntries Internal file entries.
 * @param {!Object<string, !FileEntry>} thumbnailEntriesByName Result thumbanil
 *     entries mapped by thumbnail names, initially empty.
 * @param {!Array<!FileEntry>=} pictureEntries Result picture entries, initially
 *     empty.
 */
function parseInternalEntries(
    internalEntries, thumbnailEntriesByName, pictureEntries) {
  let thumbnailEntries = [];
  if (pictureEntries) {
    for (let index = 0; index < internalEntries.length; index++) {
      if (hasThumbnailPrefix(internalEntries[index])) {
        thumbnailEntries.push(internalEntries[index]);
      } else {
        pictureEntries.push(internalEntries[index]);
      }
    }
  } else {
    thumbnailEntries = internalEntries.filter(hasThumbnailPrefix);
  }
  for (let index = 0; index < thumbnailEntries.length; index++) {
    const thumbnailEntry = thumbnailEntries[index];
    thumbnailEntriesByName[thumbnailEntry.name] = thumbnailEntry;
  }
}

/**
 * Migrates all picture-files from internal storage to external storage.
 * @return {!Promise} Promise for the operation.
 */
function migratePictures() {
  const migratePicture = (pictureEntry, thumbnailEntry) => {
    const name = regulatePictureName(pictureEntry);
    const targetDir = externalDir;
    assert(targetDir !== null);
    return getFile(targetDir, name, true).then((entry) => {
      return new Promise((resolve, reject) => {
        pictureEntry.copyTo(targetDir, entry.name, (result) => {
          if (result.name !== pictureEntry.name && thumbnailEntry) {
            // Thumbnails can be recreated later if failing to rename them here.
            thumbnailEntry.moveTo(internalDir, getThumbnailName(result));
          }
          pictureEntry.remove(() => {});
          resolve();
        }, reject);
      });
    });
  };

  return readDir(internalDir).then((internalEntries) => {
    const pictureEntries = [];
    const thumbnailEntriesByName = {};
    parseInternalEntries(
        internalEntries, thumbnailEntriesByName, pictureEntries);

    const migrated = [];
    for (let index = 0; index < pictureEntries.length; index++) {
      const entry = pictureEntries[index];
      const thumbnailName = getThumbnailName(entry);
      const thumbnailEntry = thumbnailEntriesByName[thumbnailName];
      migrated.push(migratePicture(entry, thumbnailEntry));
    }
    return Promise.all(migrated);
  });
}

/**
 * Initializes file systems, migrating pictures if needed. This function
 * should be called only once in the beginning of the app.
 * @param {function()} promptMigrate Callback to instantiate a promise that
       prompts users to migrate pictures if no acknowledgement yet.
 * @return {!Promise<boolean>} Promise for the external-fs result.
 */
export function initialize(promptMigrate) {
  const checkAcked = new Promise((resolve) => {
    // ack 0: User has not yet acknowledged to migrate pictures.
    // ack 1: User acknowledges to migrate pictures to Downloads.
    browserProxy.localStorageGet({ackMigratePictures: 0})
        .then((values) => resolve(values.ackMigratePictures >= 1));
  });
  const ackMigrate = () =>
      browserProxy.localStorageSet({ackMigratePictures: 1});
  const doneMigrate = () => browserProxy.doneMigrate();

  return Promise
      .all([
        initInternalDir(),
        initInternalTempDir(),
        initExternalDir(),
        checkAcked,
        browserProxy.checkMigrated(),
      ])
      .then((results) => {
        let /** boolean */ acked;
        let /** boolean */ migrated;
        [internalDir, internalTempDir, externalDir, acked, migrated] = results;
        if (migrated && !externalDir) {
          throw new Error('External file system should be available.');
        }
        // Check if acknowledge-prompt and migrate-pictures are needed.
        if (migrated || !externalDir) {
          return [false, false];
        }
        // Check if any internal picture other than thumbnail needs migration.
        // Pictures taken by old Camera App may not have IMG_ or VID_ prefix.
        return readDir(internalDir)
            .then((entries) => {
              return entries.some((entry) => {
                if (entry.isDirectory) {
                  return false;
                }
                const fileEntry = /** @type {!FileEntry} */ (entry);
                return !hasThumbnailPrefix(fileEntry);
              });
            })
            .then((migrateNeeded) => {
              if (migrateNeeded) {
                return [!acked, true];
              }
              // If the external file system is supported and there is already
              // no picture in the internal file system, it implies done
              // migration and then doesn't need acknowledge-prompt.
              ackMigrate();
              doneMigrate();
              return [false, false];
            });
      })
      .then(
          ([promptNeeded, migrateNeeded]) => {  // Prompt to migrate if needed.
            return !promptNeeded ? migrateNeeded : promptMigrate().then(() => {
              ackMigrate();
              return migrateNeeded;
            });
          })
      .then((migrateNeeded) => {  // Migrate pictures if needed.
        const external = externalDir !== null;
        return !migrateNeeded ?
            external :
            migratePictures().then(doneMigrate).then(() => external);
      });
}

/**
 * Saves the blob to the given file name. Name of the actually saved file
 * might be different from the given file name if the file already exists.
 * @param {!DirectoryEntry} dir Directory to be written into.
 * @param {string} name Name of the file.
 * @param {!Blob} blob Data of the file to be saved.
 * @return {!Promise<?FileEntry>} Promise for the result.
 * @private
 */
function saveToFile(dir, name, blob) {
  return getFile(dir, name, true).then((entry) => {
    return new Promise((resolve, reject) => {
      entry.createWriter((fileWriter) => {
        fileWriter.onwriteend = () => resolve(entry);
        fileWriter.onerror = reject;
        fileWriter.write(blob);
      }, reject);
    });
  });
}

/**
 * Saves photo blob or metadata blob into predefined default location.
 * @param {!Blob} blob Data of the photo to be saved.
 * @param {string} filename Filename of the photo to be saved.
 * @return {!Promise<?FileEntry>} Promise for the result.
 */
export function saveBlob(blob, filename) {
  const dir = externalDir || internalDir;
  assert(dir !== null);
  return saveToFile(dir, filename, blob);
}

/**
 * Gets metadata of the file.
 * @param {!FileEntry} file
 * @return {!Promise<!Object>}
 */
export function getMetadata(file) {
  return new Promise((resolve) => file.getMetadata(resolve));
}

/**
 * Gets FileWriter of the file.
 * @param {!FileEntry} file
 * @return {!Promise<!FileWriter>}
 */
export function getFileWriter(file) {
  return new Promise((resolve, reject) => file.createWriter(resolve, reject));
}

/**
 * Creates a file for saving temporary video recording result.
 * @return {!Promise<!FileEntry>} Newly created temporary file.
 * @throws {Error} If failed to create video temp file.
 */
export async function createTempVideoFile() {
  const dir = externalDir || internalDir;
  assert(dir !== null);
  const filename = new Filenamer().newVideoName();
  const file = await getFile(dir, filename, true);
  if (file === null) {
    throw new Error('Failed to create video temp file.');
  }
  return file;
}

/**
 * @type {string}
 */
const PRIVATE_TEMPFILE_NAME = 'video-intent.mkv';

/**
 * @return {!Promise<!FileEntry>} Newly created temporary file.
 * @throws {Error} If failed to create video temp file.
 */
export async function createPrivateTempVideoFile() {
  // TODO(inker): Handles running out of space case.
  const dir = internalTempDir;
  assert(dir !== null);
  const file = await getFile(dir, PRIVATE_TEMPFILE_NAME, true);
  if (file === null) {
    throw new Error('Failed to create private video temp file.');
  }
  return file;
}

/**
 * Saves temporary video file to predefined default location.
 * @param {!FileEntry} tempfile Temporary video file to be saved.
 * @param {string} filename Filename to be saved.
 * @return {!Promise<!FileEntry>} Saved video file.
 */
export async function saveVideo(tempfile, filename) {
  const dir = externalDir || internalDir;
  assert(dir !== null);

  // Non-null version for the Closure Compiler.
  const nonNullDir = dir;

  // Assuming content of tempfile contains all recorded chunks appended together
  // and is a well-formed video. The work needed here is just to move the file
  // to the correct directory and rename as the specified filename.
  if (tempfile.name === filename) {
    return tempfile;
  }
  return new Promise(
      (resolve, reject) =>
          tempfile.moveTo(nonNullDir, filename, resolve, reject));
}

/**
 * Increments the file index of a given file name to avoid name conflicts.
 * @param {string} name File name.
 * @return {string} File name with incremented index.
 */
function incrementFileName(name) {
  let base = '';
  let ext = '';
  let idx = 0;
  let match = name.match(/^([^.]+)(\..+)?$/);
  if (match) {
    base = match[1];
    ext = match[2];
    match = base.match(/ \((\d+)\)$/);
    if (match) {
      base = base.substring(0, match.index);
      idx = parseInt(match[1], 10);
    }
  }
  return base + ' (' + (idx + 1) + ')' + ext;
}

/**
 * Gets the file by the given name, avoiding name conflicts if necessary.
 * @param {!DirectoryEntry} dir Directory to get the file from.
 * @param {string} name File name. Result file may have a different name.
 * @param {boolean} create True to create file, false otherwise.
 * @return {!Promise<?FileEntry>} Promise for the result.
 */
export function getFile(dir, name, create) {
  return new Promise((resolve, reject) => {
           const options =
               create ? {create: true, exclusive: true} : {create: false};
           dir.getFile(name, options, resolve, reject);
         })
      .catch((error) => {
        if (create && error.name === 'InvalidModificationError') {
          // Avoid name conflicts for creating files.
          return getFile(dir, incrementFileName(name), create);
        } else if (!create && error.name === 'NotFoundError') {
          return null;
        }
        throw error;
      });
}

/**
 * Gets the picture entries.
 * @return {!Promise<!Array<!FileEntry>>} Promise for the picture entries.
 */
export function getEntries() {
  return readDir(externalDir).then((entries) => {
    return entries.filter((entry) => {
      if (entry.isDirectory) {
        return false;
      }
      const fileEntry = /** @type {!FileEntry} */ (entry);
      if (!hasVideoPrefix(fileEntry) && !hasImagePrefix(fileEntry)) {
        return false;
      }
      return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
    });
  });
}

/**
 * Returns an URL for a picture.
 * @param {!FileEntry} entry File entry.
 * @param {number} limit Size limit. The file would be truncated if it's larger
 *     than limit.
 * @return {!Promise<string>} Promise for the result.
 */
export function pictureURL(entry, limit) {
  return new Promise((resolve) => {
    entry.file((file) => {
      if (file.size > limit) {
        file = file.slice(0, limit);
      }
      const url = URL.createObjectURL(file);
      resolve(url);
    });
  });
}
