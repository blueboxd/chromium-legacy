// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/browser/ui/browser.h"

namespace safe_browsing {

namespace {

constexpr base::TimeDelta kNoDelay = base::TimeDelta::FromSeconds(0);
constexpr base::TimeDelta kSmallDelay = base::TimeDelta::FromMilliseconds(300);
constexpr base::TimeDelta kNormalDelay = base::TimeDelta::FromMilliseconds(500);

enum class ScanType { ONLY_DLP, ONLY_MALWARE, DLP_AND_MALWARE };

// Tests the behavior of the dialog in the following ways:
// - It shows the appropriate buttons depending on it's state.
// - It transitions from states in the correct order.
// - It respects time constraints (minimum shown time, initial delay, timeout)
// - It is always destroyed, therefore |quit_closure_| is called in the dtor
//   observer.
class DeepScanningDialogViewsBehaviorBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver,
      public testing::WithParamInterface<
          std::tuple<ScanType, bool, bool, base::TimeDelta>> {
 public:
  DeepScanningDialogViewsBehaviorBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);

    bool dlp_result = !dlp_enabled() || dlp_success();
    bool malware_result = !malware_enabled() || malware_success();
    expected_scan_result_ = dlp_result && malware_result;
  }

  void ConstructorCalled(DeepScanningDialogViews* views) override {
    ctor_called_timestamp_ = base::TimeTicks::Now();
    dialog_ = views;

    // The scan should be pending when constructed.
    EXPECT_TRUE(dialog_->is_pending());

    // The dialog should only be constructed once.
    EXPECT_FALSE(ctor_called_);
    ctor_called_ = true;
  }

  void ViewsFirstShown(DeepScanningDialogViews* views,
                       base::TimeTicks timestamp) override {
    DCHECK_EQ(views, dialog_);
    first_shown_timestamp_ = timestamp;

    // The dialog should only be shown after an initial delay.
    base::TimeDelta delay = first_shown_timestamp_ - ctor_called_timestamp_;
    EXPECT_GE(delay, DeepScanningDialogViews::GetInitialUIDelay());

    // The dialog can only be first shown in the pending or failure case.
    EXPECT_TRUE(dialog_->is_pending() || dialog_->is_failure());

    // If the failure dialog was shown immediately, ensure that was expected and
    // set |pending_shown_| for future assertions.
    if (dialog_->is_failure()) {
      EXPECT_FALSE(expected_scan_result_);
      pending_shown_ = false;
    } else {
      pending_shown_ = true;
    }

    // The dialog's buttons should be Cancel in the pending and fail case.
    EXPECT_EQ(dialog_->GetDialogButtons(), ui::DIALOG_BUTTON_CANCEL);

    // The dialog should only be shown once some time after being constructed.
    EXPECT_TRUE(ctor_called_);
    EXPECT_FALSE(views_first_shown_);
    views_first_shown_ = true;
  }

