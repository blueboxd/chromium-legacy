// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::AnyOf;
using testing::ElementsAre;

namespace blink {

// When a page is backgrounded this is the absolute smallest amount of time
// that can elapse between timer wake-ups.
constexpr auto kDefaultThrottledWakeUpInterval =
    base::TimeDelta::FromSeconds(1);

// A SimTest with mock time.
class ThrottlingTestBase : public SimTest {
 public:
  ThrottlingTestBase() {
    platform_->SetAutoAdvanceNowToPendingTasks(false);

    // Align the time on a 1-minute interval, to simplify expectations.
    platform_->AdvanceClock(
        platform_->NowTicks().SnappedToNextTick(
            base::TimeTicks(), base::TimeDelta::FromMinutes(1)) -
        platform_->NowTicks());
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

class DisableBackgroundThrottlingIsRespectedTest
    : public ThrottlingTestBase,
      private ScopedTimerThrottlingForBackgroundTabsForTest {
 public:
  DisableBackgroundThrottlingIsRespectedTest()
      : ScopedTimerThrottlingForBackgroundTabsForTest(false) {}
};

TEST_F(DisableBackgroundThrottlingIsRespectedTest,
       DisableBackgroundThrottlingIsRespected) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 10, repetitions - 1);"
      "  }"
      "  f(5);"
      "</script>)");

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // Run delayed tasks for 1 second. All tasks should be completed
  // with throttling disabled.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(1));

  EXPECT_THAT(ConsoleMessages(), ElementsAre("called f", "called f", "called f",
                                             "called f", "called f"));
}

class BackgroundPageThrottlingTest : public ThrottlingTestBase {};

TEST_F(BackgroundPageThrottlingTest, TimersThrottledInBackgroundPage) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 10, repetitions - 1);"
      "  }"
      "  setTimeout(f, 10, 50);"
      "</script>)");

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // Make sure that we run no more than one task a second.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(ConsoleMessages(),
              ElementsAre("called f", "called f", "called f"));
}

// Same test as above, but using timeout=0.
TEST_F(BackgroundPageThrottlingTest,
       ZeroTimeoutTimersThrottledInBackgroundPage) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 0, repetitions - 1);"
      "  }"
      "  setTimeout(f, 0, 50);"
      "</script>)");

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // 0ms timeouts are rounded up to 1ms (https://crbug.com/402694). When the
  // nesting level is 5, they are rounded up to 4 ms. The duration of a
  // throttled wake up is 3ms. Therefore, at the 2 first wake ups, the timer
  // runs twice. At the third wake up, it runs once.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called f", "called f", "called f",
                                             "called f", "called f"));
}

namespace {

class OptOutZeroTimeoutFromThrottlingTest : public ThrottlingTestBase {
 public:
  OptOutZeroTimeoutFromThrottlingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOptOutZeroTimeoutTimersFromThrottling);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Verify that in a hidden page, when the kOptOutZeroTimeoutTimersFromThrottling
// feature is enabled:
// - setTimeout(..., 0) and setTimeout(..., -1) schedule their callback after
//   1ms. The 1 ms delay exists for historical reasons crbug.com/402694.
// - setTimeout(..., 5) schedules its callback at the next aligned time
TEST_F(OptOutZeroTimeoutFromThrottlingTest, WithoutNesting) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<script>"
      "  setTimeout(function() {"
      "    setTimeout(function() { console.log('setTimeout 0'); }, 0);"
      "    setTimeout(function() { console.log('setTimeout -1'); }, -1);"
      "    setTimeout(function() { console.log('setTimeout 5'); }, 5);"
      "  }, 1000);"
      "</script>");
  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1001));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("setTimeout 0", "setTimeout -1"));

  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(998));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("setTimeout 0", "setTimeout -1"));

  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(),
              ElementsAre("setTimeout 0", "setTimeout -1", "setTimeout 5"));
}

