// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/contextual_tooltip.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/util/values/values_util.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace contextual_tooltip {

namespace {

// Keys for tooltip sub-preferences for shown count and last time shown.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";

// Keys for tooltip sub-preferences of how many times a gesture has been
// successfully performed by the user.
constexpr char kSuccessCount[] = "success_count";

base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  if (g_clock_override)
    return g_clock_override->Now();
  return base::Time::Now();
}

std::string TooltipTypeToString(TooltipType type) {
  switch (type) {
    case TooltipType::kDragHandle:
      return "drag_handle";
    case TooltipType::kBackGesture:
      return "back_gesture";
    case TooltipType::kHomeToOverview:
      return "home_to_overview";
  }
  return "invalid";
}

// Creates the path to the dictionary value from the contextual tooltip type and
// the sub-preference.
std::string GetPath(TooltipType type, const std::string& sub_pref) {
  return base::JoinString({TooltipTypeToString(type), sub_pref}, ".");
}

base::Time GetLastShownTime(PrefService* prefs, TooltipType type) {
  const base::Value* last_shown_time =
      prefs->GetDictionary(prefs::kContextualTooltips)
          ->FindPath(GetPath(type, kLastTimeShown));
  if (!last_shown_time)
    return base::Time();
  return *util::ValueToTime(last_shown_time);
}

int GetSuccessCount(PrefService* prefs, TooltipType type) {
  base::Optional<int> success_count =
      prefs->GetDictionary(prefs::kContextualTooltips)
          ->FindIntPath(GetPath(type, kSuccessCount));
  return success_count.value_or(0);
}

const base::Optional<base::TimeDelta>& GetMinIntervalOverride() {
  // Overridden minimum time between showing contextual nudges to the user.
  static base::Optional<base::TimeDelta> min_interval_override;
  if (!min_interval_override) {
    min_interval_override = switches::ContextualNudgesInterval();
  }
  return min_interval_override;
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (features::AreContextualNudgesEnabled())
    registry->RegisterDictionaryPref(prefs::kContextualTooltips);
}

bool ShouldShowNudge(PrefService* prefs, TooltipType type) {
  if (!features::AreContextualNudgesEnabled())
    return false;

  const int success_count = GetSuccessCount(prefs, type);
  if (success_count >= kSuccessLimit ||
      (type == TooltipType::kHomeToOverview &&
       success_count >= kSuccessLimitHomeToOverview)) {
    return false;
  }

  const int shown_count = GetShownCount(prefs, type);
  if (shown_count >= kNotificationLimit)
    return false;
  if (shown_count == 0)
    return true;
  const base::Time last_shown_time = GetLastShownTime(prefs, type);
  base::TimeDelta min_interval =
      GetMinIntervalOverride().value_or(kMinInterval);
  return (GetTime() - last_shown_time) >= min_interval;
}

base::TimeDelta GetNudgeTimeout(PrefService* prefs, TooltipType type) {
  const int shown_count = GetShownCount(prefs, type);
  if (shown_count == 0)
    return base::TimeDelta();
  return kNudgeShowDuration;
}

int GetShownCount(PrefService* prefs, TooltipType type) {
  base::Optional<int> shown_count =
      prefs->GetDictionary(prefs::kContextualTooltips)
          ->FindIntPath(GetPath(type, kShownCount));
  return shown_count.value_or(0);
}

void HandleNudgeShown(PrefService* prefs, TooltipType type) {
  const int shown_count = GetShownCount(prefs, type);
  DictionaryPrefUpdate update(prefs, prefs::kContextualTooltips);
  update->SetIntPath(GetPath(type, kShownCount), shown_count + 1);
  update->SetPath(GetPath(type, kLastTimeShown), util::TimeToValue(GetTime()));
}

void HandleGesturePerformed(PrefService* prefs, TooltipType type) {
  const int success_count = GetSuccessCount(prefs, type);
  DictionaryPrefUpdate update(prefs, prefs::kContextualTooltips);
  update->SetIntPath(GetPath(type, kSuccessCount), success_count + 1);
}

void ClearPrefs() {
  DCHECK(Shell::Get()->session_controller()->GetLastActiveUserPrefService());
  DictionaryPrefUpdate update(
      Shell::Get()->session_controller()->GetLastActiveUserPrefService(),
      prefs::kContextualTooltips);
  base::DictionaryValue* nudges_dict = update.Get();
  if (nudges_dict && !nudges_dict->empty())
    nudges_dict->Clear();
}

void OverrideClockForTesting(base::Clock* test_clock) {
  DCHECK(!g_clock_override);
  g_clock_override = test_clock;
}

void ClearClockOverrideForTesting() {
  DCHECK(g_clock_override);
  g_clock_override = nullptr;
}

}  // namespace contextual_tooltip

}  // namespace ash
