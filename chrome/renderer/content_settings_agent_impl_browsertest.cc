// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/content_settings_agent_impl.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_view.h"

using testing::_;
using testing::DeleteArg;

namespace {

constexpr char kScriptHtml[] = R"HTML(
  <html>
  <head>
    <script src='data:foo'></script>
  </head>
  <body></body>
  </html>;
)HTML";

constexpr char kScriptWithSrcHtml[] = R"HTML(
  <html>
  <head>
    <script src='http://www.example.com/script.js'></script>
  </head>
  <body></body>
  </html>
)HTML";

class MockContentSettingsManagerImpl
    : public chrome::mojom::ContentSettingsManager {
 public:
  MockContentSettingsManagerImpl() = default;
  ~MockContentSettingsManagerImpl() override = default;

  // chrome::mojom::ContentSettingsManager methods:
  void Clone(mojo::PendingReceiver<chrome::mojom::ContentSettingsManager>
                 receiver) override {
    ADD_FAILURE() << "Not reached";
  }
  void AllowStorageAccess(StorageType storage_type,
                          const url::Origin& origin,
                          const GURL& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override {
    ++allow_storage_access_count_;
    std::move(callback).Run(true);
  }

  int allow_storage_access_count() const { return allow_storage_access_count_; }

 private:
  int allow_storage_access_count_ = 0;
};

class MockContentSettingsAgentImpl : public ContentSettingsAgentImpl {
 public:
  MockContentSettingsAgentImpl(content::RenderFrame* render_frame,
                               service_manager::BinderRegistry* registry);
  ~MockContentSettingsAgentImpl() override {}

  bool Send(IPC::Message* message) override;

  MOCK_METHOD2(OnContentBlocked,
               void(ContentSettingsType, const base::string16&));

  const GURL& image_url() const { return image_url_; }
  const std::string& image_origin() const { return image_origin_; }

  MockContentSettingsManagerImpl* mock_manager() const {
    return static_cast<MockContentSettingsManagerImpl*>(
        mock_manager_.get()->impl());
  }

 private:
  mojo::SelfOwnedReceiverRef<chrome::mojom::ContentSettingsManager>
      mock_manager_;
  const GURL image_url_;
  const std::string image_origin_;

  DISALLOW_COPY_AND_ASSIGN(MockContentSettingsAgentImpl);
};

MockContentSettingsAgentImpl::MockContentSettingsAgentImpl(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : ContentSettingsAgentImpl(render_frame, false, registry),
      image_url_("http://www.foo.com/image.jpg"),
      image_origin_("http://www.foo.com") {
  // This raw pointer is kept alive by |mock_manager_remote|.
  mojo::Remote<chrome::mojom::ContentSettingsManager> mock_manager_remote;
  mock_manager_ = mojo::MakeSelfOwnedReceiver(
      std::make_unique<MockContentSettingsManagerImpl>(),
      mock_manager_remote.BindNewPipeAndPassReceiver());
  SetContentSettingsManagerForTesting(std::move(mock_manager_remote));
}

bool MockContentSettingsAgentImpl::Send(IPC::Message* message) {
  IPC_BEGIN_MESSAGE_MAP(MockContentSettingsAgentImpl, *message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_UNHANDLED(ADD_FAILURE())
  IPC_END_MESSAGE_MAP()

  // Our super class deletes the message.
  return RenderFrameObserver::Send(message);
}

// Evaluates a boolean |predicate| every time a provisional load is committed in
// the given |frame| while the instance of this class is in scope, and verifies
// that the result matches the |expectation|.
class CommitTimeConditionChecker : public content::RenderFrameObserver {
 public:
  using Predicate = base::RepeatingCallback<bool()>;

  CommitTimeConditionChecker(content::RenderFrame* frame,
                             const Predicate& predicate,
                             bool expectation)
      : content::RenderFrameObserver(frame),
        predicate_(predicate),
        expectation_(expectation) {}

 protected:
  // RenderFrameObserver:
  void OnDestruct() override {}
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    EXPECT_EQ(expectation_, predicate_.Run());
  }

 private:
  const Predicate predicate_;
  const bool expectation_;

  DISALLOW_COPY_AND_ASSIGN(CommitTimeConditionChecker);
};

}  // namespace

class ContentSettingsAgentImplBrowserTest : public ChromeRenderViewTest {
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Set up a fake url loader factory to ensure that script loader can create
    // a WebURLLoader.
    CreateFakeWebURLLoaderFactory();

    // Unbind the ContentSettingsAgent interface that would be registered by
    // the ContentSettingsAgentImpl created when the render frame is created.
    view_->GetMainRenderFrame()
        ->GetAssociatedInterfaceRegistry()
        ->RemoveInterface(chrome::mojom::ContentSettingsAgent::Name_);
  }
};

