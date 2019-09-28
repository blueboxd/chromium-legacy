// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include <string>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFormSubmissionSecurityLevelHistogram[] =
    "Security.SecurityLevel.FormSubmission";

// Stores the Insecure Input Events to the entry's SSLStatus user data.
void SetInputEvents(content::NavigationEntry* entry,
                    security_state::InsecureInputEventData events) {
  security_state::SSLStatus& ssl = entry->GetSSL();
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (!input_events) {
    ssl.user_data =
        std::make_unique<security_state::SSLStatusInputEventData>(events);
  } else {
    *input_events->input_events() = events;
  }
}

class SecurityStateTabHelperHistogramTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SecurityStateTabHelperHistogramTest() : helper_(nullptr) {}
  ~SecurityStateTabHelperHistogramTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SecurityStateTabHelper::CreateForWebContents(web_contents());
    helper_ = SecurityStateTabHelper::FromWebContents(web_contents());
    NavigateToHTTP();
  }

 protected:
  void ClearInputEvents() {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetVisibleEntry();
    SetInputEvents(entry, security_state::InsecureInputEventData());
    helper_->DidChangeVisibleSecurityState();
  }

  void StartFormSubmissionNavigation() {
    content::MockNavigationHandle handle(GURL("http://example.test"),
                                         web_contents()->GetMainFrame());
    handle.set_is_form_submission(true);
    helper_->DidStartNavigation(&handle);

    handle.set_has_committed(true);
    helper_->DidFinishNavigation(&handle);
  }

  void NavigateToHTTP() { NavigateAndCommit(GURL("http://example.test")); }

 private:
  SecurityStateTabHelper* helper_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperHistogramTest);
};

TEST_F(SecurityStateTabHelperHistogramTest, FormSubmissionHistogram) {
  base::HistogramTester histograms;
  StartFormSubmissionNavigation();
  histograms.ExpectUniqueSample(kFormSubmissionSecurityLevelHistogram,
                                security_state::WARNING, 1);
}

}  // namespace
