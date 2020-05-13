// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer-error-screen.js';
import './elements/viewer-password-screen.js';
import './elements/viewer-pdf-toolbar.js';
import './elements/viewer-zoom-toolbar.js';
import './elements/shared-vars.js';
// <if expr="chromeos">
import './elements/viewer-ink-host.js';
import './elements/viewer-form-warning.js';
// </if>

import {main} from './main_util.js';

main();