// Verify that in a hidden page, when the kOptOutZeroTimeoutTimersFromThrottling
// feature is enabled, a timer created with setTimeout(..., 0) is throttled
// after 5 nesting levels.
TEST_F(OptOutZeroTimeoutFromThrottlingTest, SetTimeoutNesting) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<script>"
      "  function f(repetitions) {"
      "    if (repetitions == 0) return;"
      "    console.log('called f');"
      "    setTimeout(f, 0, repetitions - 1);"
      "  }"
      "  setTimeout(f, 0, 50);"
      "</script>");
  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(1, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(2, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(3, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(4, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(995));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(4, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(5, "called f"));
}

// Verify that in a hidden page, when the kOptOutZeroTimeoutTimersFromThrottling
// feature is enabled, a timer created with setInterval(..., 0) is throttled
// after 5 nesting levels.
TEST_F(OptOutZeroTimeoutFromThrottlingTest, SetIntervalNesting) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<script>"
      "  function f() {"
      "    if (repetitions == 0) clearInterval(interval_id);"
      "    console.log('called f');"
      "    repetitions = repetitions - 1;"
      "  }"
      "  var repetitions = 50;"
      "  var interval_id = setInterval(f, 0);"
      "</script>");
  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(1, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(2, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(3, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(4, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(995));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(4, "called f"));
  platform_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(ConsoleMessages(), Vector<String>(5, "called f"));
}

namespace {

class IntensiveWakeUpThrottlingTest : public ThrottlingTestBase {
 public:
  IntensiveWakeUpThrottlingTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIntensiveWakeUpThrottling},
        // Disable freezing because it hides the effect of intensive throttling.
        {features::kStopInBackground});
  }

  void TestNoIntensiveThrotlingOnTitleOrFaviconUpdate() {
    // The page does not attempt to run onTimer in the first 5 minutes.
    platform_->RunForPeriod(base::TimeDelta::FromMinutes(5));
    EXPECT_THAT(ConsoleMessages(), ElementsAre());

    // At 5 minutes, a timer fires to run the afterFiveMinutes() function.
    // This function does not communicate in the background, so the intensive
    // throttling policy applies and onTimer() can only run after 1 minute.
    platform_->RunForPeriod(base::TimeDelta::FromMinutes(1));
    EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));

    ConsoleMessages().clear();

    // Beyond this point intensive background throttling will not apply anymore
    // since the page is communicating in the background from onTimer().

    constexpr auto kTimeUntilNextCheck = base::TimeDelta::FromSeconds(30);
    platform_->RunForPeriod(kTimeUntilNextCheck);

    // Tasks are not throttled beyond the default background throttling behavior
    // nor do they get to run more often.
    Vector<String> expected_ouput(
        kTimeUntilNextCheck.IntDiv(kDefaultThrottledWakeUpInterval),
        "called onTimer");
    EXPECT_THAT(ConsoleMessages(), expected_ouput);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Use to install a function that does not actually communicate with the user.
constexpr char kCommunicationNop[] =
    "<script>"
    "  function maybeCommunicateInBackground() {"
    "    return;"
    "  }"
    "</script>";

// Use to install a function that will communicate with the user via title
// update.
constexpr char kCommunicateThroughTitleScript[] =
    "<script>"
    "  function maybeCommunicateInBackground() {"
    "    document.title += \"A\";"
    "  }"
    "</script>";

// Use to install a function that will communicate with the user via favicon
// update.
constexpr char kCommunicateThroughFavisonScript[] =
    "<script>"
    "  function maybeCommunicateInBackground() {"
    "  document.querySelector(\"link[rel*='icon']\").href = \"favicon.ico\";"
    "  }"
    "</script>";

// A script that schedules a timer with a long delay that is not aligned on the
// intensive throttling wake up interval.
constexpr char kLongUnalignedTimerScript[] =
    "<script>"
    "  function onTimer() {"
    "     console.log('called onTimer');"
    "  }"
    "  setTimeout(onTimer, 342 * 1000);"
    "</script>";

// A time delta that matches the delay in the above script.
constexpr base::TimeDelta kLongUnalignedTimerDelay =
    base::TimeDelta::FromSeconds(342);

// Use to build a web-page ready to test intensive javascript throttling.
// The page will differ in its definition of the maybeCommunicateInBackground()
// function which has to be defined in a script passed in |communicate_script|.
String BuildRepeatingTimerPage(const char* communicate_script) {
  // A template for a page that waits 5 minutes on load then creates a timer
  // that reschedules itself 50 times with 10 ms delay. Contains the minimimal
  // page structure to simulate background communication with the user via title
  // or favicon update. Needs to be augmented with a definition for
  // maybeCommunicateInBackground;
  constexpr char kRepeatingTimerPageTemplate[] =
      "<html>"
      "<head>"
      "  <link rel='icon' href='http://www.foobar.com/favicon.ico'>"
      "</head>"
      "<body>"
      "<script>"
      "  function onTimer(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called onTimer');"
      "     maybeCommunicateInBackground();"
      "     setTimeout(onTimer, 10, repetitions - 1);"
      "  }"
      "  function afterFiveMinutes() {"
      "    setTimeout(onTimer, 10, 50);"
      "  }"
      "  setTimeout(afterFiveMinutes, 5 * 60 * 1000);"
      "</script>"
      "%s"  // maybeCommunicateInBackground definition inserted here.
      "</body>"
      "</html>";

  std::string page =
      base::StringPrintf(kRepeatingTimerPageTemplate, communicate_script);

  return {page.data(), page.size()};
}

}  // namespace

// Verify that a main frame timer that reposts itself with a 10 ms timeout runs
// once every minute.
TEST_F(IntensiveWakeUpThrottlingTest, MainFrameTimer_ShortTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  // Page does not communicate with the user. Normal intensive throttling
  // applies.
  main_resource.Complete(BuildRepeatingTimerPage(kCommunicationNop));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // No timer is scheduled in the 5 first minutes.
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(5));
  EXPECT_THAT(ConsoleMessages(), ElementsAre());

  // After that, intensive throttling starts and there should be 1 wake up per
  // minute.
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));

  // No tasks execute early.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(30));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));

  // A minute after the last timer.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(30));
  EXPECT_THAT(ConsoleMessages(),
              ElementsAre("called onTimer", "called onTimer"));
}

