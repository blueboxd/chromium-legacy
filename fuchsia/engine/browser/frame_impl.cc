// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_impl.h"

#include <limits>

#include "base/bind_helpers.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/was_activated_option.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/message_port.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/base_focus_rules.h"
#include "url/gurl.h"

namespace {

// logging::LogSeverity does not define a value to disable logging so set a
// value much lower than logging::LOG_VERBOSE here.
const logging::LogSeverity kLogSeverityNone =
    std::numeric_limits<logging::LogSeverity>::min();

// Layout manager that allows only one child window and stretches it to fill the
// parent.
class LayoutManagerImpl : public aura::LayoutManager {
 public:
  LayoutManagerImpl() = default;
  ~LayoutManagerImpl() override = default;

  // aura::LayoutManager implementation.
  void OnWindowResized() override {
    // Resize the child to match the size of the parent
    if (child_) {
      SetChildBoundsDirect(child_,
                           gfx::Rect(child_->parent()->bounds().size()));
    }
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    DCHECK(!child_);
    child_ = child;
    SetChildBoundsDirect(child_, gfx::Rect(child_->parent()->bounds().size()));
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {
    DCHECK_EQ(child, child_);
    child_ = nullptr;
  }

  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  aura::Window* child_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerImpl);
};

class FrameFocusRules : public wm::BaseFocusRules {
 public:
  FrameFocusRules() = default;
  ~FrameFocusRules() override = default;

  // wm::BaseFocusRules implementation.
  bool SupportsChildActivation(const aura::Window*) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameFocusRules);
};

bool FrameFocusRules::SupportsChildActivation(const aura::Window*) const {
  // TODO(crbug.com/878439): Return a result based on window properties such as
  // visibility.
  return true;
}

bool IsOriginWhitelisted(const GURL& url,
                         const std::vector<std::string>& allowed_origins) {
  constexpr const char kWildcard[] = "*";

  for (const std::string& origin : allowed_origins) {
    if (origin == kWildcard)
      return true;

    GURL origin_url(origin);
    if (!origin_url.is_valid()) {
      DLOG(WARNING) << "Ignored invalid origin spec for whitelisting: "
                    << origin;
      continue;
    }

    if (origin_url != url.GetOrigin())
      continue;

    // TODO(crbug.com/893236): Add handling for nonstandard origins
    // (e.g. data: URIs).

    return true;
  }
  return false;
}

class ScenicWindowTreeHost : public aura::WindowTreeHostPlatform {
 public:
  explicit ScenicWindowTreeHost(ui::PlatformWindowInitProperties properties)
      : aura::WindowTreeHostPlatform(std::move(properties)) {}

  ~ScenicWindowTreeHost() override = default;

  // Route focus & blur events to the window's focus observer and its
  // InputMethod.
  void OnActivationChanged(bool active) override {
    if (active) {
      aura::client::GetFocusClient(window())->FocusWindow(window());
      GetInputMethod()->OnFocus();
    } else {
      aura::client::GetFocusClient(window())->FocusWindow(nullptr);
      GetInputMethod()->OnBlur();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScenicWindowTreeHost);
};

logging::LogSeverity ConsoleLogLevelToLoggingSeverity(
    fuchsia::web::ConsoleLogLevel level) {
  switch (level) {
    case fuchsia::web::ConsoleLogLevel::NONE:
      return kLogSeverityNone;
    case fuchsia::web::ConsoleLogLevel::DEBUG:
      return logging::LOG_VERBOSE;
    case fuchsia::web::ConsoleLogLevel::INFO:
      return logging::LOG_INFO;
    case fuchsia::web::ConsoleLogLevel::WARN:
      return logging::LOG_WARNING;
    case fuchsia::web::ConsoleLogLevel::ERROR:
      return logging::LOG_ERROR;
  }
  NOTREACHED()
      << "Unknown log level: "
      << static_cast<std::underlying_type<fuchsia::web::ConsoleLogLevel>::type>(
             level);
}

}  // namespace

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fidl::InterfaceRequest<fuchsia::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      context_(context),
      navigation_controller_(web_contents_.get()),
      log_level_(kLogSeverityNone),
      binding_(this, std::move(frame_request)) {
  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " Frame disconnected.";
    context_->DestroyFrame(this);
  });

  content::UpdateFontRendererPreferencesFromSystemSettings(
      web_contents_->GetMutableRendererPrefs());
}

