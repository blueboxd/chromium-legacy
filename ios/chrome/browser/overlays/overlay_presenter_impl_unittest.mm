// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_presenter_impl.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/overlay_request_queue_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presenter_ui_delegate.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Mock queue observer.
class MockOverlayPresenterObserver : public OverlayPresenter::Observer {
 public:
  MockOverlayPresenterObserver() {}
  ~MockOverlayPresenterObserver() {}

  MOCK_METHOD2(WillShowOverlay, void(OverlayPresenter*, OverlayRequest*));
  MOCK_METHOD1(DidHideOverlay, void(OverlayPresenter*));

  // OverlayPresenter's ObserverList checks that it is empty upon deallocation,
  // so a custom verification implemetation must be created.
  void OverlayPresenterDestroyed(OverlayPresenter* presenter) override {
    presenter_destroyed_ = true;
    presenter->RemoveObserver(this);
  }
  bool presenter_destroyed() const { return presenter_destroyed_; }

 private:
  bool presenter_destroyed_ = false;
};
}  // namespace

// Test fixture for OverlayPresenterImpl.
class OverlayPresenterImplTest : public PlatformTest {
 public:
  OverlayPresenterImplTest() : web_state_list_(&web_state_list_delegate_) {
    TestChromeBrowserState::Builder browser_state_builder;
    chrome_browser_state_ = browser_state_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             &web_state_list_);
    OverlayPresenterImpl::Container::CreateForUserData(browser_.get(),
                                                       browser_.get());
    presenter().AddObserver(&observer_);
  }
  ~OverlayPresenterImplTest() override {
    if (browser_)
      presenter().RemoveObserver(&observer_);
  }

  WebStateList& web_state_list() { return web_state_list_; }
  web::WebState* active_web_state() {
    return web_state_list_.GetActiveWebState();
  }
  OverlayPresenterImpl& presenter() {
    return *OverlayPresenterImpl::Container::FromUserData(browser_.get())
                ->PresenterForModality(OverlayModality::kWebContentArea);
  }
  FakeOverlayPresenterUIDelegate& ui_delegate() { return ui_delegate_; }
  MockOverlayPresenterObserver& observer() { return observer_; }

  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state) {
    if (!web_state)
      return nullptr;
    OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
    return OverlayRequestQueueImpl::Container::FromWebState(web_state)
        ->QueueForModality(OverlayModality::kWebContentArea);
  }

  OverlayRequest* AddRequest(web::WebState* web_state,
                             bool expect_presentation = true) {
    OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
    if (!queue)
      return nullptr;
    std::unique_ptr<OverlayRequest> passed_request =
        OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
    OverlayRequest* request = passed_request.get();
    if (expect_presentation)
      EXPECT_CALL(observer(), WillShowOverlay(&presenter(), request));
    GetQueueForWebState(web_state)->AddRequest(std::move(passed_request));
    return request;
  }

  void DeleteBrowser() { browser_ = nullptr; }

 private:
  web::TestWebThreadBundle thread_bundle_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FakeOverlayPresenterUIDelegate ui_delegate_;
  MockOverlayPresenterObserver observer_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
};

// Tests that setting the UI delegate will present overlays requested before the
// delegate is provided.
TEST_F(OverlayPresenterImplTest, PresentAfterSettingUIDelegate) {
  // Add a WebState to the list and add a request to that WebState's queue.
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  OverlayRequest* request = AddRequest(active_web_state());
  ASSERT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kNotPresented,
            ui_delegate().GetPresentationState(request));

  // Set the UI delegate and verify that the request has been presented.
  presenter().SetUIDelegate(&ui_delegate());
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(request));
}

// Tests that requested overlays are presented when added to the active queue
// after the UI delegate has been provided.
TEST_F(OverlayPresenterImplTest, PresentAfterRequestAddedToActiveQueue) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  OverlayRequest* request = AddRequest(active_web_state());
  // Verify that the requested overlay has been presented.
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(request));
}

// Tests resetting the UI delegate.  The UI should be cancelled in the previous
// UI delegate and presented in the new delegate.
TEST_F(OverlayPresenterImplTest, ResetUIDelegate) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);

  ASSERT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(request));

  // Reset the UI delegate and verify that the overlay UI is cancelled in the
  // previous delegate's context and presented in the new delegate's context.
  FakeOverlayPresenterUIDelegate new_ui_delegate;
  EXPECT_CALL(observer(), WillShowOverlay(&presenter(), request));
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  presenter().SetUIDelegate(&new_ui_delegate);
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(request));
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            new_ui_delegate.GetPresentationState(request));
  EXPECT_EQ(request, queue->front_request());

  // Reset the UI delegate to nullptr and verify that the overlay UI is
  // cancelled in |new_ui_delegate|'s context.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  presenter().SetUIDelegate(nullptr);
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kCancelled,
            new_ui_delegate.GetPresentationState(request));
  EXPECT_EQ(request, queue->front_request());
}

