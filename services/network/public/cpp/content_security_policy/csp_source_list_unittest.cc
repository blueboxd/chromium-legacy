// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source_list.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace {

// Allow() is an abbreviation of CheckCSPSourceList. Useful for writing
// test expectations on one line.
bool Allow(const mojom::CSPSourceListPtr& source_list,
           const GURL& url,
           CSPContext* context,
           bool is_redirect = false,
           bool is_response_check = false) {
  return CheckCSPSourceList(source_list, url, context, is_redirect,
                            is_response_check);
}

}  // namespace

TEST(CSPSourceList, MultipleSource) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  std::vector<mojom::CSPSourcePtr> sources;
  sources.push_back(mojom::CSPSource::New("", "a.com", url::PORT_UNSPECIFIED,
                                          "", false, false));
  sources.push_back(mojom::CSPSource::New("", "b.com", url::PORT_UNSPECIFIED,
                                          "", false, false));
  auto source_list =
      mojom::CSPSourceList::New(std::move(sources), false, false, false);
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("http://c.com"), &context));
}

TEST(CSPSourceList, AllowStar) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  auto source_list = mojom::CSPSourceList::New(
      std::vector<mojom::CSPSourcePtr>(),  // source_list
      false,                               // allow_self
      true,                                // allow_star
      false);                              // allow_redirects
  EXPECT_TRUE(Allow(source_list, GURL("http://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("ws://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("wss://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("ftp://not-example.com"), &context));

  EXPECT_FALSE(Allow(source_list, GURL("file://not-example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), &context));

  // With a protocol of 'file', '*' allow 'file:'
  context.SetSelf(url::Origin::Create(GURL("file://example.com")));
  EXPECT_TRUE(Allow(source_list, GURL("file://not-example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), &context));
}

TEST(CSPSourceList, AllowSelf) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  auto source_list = mojom::CSPSourceList::New(
      std::vector<mojom::CSPSourcePtr>(),  // source_list
      true,                                // allow_self,
      false,                               // allow_star
      false);                              // allow_redirects
  EXPECT_TRUE(Allow(source_list, GURL("http://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("http://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("ws://example.com"), &context));
}

TEST(CSPSourceList, AllowStarAndSelf) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("https://a.com")));
  auto source_list =
      mojom::CSPSourceList::New(std::vector<mojom::CSPSourcePtr>(),
                                false,   // allow_self
                                false,   // allow_star
                                false);  // allow_redirects

  // If the request is allowed by {*} and not by {'self'} then it should be
  // allowed by the union {*,'self'}.
  source_list->allow_self = true;
  source_list->allow_star = false;
  EXPECT_FALSE(Allow(source_list, GURL("http://b.com"), &context));
  source_list->allow_self = false;
  source_list->allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
  source_list->allow_self = true;
  source_list->allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
}

TEST(CSPSourceList, AllowSelfWithUnspecifiedPort) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("https://example.com/")));
  auto source_list = mojom::CSPSourceList::New(
      std::vector<mojom::CSPSourcePtr>(),  // source_list
      true,                                // allow_self
      false,                               // allow_star
      false);                              // allow_redirects

  EXPECT_TRUE(
      Allow(source_list, GURL("https://example.com/print.pdf"), &context));
}

TEST(CSPSourceList, AllowNone) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  auto source_list = mojom::CSPSourceList::New(
      std::vector<mojom::CSPSourcePtr>(),  // source_list
      false,                               // allow_self
      false,                               // allow_star
      false);                              // allow_redirects
  EXPECT_FALSE(Allow(source_list, GURL("http://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("https://example.test/"), &context));
}

TEST(CSPSourceTest, SelfIsUnique) {
  // Policy: 'self'
  auto source_list = mojom::CSPSourceList::New(
      std::vector<mojom::CSPSourcePtr>(),  // source_list
      true,                                // allow_self
      false,                               // allow_star
      false);                              // allow_redirects
  CSPContext context;

  context.SetSelf(url::Origin::Create(GURL("http://a.com")));
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("data:text/html,hello"), &context));

  context.SetSelf(
      url::Origin::Create(GURL("data:text/html,<iframe src=[...]>")));
  EXPECT_FALSE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("data:text/html,hello"), &context));
}

}  // namespace network
