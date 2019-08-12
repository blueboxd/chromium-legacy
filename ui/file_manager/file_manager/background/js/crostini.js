// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Implementation of Crostini shared path state handler.
 *
 * @constructor
 * @implements {Crostini}
 */
function CrostiniImpl() {
  /**
   * True if VM is enabled.
   * @private {Object<boolean>}
   */
  this.enabled_ = {};

  /**
   * Maintains a list of paths shared with VMs.
   * Keyed by entry.toURL(), maps to list of VMs.
   * @private @dict {!Object<!Array<string>>}
   */
  this.shared_paths_ = {};

  /**
   * True if root access to specified VM is allowed.
   * @private {Object<boolean>}
   */
  this.rootAccessAllowed_ = {};
}

/**
 * Default Crostini VM is 'termina'.
 * @const
 */
CrostiniImpl.DEFAULT_VM = 'termina';

/**
 * Plugin VM 'PvmDefault'.
 * @const
 */
CrostiniImpl.PLUGIN_VM = 'PvmDefault';

/**
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @type {!Map<VolumeManagerCommon.RootType, string>}
 * @const
 */
CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.DOWNLOADS, 'Downloads'],
  [VolumeManagerCommon.RootType.REMOVABLE, 'Removable'],
  [VolumeManagerCommon.RootType.ANDROID_FILES, 'AndroidFiles'],
]);

/**
 * Can be collapsed into VALID_ROOT_TYPES_FOR_SHARE once
 * DriveFS flag is removed.
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @type {!Map<VolumeManagerCommon.RootType, string>}
 * @const
 */
CrostiniImpl.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT, 'DriveComputers'],
  [VolumeManagerCommon.RootType.COMPUTER, 'DriveComputers'],
  [VolumeManagerCommon.RootType.DRIVE, 'MyDrive'],
  [VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT, 'TeamDrive'],
  [VolumeManagerCommon.RootType.SHARED_DRIVE, 'TeamDrive'],
]);

/**
 * @private {string}
 * @const
 */
CrostiniImpl.UMA_ROOT_TYPE_OTHER = 'Other';

/**
 * Initialize enabled settings.
 * Must be done after loadTimeData is available.
 */
CrostiniImpl.prototype.initEnabled = function() {
  this.enabled_[CrostiniImpl.DEFAULT_VM] =
      loadTimeData.getBoolean('CROSTINI_ENABLED');
  this.enabled_[CrostiniImpl.PLUGIN_VM] =
      loadTimeData.getBoolean('PLUGIN_VM_ENABLED');
  this.rootAccessAllowed_[CrostiniImpl.DEFAULT_VM] =
      loadTimeData.getBoolean('CROSTINI_ROOT_ACCESS_ALLOWED');
};

/**
 * Initialize Volume Manager.
 * @param {!VolumeManager} volumeManager
 */
CrostiniImpl.prototype.initVolumeManager = function(volumeManager) {
  this.volumeManager_ = volumeManager;
};

/**
 * Register for any shared path changes.
 */
CrostiniImpl.prototype.listen = function() {
  chrome.fileManagerPrivate.onCrostiniChanged.addListener(
      this.onCrostiniChanged_.bind(this));
};

/**
 * Set whether the specified VM is enabled.
 * @param {string} vmName
 * @param {boolean} enabled
 */
CrostiniImpl.prototype.setEnabled = function(vmName, enabled) {
  this.enabled_[vmName] = enabled;
};

/**
 * Returns true if crostini is enabled.
 * @param {string} vmName
 * @return {boolean}
 */
CrostiniImpl.prototype.isEnabled = function(vmName) {
  return this.enabled_[vmName];
};

/**
 * Set whether the specified VM allows root access.
 * @param {string} vmName
 * @param {boolean} allowed
 */
CrostiniImpl.prototype.setRootAccessAllowed = function(vmName, allowed) {
  this.rootAccessAllowed_[vmName] = allowed;
};

/**
 * Returns true if root access to specified VM is allowed.
 * @param {string} vmName
 * @return {boolean}
 */
CrostiniImpl.prototype.isRootAccessAllowed = function(vmName) {
  return this.rootAccessAllowed_[vmName];
};

/**
 * @param {!Entry} entry
 * @return {VolumeManagerCommon.RootType}
 * @private
 */
CrostiniImpl.prototype.getRoot_ = function(entry) {
  const info =
      this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
  return info && info.rootType;
};

/**
 * Registers an entry as a shared path for the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 */
