// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
#define FUCHSIA_ENGINE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<mojom::UrlRequestRulePtr,
                     fuchsia::web::UrlRequestRewriteRule> {
  static mojom::UrlRequestRulePtr Convert(
      const fuchsia::web::UrlRequestRewriteRule& input);
};

}  // namespace mojo

#endif  // FUCHSIA_ENGINE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
