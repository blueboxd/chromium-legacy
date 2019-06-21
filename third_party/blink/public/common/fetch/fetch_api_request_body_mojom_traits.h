// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FETCH_FETCH_API_REQUEST_BODY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FETCH_FETCH_API_REQUEST_BODY_MOJOM_TRAITS_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                    scoped_refptr<network::ResourceRequestBody>> {
  static bool IsNull(const scoped_refptr<network::ResourceRequestBody>& r) {
    return !r;
  }

  static void SetToNull(scoped_refptr<network::ResourceRequestBody>* out) {
    out->reset();
  }

  static const std::vector<network::DataElement>& elements(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return *r->elements();
  }

  static uint64_t identifier(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->identifier_;
  }

  static bool contains_sensitive_info(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->contains_sensitive_info_;
  }

  static bool Read(blink::mojom::FetchAPIRequestBodyDataView data,
                   scoped_refptr<network::ResourceRequestBody>* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FETCH_FETCH_API_REQUEST_BODY_MOJOM_TRAITS_H_
