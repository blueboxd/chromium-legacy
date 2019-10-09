// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/tab_strip_resources.h"
#include "chrome/grit/tab_strip_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"

namespace {

// Writes bytes to a std::vector that can be fetched. This is used to record the
// output of skia image encoding.
//
// TODO(crbug.com/991393): remove this when we no longer transcode images here.
class BufferWStream : public SkWStream {
 public:
  BufferWStream() = default;
  ~BufferWStream() override = default;

  // Returns the output buffer by moving.
  std::vector<unsigned char> GetBuffer() { return std::move(result_); }

  // SkWStream:
  bool write(const void* buffer, size_t size) override {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(buffer);
    result_.insert(result_.end(), bytes, bytes + size);
    return true;
  }

  size_t bytesWritten() const override { return result_.size(); }

 private:
  std::vector<unsigned char> result_;
};

class TabStripUIHandler : public content::WebUIMessageHandler,
                          public TabStripModelObserver {
 public:
  explicit TabStripUIHandler(Browser* browser)
      : browser_(browser),
        thumbnail_tracker_(base::Bind(&TabStripUIHandler::HandleThumbnailUpdate,
                                      base::Unretained(this))) {}
  ~TabStripUIHandler() override = default;

  void OnJavascriptAllowed() override {
    browser_->tab_strip_model()->AddObserver(this);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (tab_strip_model->empty())
      return;

    switch (change.type()) {
      case TabStripModelChange::kInserted: {
        for (const auto& contents : change.GetInsert()->contents) {
          FireWebUIListener("tab-created",
                            GetTabData(contents.contents, contents.index));
        }
        break;
      }
      case TabStripModelChange::kRemoved: {
        for (const auto& contents : change.GetRemove()->contents) {
          FireWebUIListener("tab-removed",
                            base::Value(extensions::ExtensionTabUtil::GetTabId(
                                contents.contents)));
        }
        break;
      }
      case TabStripModelChange::kMoved: {
        auto* move = change.GetMove();
        FireWebUIListener(
            "tab-moved",
            base::Value(extensions::ExtensionTabUtil::GetTabId(move->contents)),
            base::Value(move->to_index));
        break;
      }

      case TabStripModelChange::kReplaced:
      case TabStripModelChange::kGroupChanged:
      case TabStripModelChange::kSelectionOnly:
        // Not yet implemented.
        break;
    }

    if (selection.active_tab_changed()) {
      content::WebContents* new_contents = selection.new_contents;
      int index = selection.new_model.active();
      if (new_contents && index != TabStripModel::kNoTab) {
        FireWebUIListener(
            "tab-active-changed",
            base::Value(extensions::ExtensionTabUtil::GetTabId(new_contents)));
      }
    }
  }

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    FireWebUIListener("tab-updated", GetTabData(contents, index));
  }

  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override {
    FireWebUIListener("tab-updated", GetTabData(contents, index));
  }

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getTabs",
        base::Bind(&TabStripUIHandler::HandleGetTabs, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getThemeColors", base::Bind(&TabStripUIHandler::HandleGetThemeColors,
                                     base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "addTrackedTab",
        base::Bind(&TabStripUIHandler::AddTrackedTab, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "removeTrackedTab", base::Bind(&TabStripUIHandler::RemoveTrackedTab,
                                       base::Unretained(this)));
  }

 private:
  base::DictionaryValue GetTabData(content::WebContents* contents, int index) {
    base::DictionaryValue tab_data;

    tab_data.SetBoolean("active",
                        browser_->tab_strip_model()->active_index() == index);
    tab_data.SetInteger("id", extensions::ExtensionTabUtil::GetTabId(contents));
    tab_data.SetInteger("index", index);
    tab_data.SetString("status", extensions::ExtensionTabUtil::GetTabStatusText(
                                     contents->IsLoading()));

    // TODO(johntlee): Replace with favicon from TabRendererData
    content::NavigationEntry* visible_entry =
        contents->GetController().GetVisibleEntry();
    if (visible_entry && visible_entry->GetFavicon().valid) {
      tab_data.SetString("favIconUrl", visible_entry->GetFavicon().url.spec());
    }

    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(browser_->tab_strip_model(), index);
    tab_data.SetBoolean("pinned", tab_renderer_data.pinned);
    tab_data.SetString("title", tab_renderer_data.title);
    tab_data.SetString("url", tab_renderer_data.visible_url.GetContent());
    // TODO(johntlee): Add the rest of TabRendererData

    return tab_data;
  }

