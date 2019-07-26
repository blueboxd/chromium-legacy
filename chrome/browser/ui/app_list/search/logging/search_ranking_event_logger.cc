// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/logging/search_ranking_event_logger.h"

#include <cmath>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "chromeos/constants/devicetype.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace app_list {
namespace {

using ukm::GetExponentialBucketMinForCounts1000;

// How long to wait for a URL to enter the history service before querying it
// for a UKM source ID.
constexpr base::TimeDelta kDelayForHistoryService =
    base::TimeDelta::FromSeconds(15);

// Chosen so that the bucket at the 24 hour mark is ~60 minutes long. The bucket
// exponent used for counts that are not seconds is 1.15 (via
// ukm::GetExponentialBucketMinForCounts1000). The first value skipped by
// bucketing is 10.
constexpr float kBucketExponentForSeconds = 1.045;

// Represents the type of a search result. The indices of these values
// persist to logs, so existing values should not be modified.
enum class Category {
  UNKNOWN = 0,
  FILE = 1,
  HISTORY = 2,
  NAV_SUGGEST = 3,
  SEARCH = 4,
  BOOKMARK = 5,
  DOCUMENT = 6,
  OMNIBOX_DEPRECATED = 7,
  OMNIBOX_GENERIC = 8
};

int ExtensionTypeFromFileName(const std::string& file_name) {
  // This is a limited list of commonly used extensions. The index of an
  // extension in this list persists to logs, so existing values should not be
  // modified and new values should only be added to the end. This should be
  // kept in sync with AppListNonAppImpressionFileExtension in
  // histograms/enums.xml
  static const base::NoDestructor<std::vector<std::string>> known_extensions(
      {".3ga",        ".3gp",    ".aac",     ".alac", ".asf",  ".avi",
       ".bmp",        ".csv",    ".doc",     ".docx", ".flac", ".gif",
       ".jpeg",       ".jpg",    ".log",     ".m3u",  ".m3u8", ".m4a",
       ".m4v",        ".mid",    ".mkv",     ".mov",  ".mp3",  ".mp4",
       ".mpg",        ".odf",    ".odp",     ".ods",  ".odt",  ".oga",
       ".ogg",        ".ogv",    ".pdf",     ".png",  ".ppt",  ".pptx",
       ".ra",         ".ram",    ".rar",     ".rm",   ".rtf",  ".wav",
       ".webm",       ".webp",   ".wma",     ".wmv",  ".xls",  ".xlsx",
       ".crdownload", ".crx",    ".dmg",     ".exe",  ".html", ".htm",
       ".jar",        ".ps",     ".torrent", ".txt",  ".zip",  ".mhtml",
       ".gdoc",       ".gsheet", ".gslides"});

  size_t found = file_name.find_last_of(".");
  if (found == std::string::npos)
    return -1;
  return std::distance(
      known_extensions->begin(),
      std::find(known_extensions->begin(), known_extensions->end(),
                file_name.substr(found)));
}

Category CategoryFromResultType(ash::SearchResultType type, int subtype) {
  if (type == ash::SearchResultType::kLauncher)
    return Category::FILE;

  if (type == ash::SearchResultType::kOmnibox) {
    switch (static_cast<AutocompleteMatchType::Type>(subtype)) {
      case AutocompleteMatchType::Type::HISTORY_URL:
      case AutocompleteMatchType::Type::HISTORY_TITLE:
      case AutocompleteMatchType::Type::HISTORY_BODY:
      case AutocompleteMatchType::Type::HISTORY_KEYWORD:
        return Category::HISTORY;
      case AutocompleteMatchType::Type::NAVSUGGEST:
      case AutocompleteMatchType::Type::NAVSUGGEST_PERSONALIZED:
        return Category::NAV_SUGGEST;
      case AutocompleteMatchType::Type::SEARCH_HISTORY:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_TAIL:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_PERSONALIZED:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_PROFILE:
      case AutocompleteMatchType::Type::SEARCH_OTHER_ENGINE:
        return Category::SEARCH;
      case AutocompleteMatchType::Type::BOOKMARK_TITLE:
        return Category::BOOKMARK;
      case AutocompleteMatchType::Type::DOCUMENT_SUGGESTION:
        return Category::DOCUMENT;
      case AutocompleteMatchType::Type::EXTENSION_APP_DEPRECATED:
      case AutocompleteMatchType::Type::CONTACT_DEPRECATED:
      case AutocompleteMatchType::Type::PHYSICAL_WEB_DEPRECATED:
      case AutocompleteMatchType::Type::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
      case AutocompleteMatchType::Type::TAB_SEARCH_DEPRECATED:
        return Category::OMNIBOX_DEPRECATED;
      default:
        return Category::OMNIBOX_GENERIC;
    }
  }

  return Category::UNKNOWN;
}

int GetExponentialBucketMinForSeconds(int64_t sample) {
  return ukm::GetExponentialBucketMin(sample, kBucketExponentForSeconds);
}

}  // namespace