FrameImpl::~FrameImpl() {
  TearDownView();
}

zx::unowned_channel FrameImpl::GetBindingChannelForTest() const {
  return zx::unowned_channel(binding_.channel());
}

FrameImpl::OriginScopedScript::OriginScopedScript() = default;

FrameImpl::OriginScopedScript::OriginScopedScript(
    std::vector<std::string> origins,
    base::ReadOnlySharedMemoryRegion script)
    : origins_(std::move(origins)), script_(std::move(script)) {}

FrameImpl::OriginScopedScript& FrameImpl::OriginScopedScript::operator=(
    FrameImpl::OriginScopedScript&& other) {
  origins_ = std::move(other.origins_);
  script_ = std::move(other.script_);
  return *this;
}

FrameImpl::OriginScopedScript::~OriginScopedScript() = default;

void FrameImpl::TearDownView() {
  if (window_tree_host_) {
    aura::client::SetFocusClient(root_window(), nullptr);
    wm::SetActivationClient(root_window(), nullptr);
    root_window()->RemovePreTargetHandler(focus_controller_.get());
    web_contents_->GetNativeView()->Hide();
    window_tree_host_->Hide();
    window_tree_host_->compositor()->SetVisible(false);
    window_tree_host_ = nullptr;

    // Allows posted focus events to process before the FocusController is torn
    // down.
    content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE,
                                       std::move(focus_controller_));
  }
}

