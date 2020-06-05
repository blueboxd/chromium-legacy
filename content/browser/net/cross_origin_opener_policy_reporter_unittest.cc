// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_opener_policy_reporter.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestNetworkContext : public network::TestNetworkContext {
 public:
  struct Report {
    Report(const std::string& type,
           const std::string& group,
           const GURL& url,
           base::Value body)
        : type(type), group(group), url(url), body(std::move(body)) {}

    std::string type;
    std::string group;
    GURL url;
    base::Value body;
  };
  void QueueReport(const std::string& type,
                   const std::string& group,
                   const GURL& url,
                   const base::Optional<std::string>& user_agent,
                   base::Value body) override {
    DCHECK(!user_agent);
    reports_.emplace_back(Report(type, group, url, std::move(body)));
  }

  const std::vector<Report>& reports() const { return reports_; }

 private:
  std::vector<Report> reports_;
};

}  // namespace

class CrossOriginOpenerPolicyReporterTest : public testing::Test {
 public:
  using Report = TestNetworkContext::Report;
  CrossOriginOpenerPolicyReporterTest() {
    storage_partition_.set_network_context(&network_context_);
    coop_.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
    coep_.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    coop_.reporting_endpoint = "e1";
    context_url_ = GURL("https://www1.example.com/x");
  }

  StoragePartition* storage_partition() { return &storage_partition_; }
  const TestNetworkContext& network_context() const { return network_context_; }
  const GURL& context_url() const { return context_url_; }
  const network::CrossOriginOpenerPolicy& coop() const { return coop_; }
  const network::CrossOriginEmbedderPolicy& coep() const { return coep_; }

 protected:
  std::unique_ptr<CrossOriginOpenerPolicyReporter> GetReporter() {
    return std::unique_ptr<CrossOriginOpenerPolicyReporter>(
        new CrossOriginOpenerPolicyReporter(storage_partition(), GURL(),
                                            GlobalFrameRoutingId(123, 456),
                                            context_url(), coop(), coep()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestNetworkContext network_context_;
  TestStoragePartition storage_partition_;
  GURL context_url_;
  network::CrossOriginOpenerPolicy coop_;
  network::CrossOriginEmbedderPolicy coep_;
};

TEST_F(CrossOriginOpenerPolicyReporterTest, Basic) {
  auto reporter = GetReporter();

  reporter->QueueOpenerBreakageReport(
      GURL("https://www1.example.com/y#foo?bar=baz"), false, false);
  reporter->QueueOpenerBreakageReport(GURL("http://www2.example.com:41/z"),
                                      true, false);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coop");
  EXPECT_EQ(r1.body.FindKey("disposition")->GetString(), "enforce");
  EXPECT_EQ(r1.body.FindKey("document-uri")->GetString(), context_url());
  EXPECT_EQ(r1.body.FindKey("navigation-uri")->GetString(),
            "https://www1.example.com/y#foo?bar=baz");
  EXPECT_EQ(r1.body.FindKey("violation-type")->GetString(),
            "navigation-to-document");
  EXPECT_EQ(r1.body.FindKey("effective-policy")->GetString(),
            "same-origin-plus-coep");

  EXPECT_EQ(r2.type, "coop");
  EXPECT_EQ(r2.body.FindKey("disposition")->GetString(), "enforce");
  EXPECT_EQ(r2.body.FindKey("document-uri")->GetString(), context_url());
  EXPECT_EQ(r2.body.FindKey("navigation-uri")->GetString(),
            "http://www2.example.com:41/z");
  EXPECT_EQ(r2.body.FindKey("violation-type")->GetString(),
            "navigation-from-document");
  EXPECT_EQ(r2.body.FindKey("effective-policy")->GetString(),
            "same-origin-plus-coep");
}

TEST_F(CrossOriginOpenerPolicyReporterTest, UserAndPassSanitization) {
  auto reporter = GetReporter();

  reporter->QueueOpenerBreakageReport(GURL("https://u:p@www2.example.com/x"),
                                      false, false);

  ASSERT_EQ(1u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];

  EXPECT_EQ(r1.type, "coop");
  EXPECT_EQ(r1.body.FindKey("document-uri")->GetString(),
            "https://www1.example.com/x");
  EXPECT_EQ(r1.body.FindKey("navigation-uri")->GetString(),
            "https://www2.example.com/x");
}

TEST_F(CrossOriginOpenerPolicyReporterTest, Clone) {
  auto reporter = GetReporter();

  mojo::Remote<network::mojom::CrossOriginOpenerPolicyReporter> remote;
  reporter->Clone(remote.BindNewPipeAndPassReceiver());

  remote->QueueOpenerBreakageReport(GURL("https://www1.example.com/y"), false,
                                    false);

  remote.FlushForTesting();

  ASSERT_EQ(1u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];

  EXPECT_EQ(r1.type, "coop");
  EXPECT_EQ(r1.body.FindKey("disposition")->GetString(), "enforce");
  EXPECT_EQ(r1.body.FindKey("document-uri")->GetString(), context_url());
  EXPECT_EQ(r1.body.FindKey("navigation-uri")->GetString(),
            "https://www1.example.com/y");
  EXPECT_EQ(r1.body.FindKey("violation-type")->GetString(),
            "navigation-to-document");
  EXPECT_EQ(r1.body.FindKey("effective-policy")->GetString(),
            "same-origin-plus-coep");
}

}  // namespace content