SearchRankingEventLogger::SearchRankingEventLogger(
    Profile* profile,
    SearchController* search_controller)
    : search_controller_(search_controller),
      ukm_recorder_(ukm::UkmRecorder::Get()),
      ukm_background_recorder_(
          ukm::UkmBackgroundRecorderFactory::GetForProfile(profile)),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(search_controller_);
}

SearchRankingEventLogger::~SearchRankingEventLogger() = default;

SearchRankingEventLogger::ResultState::ResultState() = default;
SearchRankingEventLogger::ResultState::~ResultState() = default;

SearchRankingEventLogger::ResultInfo::ResultInfo() = default;
SearchRankingEventLogger::ResultInfo::ResultInfo(
    const SearchRankingEventLogger::ResultInfo& other) = default;
SearchRankingEventLogger::ResultInfo::~ResultInfo() = default;

void SearchRankingEventLogger::SetEventRecordedForTesting(
    base::OnceClosure closure) {
  event_recorded_for_testing_ = std::move(closure);
}

void SearchRankingEventLogger::Log(
    const base::string16& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& search_results,
    int launched_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& id_index : search_results) {
    auto* result = search_controller_->FindSearchResult(id_index.id);
    if (!result)
      continue;

    ResultInfo result_info;
    result_info.index = id_index.position_index;
    result_info.title = result->title();
    result_info.type = result->result_type();
    result_info.subtype = result->result_subtype();
    result_info.relevance = result->relevance();

    // Omnibox results have associated URLs, so are logged keyed on the URL
    // after validating that it exists in the history service. Other results
    // have no associated URL, so use a blank source ID.
    if (result->result_type() == ash::SearchResultType::kOmnibox) {
      // The id metadata of an OmnioboxResult is a stripped URL, which does not
      // correspond to the URL that will be navigated to.
      result_info.target =
          static_cast<OmniboxResult*>(result)->DestinationURL().spec();

      // When an omnibox result is launched, we need to retrieve a source ID
      // using the history service. This may be the first time the URL is used
      // and so it must be committed to the history service database before we
      // retrieve it, which happens once the page has loaded. So we delay our
      // check for long enough that most pages will have loaded.
      if (launched_index == id_index.position_index) {
        base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &SearchRankingEventLogger::GetBackgroundSourceIdAndLogEvent,
                weak_factory_.GetWeakPtr(), next_event_id_, trimmed_query,
                result_info, launched_index),
            kDelayForHistoryService);
      } else {
        GetBackgroundSourceIdAndLogEvent(next_event_id_, trimmed_query,
                                         result_info, launched_index);
      }
    } else {
      result_info.target = result->id();
      LogEvent(next_event_id_, trimmed_query, result_info, launched_index,
               base::nullopt);
    }
  }

  ++next_event_id_;
}

void SearchRankingEventLogger::GetBackgroundSourceIdAndLogEvent(
    int event_id,
    const base::string16& trimmed_query,
    const ResultInfo& result,
    int launched_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ukm_background_recorder_->GetBackgroundSourceIdIfAllowed(
      url::Origin::Create(GURL(result.target)),
      base::BindOnce(&SearchRankingEventLogger::LogEvent,
                     base::Unretained(this), event_id, trimmed_query, result,
                     launched_index));
}

