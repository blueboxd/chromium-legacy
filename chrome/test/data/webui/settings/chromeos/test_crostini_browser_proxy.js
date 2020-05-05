// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.CrostiniBrowserProxy} */
class TestCrostiniBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestCrostiniInstallerView',
      'requestRemoveCrostini',
      'getCrostiniSharedPathsDisplayText',
      'getCrostiniSharedUsbDevices',
      'setCrostiniUsbDeviceShared',
      'removeCrostiniSharedPath',
      'exportCrostiniContainer',
      'importCrostiniContainer',
      'requestCrostiniContainerUpgradeView',
      'requestCrostiniUpgraderDialogStatus',
      'requestCrostiniContainerUpgradeAvailable',
      'addCrostiniPortForward',
      'getCrostiniDiskInfo',
      'resizeCrostiniDisk',
      'checkCrostiniMicSharingStatus',
      'addCrostiniPortForward',
      'removeCrostiniPortForward',
      'removeAllCrostiniPortForwards',
      'activateCrostiniPortForward',
      'deactivateCrostiniPortForward',
      'getCrostiniActivePorts',
      'checkCrostiniIsRunning',
    ]);
    this.sharedUsbDevices = [];
    this.removeSharedPathResult = true;
    this.methodCalls_ = {};
  }

  getNewPromiseFor(name) {
    return new Promise((resolve, reject) => {
      this.methodCalls_[name] = {name, resolve, reject};
    });
  }

  async resolvePromise(name, ...args) {
    await this.methodCalls_[name].resolve(...args);
  }

  async rejectPromise(name, ...args) {
    await this.methodCalls_[name].reject(...args);
  }

  /** @override */
  requestCrostiniInstallerView() {
    this.methodCalled('requestCrostiniInstallerView');
  }

  /** override */
  requestRemoveCrostini() {
    this.methodCalled('requestRemoveCrostini');
  }

  /** override */
  getCrostiniSharedPathsDisplayText(paths) {
    this.methodCalled('getCrostiniSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** @override */
  getCrostiniSharedUsbDevices() {
    this.methodCalled('getCrostiniSharedUsbDevices');
    return Promise.resolve(this.sharedUsbDevices);
  }

  /** @override */
  setCrostiniUsbDeviceShared(guid, shared) {
    this.methodCalled('setCrostiniUsbDeviceShared', [guid, shared]);
  }

  /** override */
  removeCrostiniSharedPath(vmName, path) {
    this.methodCalled('removeCrostiniSharedPath', [vmName, path]);
    return Promise.resolve(this.removeSharedPathResult);
  }

  /** @override */
  requestCrostiniInstallerStatus() {
    cr.webUIListenerCallback('crostini-installer-status-changed', false);
  }

  /** @override */
  requestCrostiniExportImportOperationStatus() {
    cr.webUIListenerCallback(
        'crostini-export-import-operation-status-changed', false);
  }

  /** override */
  exportCrostiniContainer() {
    this.methodCalled('exportCrostiniContainer');
  }

  /** override */
  importCrostiniContainer() {
    this.methodCalled('importCrostiniContainer');
  }

  /** @override */
  requestCrostiniContainerUpgradeView() {
    this.methodCalled('requestCrostiniContainerUpgradeView');
  }

  /** @override */
  requestCrostiniUpgraderDialogStatus() {
    cr.webUIListenerCallback('crostini-upgrader-status-changed', false);
  }

  /** @override */
  requestCrostiniContainerUpgradeAvailable() {
    cr.webUIListenerCallback(
        'crostini-container-upgrade-available-changed', true);
  }

  /** @override */
  addCrostiniPortForward(
      vmName, containerName, portNumber, protocolIndex, label) {
    this.methodCalled(
        'addCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex, label);
    return Promise.resolve(true);
  }

  /** @override */
  removeCrostiniPortForward(vmName, containerName, portNumber, protocolIndex) {
    this.methodCalled(
        'removeCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex);
    return Promise.resolve(true);
  }

  /** @override */
  activateCrostiniPortForward(
      vmName, containerName, portNumber, protocolIndex) {
    this.methodCalled(
        'activateCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex);
    return Promise.resolve(true);
  }

  /** @override */
  removeAllCrostiniPortForwards(vmName, containerName) {
    this.methodCalled('removeAllCrostiniPortForwards');
  }

  /** @override */
  getCrostiniDiskInfo(vmName, requestFullInfo) {
    this.methodCalled('getCrostiniDiskInfo', vmName, requestFullInfo);
    return this.getNewPromiseFor('getCrostiniDiskInfo');
  }

  /** @override */
  resizeCrostiniDisk(vmName, newSizeBytes) {
    this.methodCalled('resizeCrostiniDisk', vmName, newSizeBytes);
    return this.getNewPromiseFor('resizeCrostiniDisk');
  }

  /** @override */
  checkCrostiniMicSharingStatus(proposedValue) {
    this.methodCalled('checkCrostiniMicSharingStatus', proposedValue);
    return Promise.resolve(!proposedValue);
  }

  /** @override */
  deactivateCrostiniPortForward(
      vmName, containerName, portNumber, protocolIndex) {
    this.methodCalled(
        'deactivateCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex);
    return Promise.resolve(true);
  }

  /** @override */
  getCrostiniActivePorts() {
    this.methodCalled('getCrostiniActivePorts');
    return Promise.resolve(new Array());
  }

  /** @override */
  checkCrostiniIsRunning() {
    this.methodCalled('checkCrostiniIsRunning');
    return Promise.resolve(true);
  }
}
