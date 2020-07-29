// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

const element = document.createElement('iron-iconset-svg');
element.name = 'signin';
element.innerHTML = `
<svg>
  <defs>
    <!-- Copied from iron-icons. -->
    <g id="work">
        <path d="M20 6h-4V4c0-1.11-.89-2-2-2h-4c-1.11 0-2 .89-2 2v2H4c-1.11 0-1.99.89-1.99 2L2 19c0 1.11.89 2 2 2h16c1.11 0 2-.89 2-2V8c0-1.11-.89-2-2-2zm-6 0h-4V4h4v2z">
        </path>
    </g>
  </defs>
</svg>`;
document.head.appendChild(element);