// Verify that a main frame timer that reposts itself with a 10 ms timeout runs
// once every |kDefaultThrottledWakeUpInterval| after the first confirmed page
// communication through title update.
TEST_F(IntensiveWakeUpThrottlingTest, MainFrameTimer_ShortTimeout_TitleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      BuildRepeatingTimerPage(kCommunicateThroughTitleScript));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  TestNoIntensiveThrotlingOnTitleOrFaviconUpdate();
}

// Verify that a main frame timer that reposts itself with a 10 ms timeout runs
// once every |kDefaultThrottledWakeUpInterval| after the first confirmed page
// communication through favicon update.
TEST_F(IntensiveWakeUpThrottlingTest,
       MainFrameTimer_ShortTimeout_FaviconUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      BuildRepeatingTimerPage(kCommunicateThroughFavisonScript));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  TestNoIntensiveThrotlingOnTitleOrFaviconUpdate();
}

// Verify that a same-origin subframe timer that reposts itself with a 10 ms
// timeout runs once every minute.
TEST_F(IntensiveWakeUpThrottlingTest, SameOriginSubFrameTimer_ShortTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(<iframe src="https://example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();
  subframe_resource.Complete(BuildRepeatingTimerPage(kCommunicationNop));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // No timer is scheduled in the 5 first minutes.
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(5));
  EXPECT_THAT(ConsoleMessages(), ElementsAre());

  // After that, intensive throttling starts and there should be 1 wake up per
  // minute.
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(ConsoleMessages(),
              ElementsAre("called onTimer", "called onTimer"));
}

// Verify that a cross-origin subframe timer that reposts itself with a 10 ms
// timeout runs once every minute.
TEST_F(IntensiveWakeUpThrottlingTest, CrossOriginSubFrameTimer_ShortTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://cross-origin.example.com/iframe.html",
                               "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"(<iframe src="https://cross-origin.example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();
  subframe_resource.Complete(BuildRepeatingTimerPage(kCommunicationNop));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // No timer is scheduled in the 5 first minutes.
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(5));
  EXPECT_THAT(ConsoleMessages(), ElementsAre());

  // After that, intensive throttling starts and there should be 1 wake up per
  // minute.
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));
  platform_->RunForPeriod(base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(ConsoleMessages(),
              ElementsAre("called onTimer", "called onTimer"));
}

// Verify that a main frame timer with a long timeout runs at the desired run
// time when there is no other recent timer wake up.
TEST_F(IntensiveWakeUpThrottlingTest, MainFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(kLongUnalignedTimerScript);

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(kLongUnalignedTimerDelay -
                          base::TimeDelta::FromSeconds(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre());

  platform_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));
}

// Verify that a same-origin subframe timer with a long timeout runs at the
// desired run time when there is no other recent timer wake up.
TEST_F(IntensiveWakeUpThrottlingTest,
       SameOriginSubFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(<iframe src="https://example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();
  subframe_resource.Complete(kLongUnalignedTimerScript);

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(kLongUnalignedTimerDelay -
                          base::TimeDelta::FromSeconds(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre());

  platform_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));
}

// Verify that a cross-origin subframe timer with a long timeout runs at an
// aligned time, even when there is no other recent timer wake up (in a
// same-origin frame, it would have run at the desired time).
TEST_F(IntensiveWakeUpThrottlingTest,
       CrossOriginSubFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://cross-origin.example.com/iframe.html",
                               "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"(<iframe src="https://cross-origin.example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();
  subframe_resource.Complete(kLongUnalignedTimerScript);

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::TimeDelta::FromSeconds(342));
  EXPECT_THAT(ConsoleMessages(), ElementsAre());

  // Fast-forward to the next aligned time.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(18));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));
}

// Verify that if both the main frame and a cross-origin frame schedule a timer
// with a long unaligned delay, the main frame timer runs at the desired time
// (because there was no recent same-origin wake up) while the cross-origin
// timer runs at an aligned time.
TEST_F(IntensiveWakeUpThrottlingTest,
       MainFrameAndCrossOriginSubFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://cross-origin.example.com/iframe.html",
                               "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      WTF::String(kLongUnalignedTimerScript) +
      "<iframe src=\"https://cross-origin.example.com/iframe.html\" />");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();
  subframe_resource.Complete(kLongUnalignedTimerScript);

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::TimeDelta::FromSeconds(342));
  EXPECT_THAT(ConsoleMessages(), ElementsAre("called onTimer"));

  // Fast-forward to the next aligned time.
  platform_->RunForPeriod(base::TimeDelta::FromSeconds(18));
  EXPECT_THAT(ConsoleMessages(),
              ElementsAre("called onTimer", "called onTimer"));
}

}  // namespace blink
