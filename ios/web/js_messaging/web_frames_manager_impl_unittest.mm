// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frames_manager_impl.h"

#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#include "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd8";

// A base64 encoded sample key.
const char kFrameKey[] = "R7lsXtR74c6R9A9k691gUQ8JAd0be+w//Lntgcbjwrc=";

// Message command sent when a frame becomes available.
NSString* const kFrameBecameAvailableMessageName = @"FrameBecameAvailable";
// Message command sent when a frame is unloading.
NSString* const kFrameBecameUnavailableMessageName = @"FrameBecameUnavailable";

// Returns true if |web_frame| is contained in |frames|.
bool ContainsWebFrame(std::set<web::WebFrame*> frames,
                      web::WebFrame* web_frame) {
  return frames.end() != std::find(frames.begin(), frames.end(), web_frame);
}

}  // namespace

namespace web {

class WebFramesManagerImplTest : public PlatformTest,
                                 public WebFramesManagerDelegate {
 protected:
  WebFramesManagerImplTest() : frames_manager_(*this) {}

  // WebFramesManagerDelegate.
  void OnWebFrameAvailable(WebFrame* frame) override {}
  void OnWebFrameUnavailable(WebFrame* frame) override {}
  WebState* GetWebState() override { return &test_web_state_; }

  TestWebState test_web_state_;
  WebFramesManagerImpl frames_manager_;
};

// Tests that the WebFrame for the main frame is returned.
TEST_F(WebFramesManagerImplTest, GetMainWebFrame) {
  GURL security_origin;
  auto web_frame = std::make_unique<WebFrameImpl>(
      "web_frame", /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* web_frame_ptr = web_frame.get();

  frames_manager_.AddFrame(std::move(web_frame));

  EXPECT_EQ(web_frame_ptr, frames_manager_.GetMainWebFrame());
  auto frames = frames_manager_.GetAllWebFrames();
  EXPECT_EQ(1ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, web_frame_ptr));
}

// Tests that the WebFrame returns null if no main frame is known.
TEST_F(WebFramesManagerImplTest, NoMainWebFrame) {
  EXPECT_EQ(nullptr, frames_manager_.GetMainWebFrame());

  GURL security_origin;
  const std::string web_frame_frame_id = "web_frame";
  auto web_frame =
      std::make_unique<WebFrameImpl>(web_frame_frame_id, /*is_main_frame=*/true,
                                     security_origin, &test_web_state_);

  frames_manager_.AddFrame(std::move(web_frame));
  frames_manager_.RemoveFrameWithId(web_frame_frame_id);

  EXPECT_EQ(nullptr, frames_manager_.GetMainWebFrame());
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
}

// Tests that the WebFramesManagerImpl returns a list of all current WebFrame
// objects.
TEST_F(WebFramesManagerImplTest, AddFrames) {
  GURL security_origin;
  auto main_web_frame = std::make_unique<WebFrameImpl>(
      "main_web_frame",
      /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* main_web_frame_ptr = main_web_frame.get();
  frames_manager_.AddFrame(std::move(main_web_frame));

  auto child_web_frame = std::make_unique<WebFrameImpl>(
      "child_web_frame",
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  WebFrameImpl* child_web_frame_ptr = child_web_frame.get();
  frames_manager_.AddFrame(std::move(child_web_frame));

  auto frames = frames_manager_.GetAllWebFrames();
  EXPECT_EQ(2ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, main_web_frame_ptr));
  EXPECT_TRUE(ContainsWebFrame(frames, child_web_frame_ptr));
}

// Tests that the WebFramesManagerImpl correctly removes a WebFrame instance.
TEST_F(WebFramesManagerImplTest, RemoveFrame) {
  GURL security_origin;
  auto main_web_frame = std::make_unique<WebFrameImpl>(
      "main_web_frame",
      /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* main_web_frame_ptr = main_web_frame.get();
  frames_manager_.AddFrame(std::move(main_web_frame));

  const std::string child_web_frame_1_frame_id = "child_web_frame_1_frame_id";
  auto child_web_frame_1 = std::make_unique<WebFrameImpl>(
      child_web_frame_1_frame_id,
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  frames_manager_.AddFrame(std::move(child_web_frame_1));

  auto child_web_frame_2 = std::make_unique<WebFrameImpl>(
      "child_web_frame_2",
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  WebFrameImpl* child_web_frame_2_ptr = child_web_frame_2.get();
  frames_manager_.AddFrame(std::move(child_web_frame_2));

  frames_manager_.RemoveFrameWithId(child_web_frame_1_frame_id);

  auto frames = frames_manager_.GetAllWebFrames();
  EXPECT_EQ(2ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, main_web_frame_ptr));
  EXPECT_TRUE(ContainsWebFrame(frames, child_web_frame_2_ptr));
}

// Tests that all frames are removed after a call to |RemoveAllFrames|.
TEST_F(WebFramesManagerImplTest, RemoveAllFrames) {
  GURL security_origin;
  frames_manager_.AddFrame(std::make_unique<WebFrameImpl>(
      "main_web_frame",
      /*is_main_frame=*/true, security_origin, &test_web_state_));
  frames_manager_.AddFrame(std::make_unique<WebFrameImpl>(
      "web_frame",
      /*is_main_frame=*/false, security_origin, &test_web_state_));

  ASSERT_EQ(2ul, frames_manager_.GetAllWebFrames().size());
  frames_manager_.RemoveAllWebFrames();
  EXPECT_EQ(nullptr, frames_manager_.GetMainWebFrame());
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
}

// Tests that the WebFramesManagerImpl correctly ignores attempted removal of an
// already removed WebFrame.
TEST_F(WebFramesManagerImplTest, RemoveNonexistantFrame) {
  GURL security_origin;
  const std::string main_web_frame_frame_id = "main_web_frame";
  auto main_web_frame = std::make_unique<WebFrameImpl>(
      main_web_frame_frame_id,
      /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* main_web_frame_ptr = main_web_frame.get();

  frames_manager_.AddFrame(std::move(main_web_frame));
  auto frames = frames_manager_.GetAllWebFrames();
  EXPECT_EQ(1ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, main_web_frame_ptr));

  frames_manager_.RemoveFrameWithId(main_web_frame_frame_id);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
  frames_manager_.RemoveFrameWithId(main_web_frame_frame_id);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
}

// Tests that a WebFrame is correctly returned by its frame id.
TEST_F(WebFramesManagerImplTest, GetFrameWithId) {
  GURL security_origin;

  const std::string web_frame_frame_id = "web_frame_frame_id";
  auto web_frame = std::make_unique<WebFrameImpl>(
      web_frame_frame_id,
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  WebFrameImpl* web_frame_ptr = web_frame.get();
  frames_manager_.AddFrame(std::move(web_frame));

  EXPECT_EQ(web_frame_ptr, frames_manager_.GetFrameWithId(web_frame_frame_id));
  EXPECT_EQ(nullptr, frames_manager_.GetFrameWithId("invalid_id"));
}

// Tests that WebFramesManagerImpl will unregister callbacks for previous
// WKWebView and register callbacks for new WKWebView. Also tests that
// WebFramesManagerImpl creates new WebFrame when receiving
// "FrameBecameAvailable" message and removes WebFrame when receiving
// "FrameBecameUnavailable" message.
TEST_F(WebFramesManagerImplTest, OnWebViewUpdated) {
  // Mock WKUserContentController.
  WKUserContentController* user_content_controller =
      OCMClassMock([WKUserContentController class]);

  // Set up CRWWKScriptMessageRouter.
  CRWWKScriptMessageRouter* router = [[CRWWKScriptMessageRouter alloc]
      initWithUserContentController:user_content_controller];

  // Mock WKWebView.
  WKWebView* web_view_1 = OCMClassMock([WKWebView class]);
  WKWebView* web_view_2 = OCMClassMock([WKWebView class]);

  // Mock WKSecurityOrigin.
  WKSecurityOrigin* security_origin = OCMClassMock([WKSecurityOrigin class]);
  OCMStub([security_origin host]).andReturn(@"www.crw.com");
  OCMStub([security_origin port]).andReturn(443);
  OCMStub([security_origin protocol]).andReturn(@"https");

  // Mock WKFrameInfo.
  WKFrameInfo* frame_info = OCMClassMock([WKFrameInfo class]);
  OCMStub([frame_info isMainFrame]).andReturn(YES);
  OCMStub([frame_info securityOrigin]).andReturn(security_origin);

  // Mock WKScriptMessage for "FrameBecameAvailable" message.
  NSDictionary* body = @{
    @"crwFrameId" : [NSString stringWithUTF8String:kFrameId],
    @"crwFrameKey" : [NSString stringWithUTF8String:kFrameKey],
    @"crwFrameLastReceivedMessageId" : @-1,
  };
  WKScriptMessage* available_message = OCMClassMock([WKScriptMessage class]);
  OCMStub([available_message body]).andReturn(body);
  OCMStub([available_message frameInfo]).andReturn(frame_info);
  OCMStub([available_message name]).andReturn(kFrameBecameAvailableMessageName);
  OCMStub([available_message webView]).andReturn(web_view_1);

  // Mock WKScriptMessage for "FrameBecameUnavailable" message.
  WKScriptMessage* unavailable_message = OCMClassMock([WKScriptMessage class]);
  OCMStub([unavailable_message body])
      .andReturn([NSString stringWithUTF8String:kFrameId]);
  OCMStub([unavailable_message frameInfo]).andReturn(frame_info);
  OCMStub([unavailable_message name])
      .andReturn(kFrameBecameUnavailableMessageName);
  OCMStub([unavailable_message webView]).andReturn(web_view_1);

  // Test begin!

  // Tell the manager to change from nil to |web_view_1|.
  frames_manager_.OnWebViewUpdated(nil, web_view_1, router);

  // Send the "FrameBecameAvailable" to |router|.
  [(id<WKScriptMessageHandler>)router
        userContentController:user_content_controller
      didReceiveScriptMessage:available_message];

  // Check that the WebFrame for main frame is created.
  ASSERT_EQ(1UL, frames_manager_.GetAllWebFrames().size());
  WebFrame* main_frame = frames_manager_.GetMainWebFrame();
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(kFrameId, main_frame->GetFrameId());
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(GURL("https://www.crw.com"), main_frame->GetSecurityOrigin());

  // Send the "FrameBecameUnavailable" to |router|.
  [(id<WKScriptMessageHandler>)router
        userContentController:user_content_controller
      didReceiveScriptMessage:unavailable_message];

  // Check that the WebFrame for main frame is removed.
  ASSERT_EQ(0UL, frames_manager_.GetAllWebFrames().size());
  ASSERT_FALSE(frames_manager_.GetMainWebFrame());

  // Tell the manager to change from |web_view_1| to |web_view_2|.
  frames_manager_.OnWebViewUpdated(web_view_1, web_view_2, router);

  // Send the "FrameBecameAvailable" of |web_view_1| to |router| again.
  [(id<WKScriptMessageHandler>)router
        userContentController:user_content_controller
      didReceiveScriptMessage:available_message];

  // Check that WebFramesManagerImpl doesn't reply JS messages from previous
  // WKWebView.
  ASSERT_EQ(0UL, frames_manager_.GetAllWebFrames().size());
  ASSERT_FALSE(frames_manager_.GetMainWebFrame());
}

}  // namespace web
