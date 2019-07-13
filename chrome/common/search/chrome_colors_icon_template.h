// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_CHROME_COLORS_ICON_TEMPLATE_H_
#define CHROME_COMMON_SEARCH_CHROME_COLORS_ICON_TEMPLATE_H_

// Template for the icon svg.
// $1 - primary color
// $2 - secondary color
const char kChromeColorsIconTemplate[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><!DOCTYPE svg "
    "PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
    "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"><svg version=\"1.1\" "
    "xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
    "preserveAspectRatio=\"xMidYMid meet\" viewBox=\"0 0 80 80\" width=\"80\" "
    "height=\"80\"><defs><path d=\"M80 40C80 62.08 62.07 80 40 80C17.92 80 0 "
    "62.08 0 40C0 17.93 17.92 0 40 0C62.07 0 80 17.93 80 40Z\" "
    "id=\"a2GlMRZYm7\"></path><linearGradient id=\"gradientb48881kEO\" "
    "gradientUnits=\"userSpaceOnUse\" x1=\"40\" y1=\"40\" x2=\"40.1\" "
    "y2=\"40\"><stop style=\"stop-color: $1;stop-opacity: 1\" "
    "offset=\"0\%\"></stop><stop style=\"stop-color: $2;stop-opacity: 1\" "
    "offset=\"100\%\"></stop></linearGradient></defs><g><use "
    "xlink:href=\"#a2GlMRZYm7\" opacity=\"1\" "
    "fill=\"url(#gradientb48881kEO)\"></use></g></svg>";

#endif  // CHROME_COMMON_SEARCH_CHROME_COLORS_ICON_TEMPLATE_H_
