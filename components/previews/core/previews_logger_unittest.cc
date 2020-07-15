// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "components/previews/core/previews_block_list.h"
#include "components/previews/core/previews_logger_observer.h"
#include "components/previews/core/previews_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const char kPreviewsDecisionMadeEventType[] = "Decision";
const char kPreviewsNavigationEventType[] = "Navigation";
const size_t kMaximumNavigationLogs = 10;
const size_t kMaximumDecisionLogs = 25;

// Mock class to test correct MessageLog is passed back to the
// mojo::InterventionsInternalsPage.
class TestPreviewsLoggerObserver : public PreviewsLoggerObserver {
 public:
  TestPreviewsLoggerObserver() = default;

  ~TestPreviewsLoggerObserver() override = default;

  // PreviewsLoggerObserver:
  void OnNewMessageLogAdded(
      const PreviewsLogger::MessageLog& message) override {
    message_ = std::make_unique<PreviewsLogger::MessageLog>(message);
    messages_.push_back(*message_);
  }
  void OnNewBlocklistedHost(const std::string& host, base::Time time) override {
    host_blocklisted_called_ = true;
    blocklisted_hosts_[host] = time;
  }
  void OnUserBlocklistedStatusChange(bool blocklisted) override {
    ++user_status_change_calls_;
    user_blocklisted_ = blocklisted;
  }
  void OnBlocklistCleared(base::Time time) override {
    blocklist_cleared_called_ = true;
    blocklist_cleared_time_ = time;
  }
  void OnIgnoreBlocklistDecisionStatusChanged(bool ignored) override {
    blocklist_ignored_ = ignored;
  }
  void OnLastObserverRemove() override { last_removed_notified_ = true; }

  // Expose the passed in MessageLog for testing.
  PreviewsLogger::MessageLog* message() const { return message_.get(); }

  // Expose the received MessageLogs for testing.
  const std::vector<PreviewsLogger::MessageLog>& messages() const {
    return messages_;
  }

  // Expose blocklist events info for testing.
  const std::unordered_map<std::string, base::Time>& blocklisted_hosts() {
    return blocklisted_hosts_;
  }
  bool host_blocklisted_called() const { return host_blocklisted_called_; }
  size_t user_status_change_calls() const { return user_status_change_calls_; }
  bool blocklist_cleared_called() const { return blocklist_cleared_called_; }
  bool user_blocklisted() const { return user_blocklisted_; }
  base::Time blocklist_cleared_time() const { return blocklist_cleared_time_; }

  // Expose whether PreviewsBlockList decisions are ignored or not.
  bool blocklist_ignored() const { return blocklist_ignored_; }

  // Expose whether observer is notified that it is the last observer to be
  // removed for testing.
  bool last_removed_notified() { return last_removed_notified_; }

 private:
  // Received messages.
  std::vector<PreviewsLogger::MessageLog> messages_;

  // The passed in MessageLog in OnNewMessageLogAdded.
  std::unique_ptr<PreviewsLogger::MessageLog> message_;

  // Received blocklisted event info.
  std::unordered_map<std::string, base::Time> blocklisted_hosts_;
  bool user_blocklisted_;
  base::Time blocklist_cleared_time_;
  bool host_blocklisted_called_ = false;
  size_t user_status_change_calls_ = 0;
  bool blocklist_cleared_called_ = false;

  // BlocklistPreviews decision is ignored or not.
  bool blocklist_ignored_ = false;

  // Whether |this| is the last observer to be removed.
  bool last_removed_notified_ = false;
};

class PreviewsLoggerTest : public testing::Test {
 public:
  PreviewsLoggerTest() {}

  ~PreviewsLoggerTest() override {}

  void SetUp() override { logger_ = std::make_unique<PreviewsLogger>(); }

