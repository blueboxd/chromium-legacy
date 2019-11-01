// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './shared_vars.js';

const template = document.createElement('template');
template.innerHTML = `{__html_template__}`;
document.body.appendChild(template.content.cloneNode(true));
