// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor} from './module_descriptor.js';

/**
 * @fileoverview The module registry holds the descriptors of NTP modules and
 * provides management function such as instantiating the local module UIs.
 */

export class ModuleRegistry {
  /** @param {!Array<!ModuleDescriptor>} moduleDescriptors */
  constructor(moduleDescriptors) {
    /** @private {!Array<!ModuleDescriptor>} */
    this.moduleDescriptors_ = moduleDescriptors;
  }

  /**
   * Instantiates modules and appends them to |container|.
   * @param {!Element} container
   */
  async instantiateModules(container) {
    (await Promise.all(
         this.moduleDescriptors_.map(descriptor => descriptor.create())))
        .filter(module => !!module)
        .forEach(element => {
          container.appendChild(element);
        });
  }
}
