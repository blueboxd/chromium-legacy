// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/types_mojom_traits.h"

namespace mojo {

// static
local_search_service::mojom::IndexId
EnumTraits<local_search_service::mojom::IndexId,
           local_search_service::IndexId>::ToMojom(local_search_service::IndexId
                                                       input) {
  switch (input) {
    case local_search_service::IndexId::kInvalid:
      return local_search_service::mojom::IndexId::INVALID;
    case local_search_service::IndexId::kCrosSettings:
      return local_search_service::mojom::IndexId::CROS_SETTINGS;
  }
  NOTREACHED();
  return local_search_service::mojom::IndexId::INVALID;
}

// static
bool EnumTraits<local_search_service::mojom::IndexId,
                local_search_service::IndexId>::
    FromMojom(local_search_service::mojom::IndexId input,
              local_search_service::IndexId* output) {
  switch (input) {
    case local_search_service::mojom::IndexId::INVALID:
      *output = local_search_service::IndexId::kInvalid;
      return true;
    case local_search_service::mojom::IndexId::CROS_SETTINGS:
      *output = local_search_service::IndexId::kCrosSettings;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<local_search_service::mojom::ContentDataView,
                  local_search_service::Content>::
    Read(local_search_service::mojom::ContentDataView data,
         local_search_service::Content* out) {
  std::string id;
  base::string16 content;
  if (!data.ReadId(&id) || !data.ReadContent(&content))
    return false;

  *out = local_search_service::Content(id, content);
  return true;
}

// static
bool StructTraits<local_search_service::mojom::DataDataView,
                  local_search_service::Data>::
    Read(local_search_service::mojom::DataDataView data,
         local_search_service::Data* out) {
  std::string id;
  std::vector<local_search_service::Content> contents;
  if (!data.ReadId(&id) || !data.ReadContents(&contents))
    return false;

  *out = local_search_service::Data(id, contents);
  return true;
}

// static
bool StructTraits<local_search_service::mojom::SearchParamsDataView,
                  local_search_service::SearchParams>::
    Read(local_search_service::mojom::SearchParamsDataView data,
         local_search_service::SearchParams* out) {
  *out = local_search_service::SearchParams();
  out->relevance_threshold = data.relevance_threshold();
  out->partial_match_penalty_rate = data.partial_match_penalty_rate();
  out->use_prefix_only = data.use_prefix_only();
  out->use_edit_distance = data.use_edit_distance();
  return true;
}

// static
bool StructTraits<local_search_service::mojom::PositionDataView,
                  local_search_service::Position>::
    Read(local_search_service::mojom::PositionDataView data,
         local_search_service::Position* out) {
  *out = local_search_service::Position();
  if (!data.ReadContentId(&(out->content_id)))
    return false;

  out->start = data.start();
  out->length = data.length();
  return true;
}

// static
bool StructTraits<local_search_service::mojom::ResultDataView,
                  local_search_service::Result>::
    Read(local_search_service::mojom::ResultDataView data,
         local_search_service::Result* out) {
  std::string id;
  std::vector<local_search_service::Position> positions;
  if (!data.ReadId(&id) || !data.ReadPositions(&positions))
    return false;

  *out = local_search_service::Result();
  out->id = id;
  out->score = data.score();
  out->positions = positions;
  return true;
}

// static
local_search_service::mojom::ResponseStatus
EnumTraits<local_search_service::mojom::ResponseStatus,
           local_search_service::ResponseStatus>::
    ToMojom(local_search_service::ResponseStatus input) {
  switch (input) {
    case local_search_service::ResponseStatus::kUnknownError:
      return local_search_service::mojom::ResponseStatus::UNKNOWN_ERROR;
    case local_search_service::ResponseStatus::kSuccess:
      return local_search_service::mojom::ResponseStatus::SUCCESS;
    case local_search_service::ResponseStatus::kEmptyQuery:
      return local_search_service::mojom::ResponseStatus::EMPTY_QUERY;
    case local_search_service::ResponseStatus::kEmptyIndex:
      return local_search_service::mojom::ResponseStatus::EMPTY_INDEX;
  }
  NOTREACHED();
  return local_search_service::mojom::ResponseStatus::UNKNOWN_ERROR;
}

// static
bool EnumTraits<local_search_service::mojom::ResponseStatus,
                local_search_service::ResponseStatus>::
    FromMojom(local_search_service::mojom::ResponseStatus input,
              local_search_service::ResponseStatus* output) {
  switch (input) {
    case local_search_service::mojom::ResponseStatus::UNKNOWN_ERROR:
      *output = local_search_service::ResponseStatus::kUnknownError;
      return true;
    case local_search_service::mojom::ResponseStatus::SUCCESS:
      *output = local_search_service::ResponseStatus::kSuccess;
      return true;
    case local_search_service::mojom::ResponseStatus::EMPTY_QUERY:
      *output = local_search_service::ResponseStatus::kEmptyQuery;
      return true;
    case local_search_service::mojom::ResponseStatus::EMPTY_INDEX:
      *output = local_search_service::ResponseStatus::kEmptyIndex;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
