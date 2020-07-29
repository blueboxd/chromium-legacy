// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function registerConversion(data) {
  // Use a relative URL for conversion registration.
  registerConversionForOrigin(data, "");
}

function registerConversionForOrigin(data, origin) {
  let img = document.createElement("img");
  img.src = origin +
      "/server-redirect?.well-known/register-conversion?conversion-data=" +
      data;
  img.onerror = function () { document.title = "converted"; };
  document.body.appendChild(img);
}

function createTrackingPixel(url) {
  let img = document.createElement("img");
  img.src = url;
  document.body.appendChild(img);
}
