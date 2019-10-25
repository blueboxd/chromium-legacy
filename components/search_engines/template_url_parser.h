// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PARSER_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PARSER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"

class SearchTermsData;
class TemplateURL;

// TemplateURLParser, as the name implies, handling reading of TemplateURLs
// from OpenSearch description documents.
class TemplateURLParser {
 public:
  // A ParameterFilter is called for every URL paramter encountered during
  // Parse(). It passes the parameter key as the first argument and the value
  // as the second. The callback should return true if the parameter should be
  // kept, and false if it should be discarded.
  using ParameterFilter =
      base::RepeatingCallback<bool(const std::string&, const std::string&)>;

  // Decodes the chunk of data representing a TemplateURL, creates the
  // TemplateURL, and returns it.  Returns null if the data does not describe a
  // valid TemplateURL, the URLs referenced do not point to valid http/https
  // resources, or for some other reason we do not support the described
  // TemplateURL.  |parameter_filter| can be used if you want to filter some
  // parameters out of the URL.  For example, when importing from another
  // browser, we remove any parameter identifying that browser.  If set to null,
  // the URL is not modified.
  static std::unique_ptr<TemplateURL> Parse(
      const SearchTermsData& search_terms_data,
      const char* data,
      size_t length,
      const ParameterFilter& parameter_filter);

 private:
  // No one should create one of these.
  TemplateURLParser();

  DISALLOW_COPY_AND_ASSIGN(TemplateURLParser);
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PARSER_H_