  void HandleGetTabs(const base::ListValue* args) {
    AllowJavascript();
    const base::Value& callback_id = args->GetList()[0];

    base::ListValue tabs;
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      tabs.Append(GetTabData(tab_strip_model->GetWebContentsAt(i), i));
    }
    ResolveJavascriptCallback(callback_id, tabs);
  }

  void HandleGetThemeColors(const base::ListValue* args) {
    AllowJavascript();
    const base::Value& callback_id = args->GetList()[0];

    const ui::ThemeProvider& tp =
        ThemeService::GetThemeProviderForProfile(browser_->profile());

    // This should return an object of CSS variables to rgba values so that
    // the WebUI can use the CSS variables to color the tab strip
    base::DictionaryValue colors;
    colors.SetString("--tabstrip-background-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_FRAME)));
    colors.SetString("--tabstrip-tab-background-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_TOOLBAR)));
    colors.SetString("--tabstrip-tab-text-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_TAB_TEXT)));
    colors.SetString("--tabstrip-tab-separator-color",
                     color_utils::SkColorToRgbaString(SkColorSetA(
                         tp.GetColor(ThemeProperties::COLOR_TAB_TEXT),
                         /* 16% opacity */ 0.16 * 255)));

    colors.SetString("--tabstrip-tab-loading-spinning-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_THROBBER_SPINNING)));
    colors.SetString("--tabstrip-indicator-recording-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_ALERT_RECORDING)));
    colors.SetString("--tabstrip-indicator-pip-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_TAB_PIP_PLAYING)));
    colors.SetString("--tabstrip-indicator-capturing-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_ALERT_CAPTURING)));

    ResolveJavascriptCallback(callback_id, colors);
  }

  void AddTrackedTab(const base::ListValue* args) {
    AllowJavascript();

    int tab_id = 0;
    if (!args->GetInteger(0, &tab_id))
      return;

    content::WebContents* tab = nullptr;
    if (!extensions::ExtensionTabUtil::GetTabById(tab_id, browser_->profile(),
                                                  true, &tab)) {
      // ID didn't refer to a valid tab.
      DVLOG(1) << "Invalid tab ID";
      return;
    }
    thumbnail_tracker_.WatchTab(tab);
  }

  void RemoveTrackedTab(const base::ListValue* args) {
    AllowJavascript();

    int tab_id = 0;
    if (!args->GetInteger(0, &tab_id))
      return;
    // TODO(crbug.com/991393): un-watch tabs when we are done.
  }

  // Callback passed to |thumbnail_tracker_|. Called when a tab's thumbnail
  // changes, or when we start watching the tab.
  void HandleThumbnailUpdate(content::WebContents* tab, gfx::ImageSkia image) {
    const SkBitmap& bitmap =
        image.GetRepresentation(web_ui()->GetDeviceScaleFactor()).GetBitmap();
    BufferWStream stream;
    const bool encoding_succeeded =
        SkEncodeImage(&stream, bitmap, SkEncodedImageFormat::kJPEG, 100);
    DCHECK(encoding_succeeded);
    const std::vector<unsigned char> image_data = stream.GetBuffer();

    // Send base-64 encoded image to JS side.
    //
    // TODO(crbug.com/991393): streamline the process from tab capture to
    // sending the image. Currently, a frame is captured, sent to
    // ThumbnailImage, encoded to JPEG (w/ a copy), decoded to a raw bitmap
    // (another copy), and sent to observers. Here, we then re-encode the image
    // to a JPEG (another copy), encode to base64 (another copy), append the
    // base64 header (another copy), and send it (another copy). This is 6
    // copies of essentially the same image, and it is de-encoded and re-encoded
    // to the same format. We can reduce the number of copies and avoid the
    // redundant encoding.
    std::string encoded_image =
        base::Base64Encode(base::as_bytes(base::make_span(image_data)));
    encoded_image = "data:image/jpeg;base64," + encoded_image;

    const int tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
    FireWebUIListener("tab-thumbnail-updated", base::Value(tab_id),
                      base::Value(encoded_image));
  }

  Browser* const browser_;
  ThumbnailTracker thumbnail_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabStripUIHandler);
};

}  // namespace

TabStripUI::TabStripUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUITabStripHost);

  std::string generated_path =
      "@out_folder@/gen/chrome/browser/resources/tab_strip/";

  for (size_t i = 0; i < kTabStripResourcesSize; ++i) {
    std::string path = kTabStripResources[i].name;
    if (path.rfind(generated_path, 0) == 0) {
      path = path.substr(generated_path.length());
    }
    html_source->AddResourcePath(path, kTabStripResources[i].value);
  }

  html_source->SetDefaultResource(IDR_TAB_STRIP_HTML);

  // Add a load time string for the frame color to allow the tab strip to paint
  // a background color that matches the frame before any content loads
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile);
  html_source->AddString("frameColor",
                         color_utils::SkColorToRgbaString(
                             tp.GetColor(ThemeProperties::COLOR_FRAME)));

  content::WebUIDataSource::Add(profile, html_source);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());
}

TabStripUI::~TabStripUI() {}

void TabStripUI::Initialize(Browser* browser) {
  web_ui()->AddMessageHandler(std::make_unique<TabStripUIHandler>(browser));
}
