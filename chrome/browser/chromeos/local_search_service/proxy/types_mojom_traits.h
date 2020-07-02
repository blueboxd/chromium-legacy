// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_TYPES_MOJOM_TRAITS_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_TYPES_MOJOM_TRAITS_H_

#include "chrome/browser/chromeos/local_search_service/index.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/chromeos/local_search_service/proxy/local_search_service_proxy.mojom.h"
#include "chrome/browser/chromeos/local_search_service/proxy/types.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// TODO(crbug/1092767): Consolidate the API to use mojo enums instead of
// EnumTraits.

template <>
struct EnumTraits<local_search_service::mojom::IndexId,
                  local_search_service::IndexId> {
  static local_search_service::mojom::IndexId ToMojom(
      local_search_service::IndexId input);
  static bool FromMojom(local_search_service::mojom::IndexId input,
                        local_search_service::IndexId* output);
};

template <>
struct EnumTraits<local_search_service::mojom::Backend,
                  local_search_service::Backend> {
  static local_search_service::mojom::Backend ToMojom(
      local_search_service::Backend input);
  static bool FromMojom(local_search_service::mojom::Backend input,
                        local_search_service::Backend* output);
};

template <>
struct StructTraits<local_search_service::mojom::ContentDataView,
                    local_search_service::Content> {
 public:
  static std::string id(const local_search_service::Content& c) { return c.id; }
  static base::string16 content(const local_search_service::Content& c) {
    return c.content;
  }
  static double weight(const local_search_service::Content& c) {
    return c.weight;
  }

  static bool Read(local_search_service::mojom::ContentDataView data,
                   local_search_service::Content* out);
};

template <>
struct StructTraits<local_search_service::mojom::DataDataView,
                    local_search_service::Data> {
 public:
  static std::string id(const local_search_service::Data& d) { return d.id; }
  static std::vector<local_search_service::Content> contents(
      const local_search_service::Data& d) {
    return d.contents;
  }

  static std::string locale(const local_search_service::Data& d) {
    return d.locale;
  }

  static bool Read(local_search_service::mojom::DataDataView data,
                   local_search_service::Data* out);
};

template <>
struct StructTraits<local_search_service::mojom::SearchParamsDataView,
                    local_search_service::SearchParams> {
 public:
  static double relevance_threshold(
      const local_search_service::SearchParams& s) {
    return s.relevance_threshold;
  }
  static double prefix_threshold(const local_search_service::SearchParams& s) {
    return s.prefix_threshold;
  }
  static double fuzzy_threshold(const local_search_service::SearchParams& s) {
    return s.fuzzy_threshold;
  }

  static bool Read(local_search_service::mojom::SearchParamsDataView data,
                   local_search_service::SearchParams* out);
};

template <>
struct StructTraits<local_search_service::mojom::PositionDataView,
                    local_search_service::Position> {
 public:
  static std::string content_id(const local_search_service::Position& p) {
    return p.content_id;
  }
  static uint32_t start(const local_search_service::Position& p) {
    return p.start;
  }
  static uint32_t length(const local_search_service::Position& p) {
    return p.length;
  }

  static bool Read(local_search_service::mojom::PositionDataView data,
                   local_search_service::Position* out);
};

template <>
struct StructTraits<local_search_service::mojom::ResultDataView,
                    local_search_service::Result> {
 public:
  static std::string id(const local_search_service::Result& r) { return r.id; }
  static double score(const local_search_service::Result& r) { return r.score; }
  static std::vector<local_search_service::Position> positions(
      const local_search_service::Result& r) {
    return r.positions;
  }

  static bool Read(local_search_service::mojom::ResultDataView data,
                   local_search_service::Result* out);
};

template <>
struct EnumTraits<local_search_service::mojom::ResponseStatus,
                  local_search_service::ResponseStatus> {
  static local_search_service::mojom::ResponseStatus ToMojom(
      local_search_service::ResponseStatus input);
  static bool FromMojom(local_search_service::mojom::ResponseStatus input,
                        local_search_service::ResponseStatus* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_TYPES_MOJOM_TRAITS_H_