  std::string LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason reason,
      bool final_reason) {
    const base::Time time = base::Time::Now();
    PreviewsType type = PreviewsType::DEFER_ALL_SCRIPT;
    const GURL url("http://www.url_a.com/url");
    const uint64_t page_id = 1234;
    TestPreviewsLoggerObserver observer;
    logger_->AddAndNotifyObserver(&observer);
    if (final_reason) {
      std::vector<PreviewsEligibilityReason> passed_reasons = {};
      logger_->LogPreviewDecisionMade(reason, url, time, type,
                                      std::move(passed_reasons), page_id);
    } else {
      std::vector<PreviewsEligibilityReason> passed_reasons = {reason};
      logger_->LogPreviewDecisionMade(PreviewsEligibilityReason::ALLOWED, url,
                                      time, type, std::move(passed_reasons),
                                      page_id);
    }

    auto actual = observer.messages();

    const size_t expected_size = final_reason ? 1 : 2;
    EXPECT_EQ(expected_size, actual.size());

    std::vector<std::string> description_parts = base::SplitStringUsingSubstr(
        actual[0].event_description, "preview - ", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    return description_parts[1];
  }

 protected:
  std::unique_ptr<PreviewsLogger> logger_;
};

TEST_F(PreviewsLoggerTest, LogPreviewDecisionMadeLogMessage) {
  const base::Time time = base::Time::Now();

  PreviewsType type_a = PreviewsType::DEFER_ALL_SCRIPT;
  PreviewsEligibilityReason reason_a =
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE;
  std::vector<PreviewsEligibilityReason> passed_reasons_a = {};
  PreviewsEligibilityReason reason_b =
      PreviewsEligibilityReason::NETWORK_NOT_SLOW;
  std::vector<PreviewsEligibilityReason> passed_reasons_b = {
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
  };
  const GURL url_a("http://www.url_a.com/url_a");
  const GURL url_b("http://www.url_b.com/url_b");
  const uint64_t page_id_a = 1234;
  const uint64_t page_id_b = 4321;

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);

  logger_->LogPreviewDecisionMade(reason_a, url_a, time, type_a,
                                  std::move(passed_reasons_a), page_id_a);
  logger_->LogPreviewDecisionMade(reason_b, url_b, time, type_a,
                                  std::move(passed_reasons_b), page_id_b);

  auto actual = observer.messages();
  const size_t expected_size = 4;  // reason_a, reason_b, and passed_reasons_b
  EXPECT_EQ(expected_size, actual.size());

  std::string expected_description_a =
      "DeferAllScript preview - Blocklist failed to be created";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[0].event_type);
  EXPECT_EQ(expected_description_a, actual[0].event_description);
  EXPECT_EQ(url_a, actual[0].url);
  EXPECT_EQ(time, actual[0].time);
  EXPECT_EQ(page_id_a, actual[0].page_id);

  std::string expected_passed_0 =
      "DeferAllScript preview - Network quality available";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[1].event_type);
  EXPECT_EQ(expected_passed_0, actual[1].event_description);
  EXPECT_EQ(url_b, actual[1].url);
  EXPECT_EQ(time, actual[1].time);
  EXPECT_EQ(page_id_b, actual[1].page_id);

  std::string expected_passed_1 =
      "DeferAllScript preview - Page reloads allowed";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[2].event_type);
  EXPECT_EQ(expected_passed_1, actual[2].event_description);
  EXPECT_EQ(url_b, actual[2].url);
  EXPECT_EQ(time, actual[2].time);
  EXPECT_EQ(page_id_b, actual[2].page_id);

  std::string expected_description_b =
      "DeferAllScript preview - Network not slow";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[3].event_type);
  EXPECT_EQ(expected_description_b, actual[3].event_description);
  EXPECT_EQ(url_b, actual[3].url);
  EXPECT_EQ(time, actual[3].time);
  EXPECT_EQ(page_id_b, actual[3].page_id);
}