TEST_F(ContentSettingsAgentImplBrowserTest, DidBlockContentType) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  EXPECT_CALL(mock_agent, OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES,
                                           base::string16()));
  mock_agent.DidBlockContentType(CONTENT_SETTINGS_TYPE_COOKIES);

  // Blocking the same content type a second time shouldn't send a notification.
  mock_agent.DidBlockContentType(CONTENT_SETTINGS_TYPE_COOKIES);
  ::testing::Mock::VerifyAndClearExpectations(&mock_agent);
}

// Tests that multiple invokations of AllowDOMStorage result in a single IPC.
TEST_F(ContentSettingsAgentImplBrowserTest, AllowDOMStorage) {
  // Load some HTML, so we have a valid security origin.
  LoadHTMLWithUrlOverride("<html></html>", "https://example.com/");
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  mock_agent.AllowStorage(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.mock_manager()->allow_storage_access_count());

  // Accessing localStorage from the same origin again shouldn't result in a
  // new IPC.
  mock_agent.AllowStorage(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.mock_manager()->allow_storage_access_count());
}

// Regression test for http://crbug.com/35011
TEST_F(ContentSettingsAgentImplBrowserTest, JSBlockSentAfterPageLoad) {
  // 1. Load page with JS.
  const char kHtml[] =
      "<html>"
      "<head>"
      "<script>document.createElement('div');</script>"
      "</head>"
      "<body>"
      "</body>"
      "</html>";
  render_thread_->sink().ClearMessages();
  LoadHTML(kHtml);

  // 2. Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));
  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Make sure no pending messages are in the queue.
  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const auto HasSentChromeViewHostMsgContentBlocked =
      [](content::MockRenderThread* render_thread) {
        return !!render_thread->sink().GetFirstMessageMatching(
            ChromeViewHostMsg_ContentBlocked::ID);
      };

  // 3. Reload page. Verify that the notification that javascript was blocked
  // has not yet been sent at the time when the navigation commits.
  CommitTimeConditionChecker checker(
      view_->GetMainRenderFrame(),
      base::Bind(HasSentChromeViewHostMsgContentBlocked,
                 base::Unretained(render_thread_.get())),
      false);

  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(kHtml);
  GURL url(url_str);
  Reload(url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasSentChromeViewHostMsgContentBlocked(render_thread_.get()));
}

TEST_F(ContentSettingsAgentImplBrowserTest, PluginsTemporarilyAllowed) {
  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  std::string foo_plugin = "foo";
  std::string bar_plugin = "bar";

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(foo_plugin));

  // Temporarily allow the "foo" plugin.
  agent->OnLoadBlockedPlugins(foo_plugin);
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(bar_plugin));

  // Simulate same document navigation.
  OnSameDocumentNavigation(GetMainFrame(), true);
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(bar_plugin));

  // Navigate to a different page.
  LoadHTML("<html>Bar</html>");
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(bar_plugin));

  // Temporarily allow all plugins.
  agent->OnLoadBlockedPlugins(std::string());
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(bar_plugin));
}

TEST_F(ContentSettingsAgentImplBrowserTest, ImagesBlockedByDefault) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default image blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);
  EXPECT_CALL(mock_agent,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES, base::string16()));
  EXPECT_FALSE(agent->AllowImage(true, mock_agent.image_url()));
  ::testing::Mock::VerifyAndClearExpectations(&agent);

  // Create an exception which allows the image.
  image_setting_rules.insert(
      image_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString(mock_agent.image_origin()),
          base::Value::FromUniquePtrValue(
              content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
          std::string(), false));

  EXPECT_CALL(mock_agent,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES, base::string16()))
      .Times(0);
  EXPECT_TRUE(agent->AllowImage(true, mock_agent.image_url()));
  ::testing::Mock::VerifyAndClearExpectations(&agent);
}