void SearchRankingEventLogger::LogEvent(
    int event_id,
    const base::string16& trimmed_query,
    const ResultInfo& result,
    int launched_index,
    base::Optional<ukm::SourceId> source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!source_id)
    source_id = ukm_recorder_->GetNewSourceID();

  const base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  ukm::builders::AppListNonAppImpression event(source_id.value());
  event.SetEventId(event_id)
      .SetPosition(result.index)
      .SetIsLaunched(result.index == launched_index)
      .SetQueryLength(
          GetExponentialBucketMinForCounts1000(trimmed_query.size()))
      // Note this is the search provider's original relevance score, not
      // tweaked by any search ranking models. Scores are floats in 0 to 1, and
      // we map this to ints 0 to 100.
      .SetRelevanceScore(static_cast<int>(100 * result.relevance))
      .SetCategory(
          static_cast<int>(CategoryFromResultType(result.type, result.subtype)))
      .SetHourOfDay(now_exploded.hour)
      .SetDayOfWeek(now_exploded.day_of_week);

  if (result.type == ash::SearchResultType::kLauncher) {
    event.SetFileExtension(ExtensionTypeFromFileName(result.target));
  }

  auto& event_info = id_to_result_state_[result.target];

  if (event_info.last_launch != base::nullopt) {
    base::Time last_launch = event_info.last_launch.value();
    base::Time::Exploded last_launch_exploded;
    last_launch.LocalExplode(&last_launch_exploded);

    event.SetTimeSinceLastLaunch(
        GetExponentialBucketMinForSeconds((now - last_launch).InSeconds()));
    event.SetTimeOfLastLaunch(last_launch_exploded.hour);

    // Reset the number of launches this hour to 0 if this is the first launch
    // today of this event, to account for user sessions spanning multiple days.
    if (result.index == launched_index &&
        now - last_launch >= base::TimeDelta::FromHours(23)) {
      event_info.launches_per_hour[now_exploded.hour] = 0;
    }
  }

  event.SetLaunchesThisSession(
      GetExponentialBucketMinForCounts1000(event_info.launches_this_session));

  const auto& launches = event_info.launches_per_hour;
  event.SetLaunchesAtHour00(GetExponentialBucketMinForCounts1000(launches[0]));
  event.SetLaunchesAtHour01(GetExponentialBucketMinForCounts1000(launches[1]));
  event.SetLaunchesAtHour02(GetExponentialBucketMinForCounts1000(launches[2]));
  event.SetLaunchesAtHour03(GetExponentialBucketMinForCounts1000(launches[3]));
  event.SetLaunchesAtHour04(GetExponentialBucketMinForCounts1000(launches[4]));
  event.SetLaunchesAtHour05(GetExponentialBucketMinForCounts1000(launches[5]));
  event.SetLaunchesAtHour06(GetExponentialBucketMinForCounts1000(launches[6]));
  event.SetLaunchesAtHour07(GetExponentialBucketMinForCounts1000(launches[7]));
  event.SetLaunchesAtHour08(GetExponentialBucketMinForCounts1000(launches[8]));
  event.SetLaunchesAtHour09(GetExponentialBucketMinForCounts1000(launches[9]));
  event.SetLaunchesAtHour10(GetExponentialBucketMinForCounts1000(launches[10]));
  event.SetLaunchesAtHour11(GetExponentialBucketMinForCounts1000(launches[11]));
  event.SetLaunchesAtHour12(GetExponentialBucketMinForCounts1000(launches[12]));
  event.SetLaunchesAtHour13(GetExponentialBucketMinForCounts1000(launches[13]));
  event.SetLaunchesAtHour14(GetExponentialBucketMinForCounts1000(launches[14]));
  event.SetLaunchesAtHour15(GetExponentialBucketMinForCounts1000(launches[15]));
  event.SetLaunchesAtHour16(GetExponentialBucketMinForCounts1000(launches[16]));
  event.SetLaunchesAtHour17(GetExponentialBucketMinForCounts1000(launches[17]));
  event.SetLaunchesAtHour18(GetExponentialBucketMinForCounts1000(launches[18]));
  event.SetLaunchesAtHour19(GetExponentialBucketMinForCounts1000(launches[19]));
  event.SetLaunchesAtHour20(GetExponentialBucketMinForCounts1000(launches[20]));
  event.SetLaunchesAtHour21(GetExponentialBucketMinForCounts1000(launches[21]));
  event.SetLaunchesAtHour22(GetExponentialBucketMinForCounts1000(launches[22]));
  event.SetLaunchesAtHour23(GetExponentialBucketMinForCounts1000(launches[23]));

  event.Record(ukm_recorder_);

  if (result.index == launched_index) {
    event_info.last_launch = now;
    event_info.launches_this_session += 1;
    event_info.launches_per_hour[now_exploded.hour] += 1;
  }

  if (event_recorded_for_testing_)
    std::move(event_recorded_for_testing_).Run();
}

}  // namespace app_list
