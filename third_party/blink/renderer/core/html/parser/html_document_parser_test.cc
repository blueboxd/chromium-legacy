// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/loader/prerenderer_client.h"
#include "third_party/blink/renderer/core/loader/text_resource_decoder_builder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

class MockPrerendererClient : public PrerendererClient {
 public:
  MockPrerendererClient(Page& page, bool is_prefetch_only)
      : PrerendererClient(page, nullptr), is_prefetch_only_(is_prefetch_only) {}

 private:
  bool IsPrefetchOnly() override { return is_prefetch_only_; }

  bool is_prefetch_only_;
};

class HTMLDocumentParserTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetURL(KURL("https://example.test"));
  }

  HTMLDocumentParser* CreateParser(HTMLDocument& document) {
    auto* parser = MakeGarbageCollected<HTMLDocumentParser>(
        document, kForceSynchronousParsing);
    std::unique_ptr<TextResourceDecoder> decoder(
        BuildTextResourceDecoderFor(&document, "text/html", g_null_atom));
    parser->SetDecoder(std::move(decoder));
    return parser;
  }
};

}  // namespace

TEST_F(HTMLDocumentParserTest, AppendPrefetch) {
  auto& document = To<HTMLDocument>(GetDocument());
  ProvidePrerendererClientTo(
      *document.GetPage(),
      MakeGarbageCollected<MockPrerendererClient>(*document.GetPage(), true));
  EXPECT_TRUE(document.IsPrefetchOnly());
  HTMLDocumentParser* parser = CreateParser(document);

  const char kBytes[] = "<ht";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  // The bytes are forwarded to the preload scanner, not to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_TRUE(script_runner_host->HasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::kDataState, parser->Tokenizer()->GetState());
  // Finishing should not cause parsing to start (verified via an internal
  // DCHECK).
  static_cast<DocumentParser*>(parser)->Finish();
  EXPECT_EQ(HTMLTokenizer::kDataState, parser->Tokenizer()->GetState());
}

TEST_F(HTMLDocumentParserTest, AppendNoPrefetch) {
  auto& document = To<HTMLDocument>(GetDocument());
  EXPECT_FALSE(document.IsPrefetchOnly());
  // Use ForceSynchronousParsing to allow calling append().
  HTMLDocumentParser* parser = CreateParser(document);

  const char kBytes[] = "<ht";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  // The bytes are forwarded to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_FALSE(script_runner_host->HasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::kTagNameState, parser->Tokenizer()->GetState());
}

}  // namespace blink
