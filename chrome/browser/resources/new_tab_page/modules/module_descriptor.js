// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides the module descriptor. Each module must create a
 * module descriptor and register it at the NTP.
 */

/**
 * @typedef {function(): !Promise<?{
 *    element: !HTMLElement,
 *    title: string,
 *   }>}
 */
let InitializeModuleCallback;

export class ModuleDescriptor {
  /**
   * @param {string} id
   * @param {string} name
   * @param {!InitializeModuleCallback} initializeCallback
   */
  constructor(id, name, initializeCallback) {
    this.id_ = id;
    this.name_ = name;
    this.title_ = null;
    this.element_ = null;
    this.initializeCallback_ = initializeCallback;
  }

  get id() {
    return this.id_;
  }

  get name() {
    return this.name_;
  }

  get title() {
    return this.title_;
  }

  get element() {
    return this.element_;
  }

  async initialize() {
    const info = await this.initializeCallback_();
    if (!info) {
      return;
    }
    this.title_ = info.title;
    this.element_ = info.element;
  }
}