TEST_F(ContentSettingsAgentImplBrowserTest, ImagesAllowedByDefault) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default image blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);
  EXPECT_CALL(mock_agent,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES, base::string16()))
      .Times(0);
  EXPECT_TRUE(agent->AllowImage(true, mock_agent.image_url()));
  ::testing::Mock::VerifyAndClearExpectations(&agent);

  // Create an exception which blocks the image.
  image_setting_rules.insert(
      image_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString(mock_agent.image_origin()),
          base::Value::FromUniquePtrValue(
              content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
          std::string(), false));
  EXPECT_CALL(mock_agent,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES, base::string16()));
  EXPECT_FALSE(agent->AllowImage(true, mock_agent.image_url()));
  ::testing::Mock::VerifyAndClearExpectations(&agent);
}

TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsBlockScripts) {
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was blocked.
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      ChromeViewHostMsg_ContentBlocked::ID));
}

TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsAllowScripts) {
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was not blocked.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ChromeViewHostMsg_ContentBlocked::ID));
}

TEST_F(ContentSettingsAgentImplBrowserTest,
       ContentSettingsAllowScriptsWithSrc) {
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  LoadHTML(kScriptWithSrcHtml);

  // Verify that the script was not blocked.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ChromeViewHostMsg_ContentBlocked::ID));
}

// Regression test for crbug.com/232410: Load a page with JS blocked. Then,
// allow JS and reload the page. In each case, only one of noscript or script
// tags should be enabled, but never both.
TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsNoscriptTag) {
  // 1. Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // 2. Load a page which contains a noscript tag and a script tag. Note that
  // the page doesn't have a body tag.
  const char kHtml[] =
      "<html>"
      "<noscript>JS_DISABLED</noscript>"
      "<script>document.write('JS_ENABLED');</script>"
      "</html>";
  LoadHTML(kHtml);
  EXPECT_NE(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_DISABLED"));
  EXPECT_EQ(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_ENABLED"));

  // 3. Allow JavaScript.
  script_setting_rules.clear();
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));
  agent->SetContentSettingRules(&content_setting_rules);

  // 4. Reload the page.
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(kHtml);
  GURL url(url_str);
  Reload(url);
  EXPECT_NE(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_ENABLED"));
  EXPECT_EQ(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_DISABLED"));
}

// Checks that same document navigations don't update content settings for the
// page.
TEST_F(ContentSettingsAgentImplBrowserTest,
       ContentSettingsSameDocumentNavigation) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was not blocked.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ChromeViewHostMsg_ContentBlocked::ID));

  // Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // The page shouldn't see the change to script blocking setting after a
  // same document navigation.
  OnSameDocumentNavigation(GetMainFrame(), true);
  EXPECT_TRUE(agent->AllowScript(true));
}

TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsInterstitialPages) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Block scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));
  // Block images.
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);
  agent->SetAsInterstitial();

  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was allowed.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ChromeViewHostMsg_ContentBlocked::ID));

  // Verify that images are allowed.
  EXPECT_CALL(mock_agent,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES, base::string16()))
      .Times(0);
  EXPECT_TRUE(agent->AllowImage(true, mock_agent.image_url()));
  ::testing::Mock::VerifyAndClearExpectations(&agent);
}

TEST_F(ContentSettingsAgentImplBrowserTest, AutoplayContentSettings) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& autoplay_setting_rules =
      content_setting_rules.autoplay_rules;
  autoplay_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  EXPECT_TRUE(agent->AllowAutoplay(false));
  ::testing::Mock::VerifyAndClearExpectations(&agent);

  // Add rule to block autoplay.
  autoplay_setting_rules.insert(
      autoplay_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(),
          base::Value::FromUniquePtrValue(
              content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
          std::string(), false));

  EXPECT_FALSE(agent->AllowAutoplay(true));
  ::testing::Mock::VerifyAndClearExpectations(&agent);
}
