// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_URL_PARSER_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_URL_PARSER_H_

#include <stddef.h>

#include <string>

namespace chrome {

struct ParsedFaviconPath {
  // Whether the URL has the "iconurl" parameter.
  bool is_icon_url;

  // The URL from which the favicon is being requested.
  std::string url;

  // The size of the requested favicon in dip.
  int size_in_dip;

  // The device scale factor of the requested favicon.
  float device_scale_factor;

  // TODO(victorvianna): Remove this parameter.
  // The index of the first character (relative to the path) where the the URL
  // from which the favicon is being requested is located.
  size_t path_index;

  // Whether we should allow making a request to the favicon server as fallback.
  bool allow_favicon_server_fallback;
};

// Enum describing the two possible url formats: the legacy chrome://favicon
// and chrome://favicon2.
// - chrome://favicon format:
//   chrome://favicon/size&scalefactor/iconurl/url
// Some parameters are optional as described below. However, the order of the
// parameters is not interchangeable.
//
// Parameter:
//  'url'               Required
//    Specifies the page URL of the requested favicon. If the 'iconurl'
//    parameter is specified, the URL refers to the URL of the favicon image
//    instead.
//  'size&scalefactor'  Optional
//    Values: ['size/aa@bx/']
//      Specifies the requested favicon's size in DIP (aa) and the requested
//      favicon's scale factor. (b).
//      The supported requested DIP sizes are: 16x16, 32x32 and 64x64.
//      If the parameter is unspecified, the requested favicon's size defaults
//      to 16 and the requested scale factor defaults to 1x.
//      Example: chrome://favicon/size/16@2x/https://www.google.com/
//  'iconurl'           Optional
//    Values: ['iconurl']
//    'iconurl': Specifies that the url parameter refers to the URL of
//    the favicon image as opposed to the URL of the page that the favicon is
//    on.
//    Example: chrome://favicon/iconurl/https://www.google.com/favicon.ico
//
// - chrome://favicon2 format:
//   chrome://favicon2/?query_parameters
// Standard URL query parameters are used as described below.
//
// Parameter:
//  'url' Required
//    The url whose favicon we want.
//  'url_type' Optional
//   Values: ['icon_url', 'page_url']
//    Specifies whether the |url| parameter refers to the URL of the favicon
//    image directly or to the page whose favicon we want (respectively). If
//    unspecified, defaults to 'page_url'.
//  'size'  Optional
//      Specifies the requested favicon's size in DIP. If unspecified, defaults
//      to 16.
//    Example: chrome://favicon2/?size=32
// TODO(victorvianna): Refactor to remove scale_factor parameter.
//  'scale_factor'  Optional
//      Values: ['SCALEx']
//      Specifies the requested favicon's scale factor. If unspecified, defaults
//      to 1x.
//    Example: chrome://favicon2/?scale_factor=1.2x
//
//  In case |url_type| == 'page_url', we can specify an additional parameter:
//  'allow_google_server_fallback' Optional
//      Values: ['1', '0']
//      Specifies whether we are allowed to fall back to an external server
//      request in case the icon is not found locally.
enum class FaviconUrlFormat {
  // Legacy chrome://favicon format.
  kFaviconLegacy,
  // chrome://favicon2 format.
  kFavicon2,
};

// Parses |path| according to |format|, returning true if successful. The result
// of the parsing will be stored in the struct pointed by |parsed|.
bool ParseFaviconPath(const std::string& path,
                      FaviconUrlFormat format,
                      ParsedFaviconPath* parsed);

}  // namespace chrome

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_URL_PARSER_H_