  void DialogUpdated(DeepScanningDialogViews* views, bool result) override {
    DCHECK_EQ(views, dialog_);
    dialog_updated_timestamp_ = base::TimeTicks::Now();

    // The dialog should not be updated if the failure was shown immediately.
    EXPECT_TRUE(pending_shown_);

    // The dialog should only be updated after an initial delay.
    base::TimeDelta delay = dialog_updated_timestamp_ - first_shown_timestamp_;
    EXPECT_GE(delay, DeepScanningDialogViews::GetMinimumPendingDialogTime());

    // The dialog can only be updated to the success or failure case.
    EXPECT_TRUE(dialog_->is_result());
    EXPECT_EQ(dialog_->is_success(), result);
    EXPECT_EQ(dialog_->is_success(), expected_scan_result_);

    // The dialog's buttons should be Cancel in the fail case and nothing in the
    // success case.
    ui::DialogButton expected_buttons =
        result ? ui::DIALOG_BUTTON_NONE : ui::DIALOG_BUTTON_CANCEL;
    EXPECT_EQ(expected_buttons, dialog_->GetDialogButtons());

    // The dialog should only be updated once some time after being shown.
    EXPECT_TRUE(views_first_shown_);
    EXPECT_FALSE(dialog_updated_);
    dialog_updated_ = true;
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    dtor_called_timestamp_ = base::TimeTicks::Now();

    EXPECT_TRUE(views);
    EXPECT_EQ(dialog_, views);
    EXPECT_EQ(dialog_->is_success(), expected_scan_result_);

    if (views_first_shown_) {
      // Ensure the dialog update only occurred if the pending state was shown.
      EXPECT_EQ(pending_shown_, dialog_updated_);

      // Ensure the success UI timed out properly.
      EXPECT_TRUE(dialog_->is_result());
      if (dialog_->is_success()) {
        // The success dialog should stay open for some time.
        base::TimeDelta delay =
            dtor_called_timestamp_ - dialog_updated_timestamp_;
        EXPECT_GE(delay, DeepScanningDialogViews::GetSuccessDialogTimeout());

        EXPECT_EQ(ui::DIALOG_BUTTON_NONE, dialog_->GetDialogButtons());
      } else {
        EXPECT_EQ(ui::DIALOG_BUTTON_CANCEL, dialog_->GetDialogButtons());
      }
    } else {
      // Ensure the dialog update didn't occur if no dialog was shown.
      EXPECT_FALSE(dialog_updated_);
    }
    EXPECT_TRUE(ctor_called_);

    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  bool dlp_enabled() const {
    return std::get<0>(GetParam()) != ScanType::ONLY_MALWARE;
  }

  bool malware_enabled() const {
    return std::get<0>(GetParam()) != ScanType::ONLY_DLP;
  }

  bool dlp_success() const { return std::get<1>(GetParam()); }

  bool malware_success() const { return std::get<2>(GetParam()); }

  base::TimeDelta response_delay() const { return std::get<3>(GetParam()); }

 private:
  DeepScanningDialogViews* dialog_;

  base::TimeTicks ctor_called_timestamp_;
  base::TimeTicks first_shown_timestamp_;
  base::TimeTicks dialog_updated_timestamp_;
  base::TimeTicks dtor_called_timestamp_;

  bool pending_shown_ = false;
  bool ctor_called_ = false;
  bool views_first_shown_ = false;
  bool dialog_updated_ = false;

  bool expected_scan_result_;
};

// Tests the behavior of the dialog in the following ways:
// - It closes when the "Cancel" button is clicked.
// - It returns a negative verdict on the scanned content.
// - The "CancelledByUser" metrics are recorded.
class DeepScanningDialogViewsCancelPendingScanBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver {
 public:
  DeepScanningDialogViewsCancelPendingScanBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);
  }

  void ViewsFirstShown(DeepScanningDialogViews* views,
                       base::TimeTicks timestamp) override {
    // Simulate the user clicking "Cancel" after the dialog is first shown.
    views->CancelDialog();
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  void ValidateMetrics() const {
    ASSERT_EQ(
        2u,
        histograms_.GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
    ASSERT_EQ(1u, histograms_
                      .GetTotalCountsForPrefix(
                          "SafeBrowsing.DeepScan.Upload.Duration")
                      .size());
    ASSERT_EQ(1u,
              histograms_
                  .GetTotalCountsForPrefix(
                      "SafeBrowsing.DeepScan.Upload.CancelledByUser.Duration")
                  .size());
  }

 private:
  base::HistogramTester histograms_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(DeepScanningDialogViewsBehaviorBrowserTest, Test) {
  // The test is wrong if neither DLP or Malware is enabled. This would imply a
  // Deep Scanning call site called ShowForWebContents without first checking
  // IsEnabled returns true.
  EXPECT_TRUE(dlp_enabled() || malware_enabled());

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  base::Optional<bool> dlp = base::nullopt;
  base::Optional<bool> malware = base::nullopt;
  if (dlp_enabled()) {
    dlp = dlp_success();
    SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  }
  if (malware_enabled()) {
    malware = malware_success();
    SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  }
  SetStatusCallbackResponse(
      SimpleDeepScanningClientResponseForTesting(dlp, malware));

  // Always set this policy so the UI is shown.
  SetWaitPolicy(DELAY_UPLOADS);

  // Set up delegate test values.
  FakeDeepScanningDialogDelegate::SetResponseDelay(response_delay());
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = dlp_enabled();
  data.do_malware_scan = malware_enabled();
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            *called = true;
          },
          &called),
      DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);
}

// The scan type controls if DLP, malware or both are enabled via policies. The
// dialog currently behaves identically in all 3 cases, so this parameter
// ensures this assumption is not broken by new code.
//
// The DLP/Malware success parameters determine how the response is populated,
// and therefore what the dialog should show.
//
// The three different delays test three cases:
// kNoDelay: The response is as fast as possible, and therefore the pending
//           UI is not shown (kNoDelay < GetInitialUIDelay).
// kSmallDelay: The response is not fast enough to prevent the pending UI from
//              showing, but fast enough that it hasn't been show long enough
//              (GetInitialDelay < kSmallDelay < GetMinimumPendingDialogTime).
// kNormalDelay: The response is slow enough that the pending UI is shown for
//               more than its minimum duration (GetMinimumPendingDialogTime <
//               kNormalDelay).
INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogViewsBehaviorBrowserTest,
    DeepScanningDialogViewsBehaviorBrowserTest,
    testing::Combine(
        /*scan_type*/ testing::Values(ScanType::ONLY_DLP,
                                      ScanType::ONLY_MALWARE,
                                      ScanType::DLP_AND_MALWARE),
        /*dlp_success*/ testing::Bool(),
        /*malware_success*/ testing::Bool(),
        /*response_delay*/
        testing::Values(kNoDelay, kSmallDelay, kNormalDelay)));

IN_PROC_BROWSER_TEST_F(DeepScanningDialogViewsCancelPendingScanBrowserTest,
                       Test) {
  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  SetDlpPolicy(CHECK_UPLOADS);
  SetStatusCallbackResponse(SimpleDeepScanningClientResponseForTesting(
      /*dlp=*/true, /*malware=*/base::nullopt));

  // Always set this policy so the UI is shown.
  SetWaitPolicy(DELAY_UPLOADS);

  // Set up delegate test values.
  FakeDeepScanningDialogDelegate::SetResponseDelay(kSmallDelay);
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = false;
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/baz.doc"));

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            for (bool result : result.paths_results)
              ASSERT_FALSE(result);
            *called = true;
          },
          &called),
      DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);

  ValidateMetrics();
}

}  // namespace safe_browsing
