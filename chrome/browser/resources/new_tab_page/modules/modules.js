// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {dummyDescriptor, dummyDescriptor2} from './dummy/module.js';
import {kaleidoscopeDescriptor} from './kaleidoscope/module.js';
import {ModuleDescriptor} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';

/** @type {!Array<!ModuleDescriptor>} */
const descriptors = [dummyDescriptor, dummyDescriptor2];

if (loadTimeData.getBoolean('kaleidoscopeModuleEnabled')) {
  descriptors.push(kaleidoscopeDescriptor);
}

ModuleRegistry.getInstance().registerModules(descriptors);
