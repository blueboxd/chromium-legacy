// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_embedder_policy_reporter.h"

#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using network::CrossOriginEmbedderPolicy;

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
    reports_.push_back(Report(type, group, url, std::move(body)));
  }

  const std::vector<Report>& reports() const { return reports_; }

 private:
  std::vector<Report> reports_;
};

class CrossOriginEmbedderPolicyReporterTest : public testing::Test {
 public:
  using Report = TestNetworkContext::Report;
  CrossOriginEmbedderPolicyReporterTest() {
    storage_partition_.set_network_context(&network_context_);
  }

  StoragePartition* storage_partition() { return &storage_partition_; }
  const TestNetworkContext& network_context() const { return network_context_; }

  base::Value CreateBodyForCorp(base::StringPiece s) {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("type", base::Value("corp"));
    dict.SetKey("blocked-url", base::Value(s));
    return dict;
  }

  base::Value CreateBodyForNavigation(base::StringPiece s) {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("type", base::Value("navigation"));
    dict.SetKey("blocked-url", base::Value(s));
    return dict;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestNetworkContext network_context_;
  TestStoragePartition storage_partition_;
};

TEST_F(CrossOriginEmbedderPolicyReporterTest, NullEndpointsForCorp) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             base::nullopt, base::nullopt);

  reporter.QueueCorpViolationReport(GURL("https://www1.example.com/y"),
                                    /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("https://www2.example.com/x"),
                                    /*report_only=*/true);

  EXPECT_TRUE(network_context().reports().empty());
}

TEST_F(CrossOriginEmbedderPolicyReporterTest, BasicCorp) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             "e1", "e2");

  reporter.QueueCorpViolationReport(
      GURL("https://www1.example.com/x#foo?bar=baz"),
      /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("http://www2.example.com:41/y"),
                                    /*report_only=*/true);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coep");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body,
            CreateBodyForCorp("https://www1.example.com/x#foo?bar=baz"));
  EXPECT_EQ(r2.type, "coep");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body, CreateBodyForCorp("http://www2.example.com:41/y"));
}

TEST_F(CrossOriginEmbedderPolicyReporterTest, UserAndPassForCorp) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             "e1", "e2");

  reporter.QueueCorpViolationReport(GURL("https://u:p@www1.example.com/x"),
                                    /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("https://u:p@www2.example.com/y"),
                                    /*report_only=*/true);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coep");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body, CreateBodyForCorp("https://www1.example.com/x"));
  EXPECT_EQ(r2.type, "coep");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body, CreateBodyForCorp("https://www2.example.com/y"));
}

TEST_F(CrossOriginEmbedderPolicyReporterTest, Clone) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             "e1", "e2");

  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter> remote;
  reporter.Clone(remote.BindNewPipeAndPassReceiver());

  remote->QueueCorpViolationReport(GURL("https://www1.example.com/x"),
                                   /*report_only=*/false);
  remote->QueueCorpViolationReport(GURL("https://www2.example.com/y"),
                                   /*report_only=*/true);

  remote.FlushForTesting();

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coep");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body, CreateBodyForCorp("https://www1.example.com/x"));
  EXPECT_EQ(r2.type, "coep");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body, CreateBodyForCorp("https://www2.example.com/y"));
}

TEST_F(CrossOriginEmbedderPolicyReporterTest, NullEndpointsForNavigation) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             base::nullopt, base::nullopt);

  reporter.QueueNavigationReport(GURL("https://www1.example.com/y"),
                                 /*report_only=*/false);
  reporter.QueueNavigationReport(GURL("https://www2.example.com/x"),
                                 /*report_only=*/true);

  EXPECT_TRUE(network_context().reports().empty());
}

TEST_F(CrossOriginEmbedderPolicyReporterTest, BasicNavigation) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             "e1", "e2");
  CrossOriginEmbedderPolicy child_coep;
  child_coep.report_only_value =
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;

  reporter.QueueNavigationReport(GURL("https://www1.example.com/x#foo?bar=baz"),
                                 /*report_only=*/false);
  reporter.QueueNavigationReport(GURL("http://www2.example.com:41/y"),
                                 /*report_only=*/true);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coep");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body,
            CreateBodyForNavigation("https://www1.example.com/x#foo?bar=baz"));
  EXPECT_EQ(r2.type, "coep");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body, CreateBodyForNavigation("http://www2.example.com:41/y"));
}

TEST_F(CrossOriginEmbedderPolicyReporterTest, UserAndPassForNavigation) {
  const GURL kContextUrl("https://example.com/path");
  CrossOriginEmbedderPolicyReporter reporter(storage_partition(), kContextUrl,
                                             "e1", "e2");
  reporter.QueueNavigationReport(GURL("https://u:p@www1.example.com/x"),
                                 /*report_only=*/false);
  reporter.QueueNavigationReport(GURL("https://u:p@www2.example.com/y"),
                                 /*report_only=*/true);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coep");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body, CreateBodyForNavigation("https://www1.example.com/x"));
  EXPECT_EQ(r2.type, "coep");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body, CreateBodyForNavigation("https://www2.example.com/y"));
}

}  // namespace
}  // namespace content