// Tests changing the active WebState while no overlays are presented over the
// current active WebState.
TEST_F(OverlayPresenterImplTest, ChangeActiveWebStateWhileNotPresenting) {
  // Add a WebState to the list and activate it.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());

  // Create a new WebState with a queued request and add it as the new active
  // WebState.
  std::unique_ptr<web::WebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::WebState* second_web_state = passed_web_state.get();
  OverlayRequest* request = AddRequest(second_web_state);
  web_state_list().InsertWebState(/*index=*/1, std::move(passed_web_state),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());

  // Verify that the new active WebState's overlay is being presented.
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(request));
}

// Tests changing the active WebState while is it presenting an overlay.
TEST_F(OverlayPresenterImplTest, ChangeActiveWebStateWhilePresenting) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  web::WebState* first_web_state = active_web_state();
  OverlayRequest* first_request = AddRequest(first_web_state);
  ASSERT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_request));

  // Create a new WebState with a queued request and add it as the new active
  // WebState.
  std::unique_ptr<web::WebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::WebState* second_web_state = passed_web_state.get();
  OverlayRequest* second_request = AddRequest(second_web_state);
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  web_state_list().InsertWebState(/*index=*/1, std::move(passed_web_state),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());

  // Verify that the previously shown overlay is hidden and that the overlay for
  // the new active WebState is presented.
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kHidden,
            ui_delegate().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(second_request));

  // Reactivate the first WebState and verify that its overlay is presented
  // while the second WebState's overlay is hidden.
  EXPECT_CALL(observer(), WillShowOverlay(&presenter(), first_request));
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  web_state_list().ActivateWebStateAt(0);
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kHidden,
            ui_delegate().GetPresentationState(second_request));
}

// Tests replacing the active WebState while it is presenting an overlay.
TEST_F(OverlayPresenterImplTest, ReplaceActiveWebState) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  web::WebState* first_web_state = active_web_state();
  OverlayRequest* first_request = AddRequest(first_web_state);
  ASSERT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_request));

  // Replace |first_web_state| with a new active WebState with a queued request.
  std::unique_ptr<web::WebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::WebState* replacement_web_state = passed_web_state.get();
  OverlayRequest* replacement_request = AddRequest(replacement_web_state);
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  web_state_list().ReplaceWebStateAt(/*index=*/0, std::move(passed_web_state));

  // Verify that the previously shown overlay is canceled and that the overlay
  // for the replacement WebState is presented.
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(replacement_request));
}

// Tests removing the active WebState while it is presenting an overlay.
TEST_F(OverlayPresenterImplTest, RemoveActiveWebState) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(request));

  // Remove the WebState and verify that its overlay was cancelled.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  web_state_list().CloseWebStateAt(/*index=*/0, /* close_flags= */ 0);
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(request));
}

// Tests dismissing an overlay for user interaction.
TEST_F(OverlayPresenterImplTest, DismissForUserInteraction) {
  // Add a WebState to the list and add two request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(
      /*index=*/0, std::make_unique<web::TestWebState>(),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  web::WebState* web_state = active_web_state();
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  OverlayRequest* first_request = AddRequest(web_state);
  OverlayRequest* second_request = AddRequest(web_state);

  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_request));
  EXPECT_EQ(first_request, queue->front_request());
  EXPECT_EQ(2U, queue->size());

  // Dismiss the overlay and check that the second request's overlay is
  // presented.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  ui_delegate().SimulateDismissalForRequest(
      first_request, OverlayDismissalReason::kUserInteraction);

  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kUserDismissed,
            ui_delegate().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(second_request));
  EXPECT_EQ(second_request, queue->front_request());
  EXPECT_EQ(1U, queue->size());
}

// Tests cancelling the requests.
TEST_F(OverlayPresenterImplTest, CancelRequests) {
  // Add a WebState to the list and a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  OverlayRequestQueueImpl* queue = GetQueueForWebState(active_web_state());
  OverlayRequest* active_request = AddRequest(active_web_state());
  OverlayRequest* queued_request =
      AddRequest(active_web_state(), /*expect_presentation=*/false);

  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(active_request));

  // Cancel the queue's requests and verify that the UI is also cancelled.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter()));
  queue->CancelAllRequests();
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(active_request));
  EXPECT_EQ(FakeOverlayPresenterUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(queued_request));
}

// Tests that deleting the presenter
TEST_F(OverlayPresenterImplTest, PresenterWasDestroyed) {
  DeleteBrowser();
  EXPECT_TRUE(observer().presenter_destroyed());
}