CrostiniImpl.prototype.registerSharedPath = function(vmName, entry) {
  const url = entry.toURL();
  // Remove any existing paths that are children of the new path.
  // These paths will still be shared as a result of a parent path being
  // shared, but if the parent is unshared in the future, these children
  // paths should not remain.
  for (const [path, vms] of Object.entries(this.shared_paths_)) {
    if (path.startsWith(url)) {
      this.unregisterSharedPath_(vmName, path);
    }
  }
  const vms = this.shared_paths_[url];
  if (this.shared_paths_[url]) {
    this.shared_paths_[url].push(vmName);
  } else {
    this.shared_paths_[url] = [vmName];
  }

  // Record UMA.
  const root = this.getRoot_(entry);
  let suffix = CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE.get(root) ||
      CrostiniImpl.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE.get(root) ||
      CrostiniImpl.UMA_ROOT_TYPE_OTHER;
  metrics.recordSmallCount(
      'CrostiniSharedPaths.Depth.' + suffix,
      entry.fullPath.split('/').length - 1);
};

/**
 * Unregisters path as a shared path from the specified VM.
 * @param {string} vmName
 * @param {string} path
 * @private
 */
CrostiniImpl.prototype.unregisterSharedPath_ = function(vmName, path) {
  const vms = this.shared_paths_[path];
  if (vms) {
    const newVms = vms.filter(vm => vm != vmName);
    if (newVms.length > 0) {
      this.shared_paths_[path] = newVms;
    } else {
      delete this.shared_paths_[path];
    }
  }
};

/**
 * Unregisters entry as a shared path from the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 */
CrostiniImpl.prototype.unregisterSharedPath = function(vmName, entry) {
  this.unregisterSharedPath_(vmName, entry.toURL());
};

/**
 * Handles events for enable/disable, share/unshare.
 * @param {chrome.fileManagerPrivate.CrostiniEvent} event
 * @private
 */
CrostiniImpl.prototype.onCrostiniChanged_ = function(event) {
  switch (event.eventType) {
    case chrome.fileManagerPrivate.CrostiniEventType.ENABLE:
      this.setEnabled(event.vmName, true);
      break;
    case chrome.fileManagerPrivate.CrostiniEventType.DISABLE:
      this.setEnabled(event.vmName, false);
      break;
    case chrome.fileManagerPrivate.CrostiniEventType.ROOT_ACCESS_ALLOW:
      this.setRootAccessAllowed(event.vmName, true);
      break;
    case chrome.fileManagerPrivate.CrostiniEventType.ROOT_ACCESS_DISALLOW:
      this.setRootAccessAllowed(event.vmName, false);
      break;
    case chrome.fileManagerPrivate.CrostiniEventType.SHARE:
      for (const entry of event.entries) {
        this.registerSharedPath(event.vmName, entry);
      }
      break;
    case chrome.fileManagerPrivate.CrostiniEventType.UNSHARE:
      for (const entry of event.entries) {
        this.unregisterSharedPath(event.vmName, entry);
      }
      break;
  }
};

/**
 * Returns true if entry is shared with the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 * @return {boolean} True if path is shared either by a direct
 *   share or from one of its ancestor directories.
 */
CrostiniImpl.prototype.isPathShared = function(vmName, entry) {
  // Check path and all ancestor directories.
  let path = entry.toURL();
  let root = path;
  if (entry && entry.filesystem && entry.filesystem.root) {
    root = entry.filesystem.root.toURL();
  }

  while (path.length > root.length) {
    const vms = this.shared_paths_[path];
    if (vms && vms.includes(vmName)) {
      return true;
    }
    path = path.substring(0, path.lastIndexOf('/'));
  }
  const rootVms = this.shared_paths_[root];
  return !!rootVms && rootVms.includes(vmName);
};

/**
 * Returns true if entry can be shared with the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 * @param {boolean} persist If path is to be persisted.
 */
CrostiniImpl.prototype.canSharePath = function(vmName, entry, persist) {
  if (!this.enabled_[vmName]) {
    return false;
  }

  // Only directories for persistent shares.
  if (persist && !entry.isDirectory) {
    return false;
  }

  const root = this.getRoot_(entry);

  // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
  // Disallow Computers Grand Root, and Computer Root.
  if (root === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
      (root === VolumeManagerCommon.RootType.COMPUTER &&
       entry.fullPath.split('/').length <= 3)) {
    return false;
  }

  // TODO(crbug.com/958840): Sharing Play files root is disallowed until
  // we can ensure it will not also share Downloads.
  if (root === VolumeManagerCommon.RootType.ANDROID_FILES &&
      entry.fullPath === '/') {
    return false;
  }

  // Special case to disallow PluginVm sharing on /MyFiles/PluginVm and
  // subfolders since it gets shared by default.
  if (vmName === CrostiniImpl.PLUGIN_VM &&
      root === VolumeManagerCommon.RootType.DOWNLOADS &&
      entry.fullPath.split('/')[1] === CrostiniImpl.PLUGIN_VM) {
    return false;
  }

  return CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE.has(root) ||
      (loadTimeData.getBoolean('DRIVE_FS_ENABLED') &&
       CrostiniImpl.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE.has(root));
};