TEST_F(PreviewsLoggerTest, LogPreviewNavigationLogMessage) {
  const base::Time time = base::Time::Now();

  PreviewsType type_a = PreviewsType::DEFER_ALL_SCRIPT;
  const GURL url_a("http://www.url_a.com/url_a");
  const GURL url_b("http://www.url_b.com/url_b");
  const uint64_t page_id_a = 1234;
  const uint64_t page_id_b = 1234;

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);

  logger_->LogPreviewNavigation(url_a, type_a, true /* opt_out */, time,
                                page_id_a);
  logger_->LogPreviewNavigation(url_b, type_a, false /* opt_out */, time,
                                page_id_b);

  auto actual = observer.messages();

  const size_t expected_size = 2;
  EXPECT_EQ(expected_size, actual.size());

  std::string expected_description_a =
      "DeferAllScript preview - user opt-out: True";
  EXPECT_EQ(kPreviewsNavigationEventType, actual[0].event_type);
  EXPECT_EQ(expected_description_a, actual[0].event_description);
  EXPECT_EQ(url_a, actual[0].url);
  EXPECT_EQ(time, actual[0].time);
  EXPECT_EQ(page_id_a, actual[0].page_id);

  std::string expected_description_b =
      "DeferAllScript preview - user opt-out: False";
  EXPECT_EQ(kPreviewsNavigationEventType, actual[1].event_type);
  EXPECT_EQ(expected_description_b, actual[1].event_description);
  EXPECT_EQ(url_b, actual[1].url);
  EXPECT_EQ(time, actual[1].time);
  EXPECT_EQ(page_id_b, actual[1].page_id);
}

TEST_F(PreviewsLoggerTest, PreviewsLoggerOnlyKeepsCertainNumberOfDecisionLogs) {
  PreviewsType type = PreviewsType::DEFER_ALL_SCRIPT;
  PreviewsEligibilityReason reason =
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE;
  const base::Time time = base::Time::Now();
  const GURL url("http://www.url_.com/url_");
  const uint64_t page_id = 1234;

  for (size_t i = 0; i < 2 * kMaximumDecisionLogs; i++) {
    std::vector<PreviewsEligibilityReason> passed_reasons = {};
    logger_->LogPreviewDecisionMade(reason, url, time, type,
                                    std::move(passed_reasons), page_id);
  }

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_EQ(kMaximumDecisionLogs, observer.messages().size());
}

TEST_F(PreviewsLoggerTest,
       PreviewsLoggerOnlyKeepsCertainNumberOfNavigationLogs) {
  PreviewsType type = PreviewsType::DEFER_ALL_SCRIPT;
  const GURL url("http://www.url_.com/url_");
  const base::Time time = base::Time::Now();
  const uint64_t page_id = 1234;

  for (size_t i = 0; i < 2 * kMaximumNavigationLogs; ++i) {
    logger_->LogPreviewNavigation(url, type, true /* opt_out */, time, page_id);
  }

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_EQ(kMaximumNavigationLogs, observer.messages().size());
}

TEST_F(PreviewsLoggerTest,
       ObserverIsNotifiedOfHistoricalNavigationsAndDecisionsWhenAdded) {
  // Non historical log event.
  logger_->LogMessage("Event_", "Some description_",
                      GURL("http://www.url_.com/url_"), base::Time::Now(),
                      1234 /* page_id */);

  PreviewsType type = PreviewsType::DEFER_ALL_SCRIPT;
  PreviewsEligibilityReason final_reason =
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE;
  std::vector<PreviewsEligibilityReason> passed_reasons = {
      PreviewsEligibilityReason::NETWORK_NOT_SLOW};
  const GURL urls[] = {
      GURL("http://www.url_0.com/url_0"),  // Decision event.
      GURL("http://www.url_0.com/url_0"),  // Decision event.
      GURL("http://www.url_1.com/url_1"),  // Navigation event.
  };
  const base::Time times[] = {
      base::Time::FromJsTime(-413696806000),  // Nov 21 1956 20:13:14 UTC
      base::Time::FromJsTime(-413696806000),  // Same as above.
      base::Time::FromJsTime(758620800000),   // Jan 15 1994 08:00:00 UTC
  };
  const uint64_t page_ids[] = {1233, 1233, 5678 /* Navigation page_id */};

  // Logging decisions and navigations events.
  logger_->LogPreviewDecisionMade(final_reason, urls[0], times[0], type,
                                  std::move(passed_reasons), page_ids[0]);
  logger_->LogPreviewNavigation(urls[2], type, true /* opt_out */, times[2],
                                page_ids[2]);

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);

  // Check correct ordering of the historical log messages.
  auto received_messages = observer.messages();

  const size_t expected_size = 3;
  EXPECT_EQ(expected_size, received_messages.size());

  const std::string expected_types[] = {
      kPreviewsDecisionMadeEventType, kPreviewsDecisionMadeEventType,
      kPreviewsNavigationEventType,
  };

  for (size_t i = 0; i < expected_size; i++) {
    EXPECT_EQ(expected_types[i], received_messages[i].event_type);
    EXPECT_EQ(urls[i], received_messages[i].url);
    EXPECT_EQ(times[i], received_messages[i].time);
    EXPECT_EQ(page_ids[i], received_messages[i].page_id);
  }
}