void FrameImpl::ExecuteJavaScriptInternal(std::vector<std::string> origins,
                                          fuchsia::mem::Buffer script,
                                          ExecuteJavaScriptCallback callback,
                                          bool need_result) {
  fuchsia::web::Frame_ExecuteJavaScript_Result result;
  if (!context_->IsJavaScriptInjectionAllowed()) {
    result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
    callback(std::move(result));
    return;
  }

  if (!IsOriginWhitelisted(web_contents_->GetLastCommittedURL(), origins)) {
    result.set_err(fuchsia::web::FrameError::INVALID_ORIGIN);
    callback(std::move(result));
    return;
  }

  base::string16 script_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(script, &script_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  content::RenderFrameHost::JavaScriptResultCallback result_callback;
  if (need_result) {
    result_callback = base::BindOnce(
        [](ExecuteJavaScriptCallback callback, base::Value result_value) {
          fuchsia::web::Frame_ExecuteJavaScript_Result result;

          std::string result_json;
          if (!base::JSONWriter::Write(result_value, &result_json)) {
            result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
            callback(std::move(result));
            return;
          }

          fuchsia::web::Frame_ExecuteJavaScript_Response response;
          response.result =
              cr_fuchsia::MemBufferFromString(std::move(result_json));
          result.set_response(std::move(response));
          callback(std::move(result));
        },
        std::move(callback));
  }

  web_contents_->GetMainFrame()->ExecuteJavaScript(script_utf16,
                                                   std::move(result_callback));

  if (!need_result) {
    // If no result is required then invoke callback() immediately.
    fuchsia::web::Frame_ExecuteJavaScript_Result result;
    result.set_response(fuchsia::web::Frame_ExecuteJavaScript_Response());
    callback(std::move(result));
  }
}

void FrameImpl::CreateView(fuchsia::ui::views::ViewToken view_token) {
  // If a View to this Frame is already active then disconnect it.
  TearDownView();

  ui::PlatformWindowInitProperties properties;
  properties.view_token = std::move(view_token);

  window_tree_host_ =
      std::make_unique<ScenicWindowTreeHost>(std::move(properties));
  window_tree_host_->InitHost();

  window_tree_host_->window()->GetHost()->AddEventRewriter(
      &discarding_event_filter_);
  focus_controller_ =
      std::make_unique<wm::FocusController>(new FrameFocusRules);

  aura::client::SetFocusClient(root_window(), focus_controller_.get());
  wm::SetActivationClient(root_window(), focus_controller_.get());

  // Add hooks which automatically set the focus state when input events are
  // received.
  root_window()->AddPreTargetHandler(focus_controller_.get());

  // Track child windows for enforcement of window management policies and
  // propagate window manager events to them (e.g. window resizing).
  root_window()->SetLayoutManager(new LayoutManagerImpl());

  root_window()->AddChild(web_contents_->GetNativeView());
  web_contents_->GetNativeView()->Show();
  window_tree_host_->Show();
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  navigation_controller_.AddBinding(std::move(controller));
}

void FrameImpl::ExecuteJavaScript(std::vector<std::string> origins,
                                  fuchsia::mem::Buffer script,
                                  ExecuteJavaScriptCallback callback) {
  ExecuteJavaScriptInternal(std::move(origins), std::move(script),
                            std::move(callback), true);
}

void FrameImpl::ExecuteJavaScriptNoResult(
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    ExecuteJavaScriptNoResultCallback callback) {
  ExecuteJavaScriptInternal(
      std::move(origins), std::move(script),
      [callback = std::move(callback)](
          fuchsia::web::Frame_ExecuteJavaScript_Result result_with_value) {
        fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result;
        if (result_with_value.is_err()) {
          result.set_err(result_with_value.err());
        } else {
          result.set_response(
              fuchsia::web::Frame_ExecuteJavaScriptNoResult_Response());
        }
        callback(std::move(result));
      },
      false);
}

void FrameImpl::AddBeforeLoadJavaScript(
    uint64_t id,
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    AddBeforeLoadJavaScriptCallback callback) {
  fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result;
  if (!context_->IsJavaScriptInjectionAllowed()) {
    result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
    callback(std::move(result));
    return;
  }

  // Convert the script to UTF8 and store it as a shared memory buffer, so that
  // it can be efficiently shared with multiple renderer processes.
  base::string16 script_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(script, &script_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  // Create a read-only VMO from |script|.
  fuchsia::mem::Buffer script_buffer =
      cr_fuchsia::MemBufferFromString16(script_utf16);

  // Wrap the VMO into a read-only shared-memory container that Mojo can work
  // with.
  base::subtle::PlatformSharedMemoryRegion script_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(script_buffer.vmo),
          base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
          script_buffer.size, base::UnguessableToken::Create());
  script_region.ConvertToReadOnly();
  auto script_region_mojo =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(script_region));

  // If there is no script with the identifier |id|, then create a place for it
  // at the end of the injection sequence.
  if (before_load_scripts_.find(id) == before_load_scripts_.end())
    before_load_scripts_order_.push_back(id);

  before_load_scripts_[id] =
      OriginScopedScript(origins, std::move(script_region_mojo));

  result.set_response(fuchsia::web::Frame_AddBeforeLoadJavaScript_Response());
  callback(std::move(result));
}

void FrameImpl::RemoveBeforeLoadJavaScript(uint64_t id) {
  before_load_scripts_.erase(id);

  for (auto script_id_iter = before_load_scripts_order_.begin();
       script_id_iter != before_load_scripts_order_.end(); ++script_id_iter) {
    if (*script_id_iter == id) {
      before_load_scripts_order_.erase(script_id_iter);
      return;
    }
  }
}

