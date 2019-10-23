// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/data_decoder_service.h"

#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace content {

using DataDecoderBrowserTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, Launch) {
  // Simple smoke test to verify that we can launch an instance of the Data
  // Decoder service and communicate end-to-end.

  mojo::Remote<data_decoder::mojom::DataDecoderService> service =
      LaunchDataDecoder();
  mojo::Remote<data_decoder::mojom::JsonParser> parser;
  service->BindJsonParser(parser.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  parser->Parse(
      R"({"a": 5, "b": 42})",
      base::BindLambdaForTesting([&](base::Optional<base::Value> result,
                                     const base::Optional<std::string>& error) {
        EXPECT_FALSE(error);
        ASSERT_TRUE(result);
        EXPECT_EQ(base::Value::Type::DICTIONARY, result->type());
        auto* a = result->FindKey("a");
        EXPECT_TRUE(a);
        EXPECT_EQ(5, a->GetInt());
        auto* b = result->FindKey("b");
        EXPECT_TRUE(b);
        EXPECT_EQ(42, b->GetInt());
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace content