TEST_F(PreviewsLoggerTest, ObserversOnNewMessageIsCalledWithCorrectParams) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const std::string type = "Event_";
  const std::string description = "Some description";
  const GURL url("http://www.url_.com/url_");
  const base::Time now = base::Time::Now();
  const uint64_t page_id = 1234;
  logger_->LogMessage(type, description, url, now, page_id);

  const size_t expected_size = 1;
  for (size_t i = 0; i < number_of_obs; i++) {
    EXPECT_EQ(expected_size, observers[i].messages().size());
    EXPECT_EQ(type, observers[i].message()->event_type);
    EXPECT_EQ(description, observers[i].message()->event_description);
    EXPECT_EQ(url, observers[i].message()->url);
    EXPECT_EQ(now, observers[i].message()->time);
    EXPECT_EQ(page_id, observers[i].message()->page_id);
  }
}

TEST_F(PreviewsLoggerTest, RemovedObserverIsNotNotified) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);

  const std::string type = "Event_";
  const std::string description = "Some description";
  const GURL url("http://www.url_.com/url_");
  const base::Time now = base::Time::Now();
  const uint64_t page_id = 1234;
  logger_->LogMessage(type, description, url, now, page_id);

  const size_t expected_size = 0;
  EXPECT_EQ(expected_size, observers[removed_observer].messages().size());

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_EQ(type, observers[i].message()->event_type);
      EXPECT_EQ(description, observers[i].message()->event_description);
      EXPECT_EQ(url, observers[i].message()->url);
      EXPECT_EQ(now, observers[i].message()->time);
      EXPECT_EQ(page_id, observers[i].message()->page_id);
    }
  }
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionAllowedChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::ALLOWED, true /* final_reason */);
  std::string expected_description = "Allowed";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionUnavailabeFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE,
      true /* final_reason */);
  std::string expected_description = "Blocklist failed to be created";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionUnavailabeChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE,
      false /* final_reason */);
  std::string expected_description = "Blocklist not null";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNotLoadedFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED,
      true /* final_reason */);
  std::string expected_description = "Blocklist not loaded from disk yet";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNotLoadedChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED,
      false /* final_reason */);
  std::string expected_description = "Blocklist loaded from disk";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionRecentlyOptedOutFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      true /* final_reason */);
  std::string expected_description = "User recently opted out";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionRecentlyOptedOutChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      false /* final_reason */);
  std::string expected_description = "User did not opt out recently";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionBlocklistedFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::USER_BLOCKLISTED, true /* final_reason */);
  std::string expected_description = "All previews are blocklisted";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionBlocklistedChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::USER_BLOCKLISTED, false /* final_reason */);
  std::string expected_description = "Not all previews are blocklisted";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionHostBlocklistedFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::HOST_BLOCKLISTED, true /* final_reason */);
  std::string expected_description =
      "All previews on this host are blocklisted";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionHostBlocklistedChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::HOST_BLOCKLISTED, false /* final_reason */);
  std::string expected_description = "Host is not blocklisted on all previews";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionNetworkUnavailableFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
      true /* final_reason */);
  std::string expected_description = "Network quality unavailable";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionNetworkUnavailableChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
      false /* final_reason */);
  std::string expected_description = "Network quality available";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNetworkNotSlowFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NETWORK_NOT_SLOW, true /* final_reason */);
  std::string expected_description = "Network not slow";

  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNetworkNotSlowChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NETWORK_NOT_SLOW, false /* final_reason */);
  std::string expected_description = "Network is slow";

  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionReloadDisallowedFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::RELOAD_DISALLOWED, true /* final_reason */);
  std::string expected_description =
      "Page reloads do not show previews for this preview type";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionReloadDisallowedChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::RELOAD_DISALLOWED, false /* final_reason */);
  std::string expected_description = "Page reloads allowed";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionServerRulesFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::DEPRECATED_HOST_BLOCKLISTED_BY_SERVER,
      true /* final_reason */);
  std::string expected_description = "Host blocklisted by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionServerRulesChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::DEPRECATED_HOST_BLOCKLISTED_BY_SERVER,
      false /* final_reason */);
  std::string expected_description = "Host not blocklisted by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionNotAllowistedByServerFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::DEPRECATED_HOST_NOT_ALLOWLISTED_BY_SERVER,
      true /* final_reason */);
  std::string expected_description = "Host not allowlisted by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionNotAllowlistedByServerChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::DEPRECATED_HOST_NOT_ALLOWLISTED_BY_SERVER,
      false /* final_reason */);
  std::string expected_description = "Host allowlisted by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(
    PreviewsLoggerTest,
    LogPreviewDecisionDescriptionAllowedWithoutServerOptimizationHintsFailed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS,
      true /* final_reason */);
  std::string expected_description = "Allowed (but without server rule check)";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(
    PreviewsLoggerTest,
    LogPreviewDecisionDescriptionAllowedWithoutServerOptimizationHintsChecked) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS,
      false /* final_reason */);
  std::string expected_description = "Not allowed (without server rule check)";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionCommitted) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::COMMITTED, true /* final_reason */);
  std::string expected_description = "Committed";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionCacheControlNoTransform) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::CACHE_CONTROL_NO_TRANSFORM,
      true /* final_reason */);
  std::string expected_description = "Cache-control no-transform received";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionNotAllowedByOptimizationGuide) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE,
      true /*final_reason */);
  std::string expected_description = "Not allowed by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionAllowedByOptimizationGuide) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE,
      false /*final_reason */);
  std::string expected_description = "Allowed by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionPageLoadNotPainful) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::PAGE_LOAD_PREDICTION_NOT_PAINFUL,
      true /* final_reason */);
  std::string expected_description = "Page load not predicted to be painful";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionPageLoadPainful) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::PAGE_LOAD_PREDICTION_NOT_PAINFUL,
      false /* final_reason */);
  std::string expected_description = "Page load predicted to be painful";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, NotifyObserversOfNewBlocklistedHost) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);

  const std::string expected_host = "example.com";
  const base::Time expected_time = base::Time::Now();
  const size_t expected_size = 1;
  logger_->OnNewBlocklistedHost(expected_host, expected_time);

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_TRUE(observers[i].host_blocklisted_called());
      EXPECT_EQ(expected_size, observers[i].blocklisted_hosts().size());
      EXPECT_EQ(expected_time,
                observers[i].blocklisted_hosts().find(expected_host)->second);
    }
  }
  EXPECT_FALSE(observers[removed_observer].host_blocklisted_called());
}