void FrameImpl::PostMessage(std::string origin,
                            fuchsia::web::WebMessage message,
                            PostMessageCallback callback) {
  constexpr char kWildcardOrigin[] = "*";

  fuchsia::web::Frame_PostMessage_Result result;
  if (origin.empty()) {
    result.set_err(fuchsia::web::FrameError::INVALID_ORIGIN);
    callback(std::move(result));
    return;
  }

  if (!message.has_data()) {
    result.set_err(fuchsia::web::FrameError::NO_DATA_IN_MESSAGE);
    callback(std::move(result));
    return;
  }

  base::Optional<base::string16> origin_utf16;
  if (origin != kWildcardOrigin)
    origin_utf16 = base::UTF8ToUTF16(origin);

  base::string16 data_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(message.data(), &data_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  // Include outgoing MessagePorts in the message.
  std::vector<mojo::ScopedMessagePipeHandle> message_ports;
  if (message.has_outgoing_transfer()) {
    // Verify that all the Transferables are valid before we start allocating
    // resources to them.
    for (const fuchsia::web::OutgoingTransferable& outgoing :
         message.outgoing_transfer()) {
      if (!outgoing.is_message_port()) {
        result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
        callback(std::move(result));
        return;
      }
    }

    for (fuchsia::web::OutgoingTransferable& outgoing :
         *message.mutable_outgoing_transfer()) {
      mojo::ScopedMessagePipeHandle port =
          cr_fuchsia::MessagePortFromFidl(std::move(outgoing.message_port()));
      if (!port) {
        result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
        callback(std::move(result));
        return;
      }
      message_ports.push_back(std::move(port));
    }
  }

  content::MessagePortProvider::PostMessageToFrame(
      web_contents_.get(), base::string16(), origin_utf16,
      std::move(data_utf16), std::move(message_ports));
  result.set_response(fuchsia::web::Frame_PostMessage_Response());
  callback(std::move(result));
}

void FrameImpl::SetNavigationEventListener(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener) {
  navigation_controller_.SetEventListener(std::move(listener));
}

void FrameImpl::SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) {
  log_level_ = ConsoleLogLevelToLoggingSeverity(level);
}

void FrameImpl::SetEnableInput(bool enable_input) {
  discarding_event_filter_.set_discard_events(!enable_input);
}

void FrameImpl::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  context_->DestroyFrame(this);
}

bool FrameImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  logging::LogSeverity log_severity =
      blink::ConsoleMessageLevelToLogSeverity(log_level);
  if (log_level_ > log_severity) {
    return false;
  }

  std::string formatted_message =
      base::StringPrintf("%s:%d : %s", base::UTF16ToUTF8(source_id).data(),
                         line_no, base::UTF16ToUTF8(message).data());
  switch (log_level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      LOG(INFO) << "debug:" << formatted_message;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      LOG(INFO) << "info:" << formatted_message;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      LOG(WARNING) << "warn:" << formatted_message;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      LOG(ERROR) << "error:" << formatted_message;
      break;
    default:
      DLOG(WARNING) << "Unknown log level: " << log_severity;
      return false;
  }

  if (console_log_message_hook_)
    console_log_message_hook_.Run(formatted_message);

  return true;
}

bool FrameImpl::ShouldCreateWebContents(
    content::WebContents* web_contents,
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK_EQ(web_contents, web_contents_.get());

  // Prevent any child WebContents (popup windows, tabs, etc.) from spawning.
  // TODO(crbug.com/888131): Implement support for popup windows.
  NOTIMPLEMENTED() << "Ignored popup window request for URL: "
                   << target_url.spec();

  return false;
}

void FrameImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (before_load_scripts_.empty())
    return;

  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage())
    return;

  mojom::OnLoadScriptInjectorAssociatedPtr before_load_script_injector;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&before_load_script_injector);

  // Provision the renderer's ScriptInjector with the scripts scoped to this
  // page's origin.
  before_load_script_injector->ClearOnLoadScripts();
  for (uint64_t script_id : before_load_scripts_order_) {
    const OriginScopedScript& script = before_load_scripts_[script_id];
    if (IsOriginWhitelisted(navigation_handle->GetURL(), script.origins())) {
      before_load_script_injector->AddOnLoadScript(
          mojo::WrapReadOnlySharedMemoryRegion(script.script().Duplicate()));
    }
  }
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  context_->OnDebugDevToolsPortReady();
}
