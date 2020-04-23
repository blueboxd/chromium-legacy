// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/omnibox_mojo_utils.h"

#include <vector>

#include "chrome/common/search/omnibox.mojom.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

namespace omnibox {

namespace {

base::flat_map<int32_t, chrome::mojom::SuggestionGroupPtr>
CreateSuggestionGroupsMap(
    PrefService* prefs,
    const SearchSuggestionParser::HeadersMap& headers_map) {
  base::flat_map<int32_t, chrome::mojom::SuggestionGroupPtr> result_map;
  for (const auto& pair : headers_map) {
    chrome::mojom::SuggestionGroupPtr suggestion_group =
        chrome::mojom::SuggestionGroup::New();
    suggestion_group->header = pair.second;
    suggestion_group->hidden =
        omnibox::IsSuggestionGroupIdHidden(prefs, pair.first);
    result_map.emplace(pair.first, std::move(suggestion_group));
  }
  return result_map;
}

}  // namespace

const char kGoogleGIconResourceName[] = "google_g";
const char kBookmarkIconResourceName[] = "bookmark";
const char kCalculatorIconResourceName[] = "calculator";
const char kClockIconResourceName[] = "clock";
const char kDriveDocsIconResourceName[] = "drive_docs";
const char kDriveFolderIconResourceName[] = "drive_folder";
const char kDriveFormIconResourceName[] = "drive_form";
const char kDriveImageIconResourceName[] = "drive_image";
const char kDriveLogoIconResourceName[] = "drive_logo";
const char kDrivePdfIconResourceName[] = "drive_pdf";
const char kDriveSheetsIconResourceName[] = "drive_sheets";
const char kDriveSlidesIconResourceName[] = "drive_slides";
const char kDriveVideoIconResourceName[] = "drive_video";
const char kExtensionAppIconResourceName[] = "extension_app";
const char kPageIconResourceName[] = "page";
const char kSearchIconResourceName[] = "search";

std::string AutocompleteMatchVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  if (icon.name == omnibox::kBlankIcon.name) {
    return "";  // An empty resource name is effectively a blank icon.
  } else if (icon.name == omnibox::kBookmarkIcon.name) {
    return kBookmarkIconResourceName;
  } else if (icon.name == omnibox::kCalculatorIcon.name) {
    return kCalculatorIconResourceName;
  } else if (icon.name == omnibox::kClockIcon.name) {
    return kClockIconResourceName;
  } else if (icon.name == omnibox::kDriveDocsIcon.name) {
    return kDriveDocsIconResourceName;
  } else if (icon.name == omnibox::kDriveFolderIcon.name) {
    return kDriveFolderIconResourceName;
  } else if (icon.name == omnibox::kDriveFormsIcon.name) {
    return kDriveFormIconResourceName;
  } else if (icon.name == omnibox::kDriveImageIcon.name) {
    return kDriveImageIconResourceName;
  } else if (icon.name == omnibox::kDriveLogoIcon.name) {
    return kDriveLogoIconResourceName;
  } else if (icon.name == omnibox::kDrivePdfIcon.name) {
    return kDrivePdfIconResourceName;
  } else if (icon.name == omnibox::kDriveSheetsIcon.name) {
    return kDriveSheetsIconResourceName;
  } else if (icon.name == omnibox::kDriveSlidesIcon.name) {
    return kDriveSlidesIconResourceName;
  } else if (icon.name == omnibox::kDriveVideoIcon.name) {
    return kDriveVideoIconResourceName;
  } else if (icon.name == omnibox::kExtensionAppIcon.name) {
    return kExtensionAppIconResourceName;
  } else if (icon.name == omnibox::kPageIcon.name) {
    return kPageIconResourceName;
  } else if (icon.name == omnibox::kPedalIcon.name) {
    return "";  // Pedals are not supported in the NTP Realbox.
  } else if (icon.name == vector_icons::kSearchIcon.name) {
    return kSearchIconResourceName;
  } else {
    NOTREACHED()
        << "Every vector icon returned by AutocompleteMatch::GetVectorIcon "
           "must have an equivalent SVG resource for the NTP Realbox.";
    return "";
  }
}

std::vector<chrome::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
    const AutocompleteResult& result) {
  std::vector<chrome::mojom::AutocompleteMatchPtr> matches;
  for (const AutocompleteMatch& match : result) {
    chrome::mojom::AutocompleteMatchPtr mojom_match =
        chrome::mojom::AutocompleteMatch::New();
    mojom_match->allowed_to_be_default_match =
        match.allowed_to_be_default_match;
    mojom_match->contents = match.contents;
    for (const auto& contents_class : match.contents_class) {
      mojom_match->contents_class.push_back(
          chrome::mojom::ACMatchClassification::New(contents_class.offset,
                                                    contents_class.style));
    }
    mojom_match->description = match.description;
    for (const auto& description_class : match.description_class) {
      mojom_match->description_class.push_back(
          chrome::mojom::ACMatchClassification::New(description_class.offset,
                                                    description_class.style));
    }
    mojom_match->destination_url = match.destination_url.spec();
    mojom_match->suggestion_group_id = match.suggestion_group_id.value_or(0);
    mojom_match->icon_url =
        AutocompleteMatchVectorIconToResourceName(match.GetVectorIcon(false));
    mojom_match->image_dominant_color = match.image_dominant_color;
    mojom_match->image_url = match.image_url.spec();
    mojom_match->fill_into_edit = match.fill_into_edit;
    mojom_match->inline_autocompletion = match.inline_autocompletion;
    mojom_match->is_search_type = AutocompleteMatch::IsSearchType(match.type);
    mojom_match->swap_contents_and_description =
        match.swap_contents_and_description;
    mojom_match->type = AutocompleteMatchType::ToString(match.type);
    mojom_match->supports_deletion = match.SupportsDeletion();
    matches.push_back(std::move(mojom_match));
  }
  return matches;
}

chrome::mojom::AutocompleteResultPtr CreateAutocompleteResult(
    const base::string16& input,
    const AutocompleteResult& result,
    PrefService* prefs) {
  return chrome::mojom::AutocompleteResult::New(
      input, CreateSuggestionGroupsMap(prefs, result.headers_map()),
      CreateAutocompleteMatches(result));
}

}  // namespace omnibox