TEST_F(PreviewsLoggerTest, NotifyObserversWhenUserBlocklisted) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);
  logger_->OnUserBlocklistedStatusChange(true /* blocklisted */);
  const size_t expected_times = 2;

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_EQ(expected_times, observers[i].user_status_change_calls());
      EXPECT_TRUE(observers[i].user_blocklisted());
    }
  }
  EXPECT_EQ(expected_times - 1,
            observers[removed_observer].user_status_change_calls());
}

TEST_F(PreviewsLoggerTest, NotifyObserversWhenUserNotBlocklisted) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);
  logger_->OnUserBlocklistedStatusChange(false /* blocklisted */);
  const size_t expected_times = 2;

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_EQ(expected_times, observers[i].user_status_change_calls());
      EXPECT_FALSE(observers[i].user_blocklisted());
    }
  }
  EXPECT_EQ(expected_times - 1,
            observers[removed_observer].user_status_change_calls());
}

TEST_F(PreviewsLoggerTest, NotifyObserversWhenBlocklistCleared) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);

  const base::Time expected_time = base::Time::Now();
  logger_->OnBlocklistCleared(expected_time);

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_TRUE(observers[i].blocklist_cleared_called());
      EXPECT_EQ(expected_time, observers[i].blocklist_cleared_time());
    }
  }
  EXPECT_FALSE(observers[removed_observer].blocklist_cleared_called());
}

