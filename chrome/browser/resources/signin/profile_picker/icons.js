// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

const element = document.createElement('iron-iconset-svg');
element.name = 'profiles';
element.innerHTML = `
<svg>
  <defs>
    <g id="add" viewBox="0 0 74 74">
      <circle cx="37" cy="37" r="37" stroke="none"/>
      <path d="M36.9999 46.4349C36.1315 46.4349 35.4274 45.7309 35.4274 44.8624V38.5724H29.1374C28.269 38.5724 27.5649 37.8684 27.5649 36.9999C27.5649 36.1315 28.269 35.4274 29.1374 35.4274H35.4274V29.1374C35.4274 28.269 36.1315 27.5649 36.9999 27.5649C37.8684 27.5649 38.5724 28.269 38.5724 29.1374V35.4274H44.8624C45.7309 35.4274 46.4349 36.1315 46.4349 36.9999C46.4349 37.8684 45.7309 38.5724 44.8624 38.5724H38.5724V44.8624C38.5724 45.7309 37.8684 46.4349 36.9999 46.4349Z" fill="var(--iron-icon-stroke-color)"/>
    </g>

    <g id="account-circle" viewBox="0 0 24 24" fill="var(--footer-text-color)" width="18px" height="18px">
      <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z"/>
    </g>
  </defs>
</svg>`;
document.head.appendChild(element);
