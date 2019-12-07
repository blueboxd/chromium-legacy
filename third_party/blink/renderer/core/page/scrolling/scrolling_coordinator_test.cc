/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class ScrollingCoordinatorTest : public testing::Test {
 public:
  ScrollingCoordinatorTest() : base_url_("http://www.test.com/") {
    helper_.Initialize(nullptr, nullptr, nullptr, &ConfigureSettings);
    GetWebView()->MainFrameWidget()->Resize(IntSize(320, 240));
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  ~ScrollingCoordinatorTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(), html,
                                       url_test_helpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

 protected:
  std::string base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(ScrollingCoordinatorTest, UpdateUMAMetricUpdated) {
  HistogramTester histogram_tester;
  LoadHTML(R"HTML(
    <div id='bg' style='background: blue;'></div>
    <div id='scroller' style='overflow: scroll; width: 10px; height: 10px; background: blue'>
      <div id='forcescroll' style='height: 1000px;'></div>
    </div>
  )HTML");

  // The initial counts should be zero.
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 0);

  // After an initial compositing update, we should have one scrolling update
  // recorded as PreFCP.
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 0);

  // An update with no scrolling changes should not cause a scrolling update.
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 0);

  // A change to background color does not need to cause a scrolling update but,
  // because hit test display items paint, we also cause a scrolling coordinator
  // update when the background paints. Also render some text to get past FCP.
  auto* background = GetFrame()->GetDocument()->getElementById("bg");
  background->removeAttribute(html_names::kStyleAttr);
  background->SetInnerHTMLFromString("Some Text");
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 2);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 1);

  // Removing a scrollable area should cause a scrolling update.
  auto* scroller = GetFrame()->GetDocument()->getElementById("scroller");
  scroller->removeAttribute(html_names::kStyleAttr);
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 3);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 2);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 1);
}

}  // namespace blink
