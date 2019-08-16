/* Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

if (window.location.href.startsWith("chrome://flags/enterprise")) {
  const styles = document.createElement('link');
  styles.rel = 'stylesheet';
  styles.href = 'chrome://flags/enterprise_flags.css';
  document.querySelector('head').appendChild(styles);
}
