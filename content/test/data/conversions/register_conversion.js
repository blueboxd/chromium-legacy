// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function registerConversion(data) {
  let img = document.createElement("img");
  img.src =
      "server-redirect?.well-known/register-conversion?conversion-data=" +
      data;
  document.body.appendChild(img);
}

function createTrackingPixel(url) {
  let img = document.createElement("img");
  img.src = url;
  document.body.appendChild(img);
}
