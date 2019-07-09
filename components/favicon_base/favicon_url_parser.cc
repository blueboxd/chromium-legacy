// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_url_parser.h"

#include "base/strings/string_number_conversions.h"
#include "components/favicon_base/favicon_types.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/favicon_size.h"

namespace chrome {

namespace {

// Returns true if |search| is a substring of |path| which starts at
// |start_index|.
bool HasSubstringAt(const std::string& path,
                    size_t start_index,
                    const std::string& search) {
  return path.compare(start_index, search.length(), search) == 0;
}

bool ParseIsIconUrl(const std::string& url_type, bool* is_icon_url) {
  if (url_type == "page_url") {
    *is_icon_url = false;
    return true;
  }

  if (url_type == "icon_url") {
    *is_icon_url = true;
    return true;
  }

  return false;
}

// Parse with legacy FaviconUrlFormat::kFavicon format.
bool ParseFaviconPathWithLegacyFormat(const std::string& path,
                                      chrome::ParsedFaviconPath* parsed) {
  // Parameters which can be used in chrome://favicon path. See file
  // "favicon_url_parser.h" for a description of what each one does.
  const char kIconURLParameter[] = "iconurl/";
  const char kSizeParameter[] = "size/";

  parsed->is_icon_url = false;
  parsed->url = "";
  parsed->size_in_dip = gfx::kFaviconSize;
  parsed->device_scale_factor = 1.0f;
  parsed->path_index = std::string::npos;
  // Use of the favicon server is never exposed for the legacy format (only used
  // by extensions).
  parsed->allow_favicon_server_fallback = false;

  if (path.empty())
    return false;

  size_t parsed_index = 0;
  if (HasSubstringAt(path, parsed_index, kSizeParameter)) {
    parsed_index += strlen(kSizeParameter);

    size_t slash = path.find("/", parsed_index);
    if (slash == std::string::npos)
      return false;

    size_t scale_delimiter = path.find("@", parsed_index);
    std::string size_str;
    std::string scale_str;
    if (scale_delimiter == std::string::npos) {
      // Support the legacy size format of 'size/aa/' where 'aa' is the desired
      // size in DIP for the sake of not regressing the extensions which use it.
      size_str = path.substr(parsed_index, slash - parsed_index);
    } else {
      size_str = path.substr(parsed_index, scale_delimiter - parsed_index);
      scale_str = path.substr(scale_delimiter + 1,
                              slash - scale_delimiter - 1);
    }

    if (!base::StringToInt(size_str, &parsed->size_in_dip))
      return false;

    if (!scale_str.empty())
      webui::ParseScaleFactor(scale_str, &parsed->device_scale_factor);

    parsed_index = slash + 1;
  }

  if (HasSubstringAt(path, parsed_index, kIconURLParameter)) {
    parsed_index += strlen(kIconURLParameter);
    parsed->is_icon_url = true;
    parsed->url = path.substr(parsed_index);
  } else {
    parsed->url = path.substr(parsed_index);
  }

  // The parsed index needs to be returned in order to allow Instant Extended
  // to translate favicon URLs using advanced parameters.
  // Example:
  //   "chrome-search://favicon/size/16@2x/<renderer-id>/<most-visited-id>"
  // would be translated to:
  //   "chrome-search://favicon/size/16@2x/<most-visited-item-with-given-id>".
  parsed->path_index = parsed_index;
  return true;
}

// Parse with FaviconUrlFormat::kFavicon2 format.
bool ParseFaviconPathWithFavicon2Format(const std::string& path,
                                        chrome::ParsedFaviconPath* parsed) {
  // Parameters which can be used in chrome://favicon2 path. See file
  // "favicon_url_parser.h" for a description of what each one does.
  const std::string kSizeParameter = "size";
  const std::string kScaleParameter = "scale_factor";
  const std::string kUrlTypeParameter = "url_type";
  const std::string kUrlParameter = "url";
  const std::string kAllowFallbackParameter = "allow_google_server_fallback";

  if (path.empty())
    return false;

  GURL query_url("chrome://favicon2/" + path);

  std::string size_str;
  if (!net::GetValueForKeyInQuery(query_url, kSizeParameter, &size_str))
    parsed->size_in_dip = gfx::kFaviconSize;
  else if (!base::StringToInt(size_str, &parsed->size_in_dip))
    return false;

  std::string scale_str;
  if (!net::GetValueForKeyInQuery(query_url, kScaleParameter, &scale_str))
    parsed->device_scale_factor = 1.0f;
  else if (!webui::ParseScaleFactor(scale_str, &parsed->device_scale_factor))
    return false;

  std::string url_type;
  if (!net::GetValueForKeyInQuery(query_url, kUrlTypeParameter, &url_type))
    parsed->is_icon_url = false;
  else if (!ParseIsIconUrl(url_type, &parsed->is_icon_url))
    return false;

  std::string url;
  if (!net::GetValueForKeyInQuery(query_url, kUrlParameter, &url))
    return false;
  parsed->url = url;

  if (parsed->is_icon_url) {
    // Fallback is never allowed for icon urls, since the server is queried by
    // page url.
    parsed->allow_favicon_server_fallback = false;
    return true;
  }

  // Check optional |kAllowFallbackParameter|.
  std::string allow_favicon_server_fallback;
  if (!net::GetValueForKeyInQuery(query_url, kAllowFallbackParameter,
                                  &allow_favicon_server_fallback)) {
    parsed->allow_favicon_server_fallback = false;
  } else if (!base::StringToInt(allow_favicon_server_fallback,
                                (int*)&parsed->allow_favicon_server_fallback)) {
    return false;
  }

  return true;
}

}  // namespace

bool ParseFaviconPath(const std::string& path,
                      FaviconUrlFormat format,
                      ParsedFaviconPath* parsed) {
  switch (format) {
    case FaviconUrlFormat::kFaviconLegacy:
      return ParseFaviconPathWithLegacyFormat(path, parsed);
    case FaviconUrlFormat::kFavicon2:
      return ParseFaviconPathWithFavicon2Format(path, parsed);
  }
  NOTREACHED();
  return false;
}

}  // namespace chrome