TEST_F(PreviewsLoggerTest, ObserverNotifiedOfUserBlocklistedStateWhenAdded) {
  TestPreviewsLoggerObserver observer;

  const std::string host0 = "example0.com";
  const std::string host1 = "example1.com";
  const base::Time time0 = base::Time::Now();
  const base::Time time1 = base::Time::Now();

  logger_->OnUserBlocklistedStatusChange(true /* blocklisted */);
  logger_->OnNewBlocklistedHost(host0, time0);
  logger_->OnNewBlocklistedHost(host1, time1);
  logger_->AddAndNotifyObserver(&observer);

  const size_t expected_times = 1;
  EXPECT_EQ(expected_times, observer.user_status_change_calls());
  const size_t expected_size = 2;
  EXPECT_EQ(expected_size, observer.blocklisted_hosts().size());
  EXPECT_EQ(time0, observer.blocklisted_hosts().find(host0)->second);
  EXPECT_EQ(time1, observer.blocklisted_hosts().find(host1)->second);
}

TEST_F(PreviewsLoggerTest, NotifyObserversBlocklistIgnoredUpdate) {
  TestPreviewsLoggerObserver observers[3];
  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  for (size_t i = 0; i < number_of_obs; i++) {
    EXPECT_FALSE(observers[i].blocklist_ignored());
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);
  logger_->OnIgnoreBlocklistDecisionStatusChanged(true /* ignored */);
  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_TRUE(observers[i].blocklist_ignored());
    }
  }
  EXPECT_FALSE(observers[removed_observer].blocklist_ignored());
}

TEST_F(PreviewsLoggerTest, ObserverNotifiedOfBlocklistIgnoreStatusOnAdd) {
  logger_->OnIgnoreBlocklistDecisionStatusChanged(true /* ignored */);
  TestPreviewsLoggerObserver observer;
  EXPECT_FALSE(observer.blocklist_ignored());
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_TRUE(observer.blocklist_ignored());
}

TEST_F(PreviewsLoggerTest,
       ObserverNotifiedOfBlocklistIgnoreStatusDisabledViaFlag) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIgnorePreviewsBlocklist));

  TestPreviewsLoggerObserver observer;
  PreviewsLogger logger;
  logger.AddAndNotifyObserver(&observer);
  EXPECT_FALSE(observer.blocklist_ignored());
}

TEST_F(PreviewsLoggerTest,
       ObserverNotifiedOfBlocklistIgnoreStatusEnabledViaFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kIgnorePreviewsBlocklist);
  ASSERT_TRUE(switches::ShouldIgnorePreviewsBlocklist());

  TestPreviewsLoggerObserver observer;
  PreviewsLogger logger;
  logger.AddAndNotifyObserver(&observer);
  EXPECT_TRUE(observer.blocklist_ignored());
}

TEST_F(PreviewsLoggerTest, LastObserverRemovedIsNotified) {
  TestPreviewsLoggerObserver observers[3];
  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->RemoveObserver(&observers[i]);
  }
  EXPECT_TRUE(observers[number_of_obs - 1].last_removed_notified());
}

TEST_F(PreviewsLoggerTest, ClearBufferLogsWhenBlocklistCleared) {
  const std::string type = "Event_";
  const std::string description = "Some description";
  const GURL url("http://www.url_.com/url_");
  const base::Time now = base::Time::Now();
  const uint64_t page_id = 1234;

  logger_->LogMessage(type, description, url, now, page_id);

  logger_->OnBlocklistCleared(base::Time::Now());

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_EQ(0UL, observer.messages().size());
}

}  // namespace

}  // namespace previews
