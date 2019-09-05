/* Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

if (window.location.href.startsWith("chrome://flags/deprecated")) {
  const styles = document.createElement('link');
  styles.rel = 'stylesheet';
  styles.href = 'chrome://flags/flags_deprecated.css';
  document.querySelector('head').appendChild(styles);
}
