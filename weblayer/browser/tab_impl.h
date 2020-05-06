// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TAB_IMPL_H_
#define WEBLAYER_BROWSER_TAB_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/find_in_page/find_result_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_controls_state.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/public/tab.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace autofill {
class AutofillProvider;
}  // namespace autofill

namespace content {
class RenderWidgetHostView;
class WebContents;
struct ContextMenuParams;
struct WebPreferences;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace sessions {
class SessionTabHelperDelegate;
}

namespace weblayer {
class BrowserImpl;
class FullscreenDelegate;
class NavigationControllerImpl;
class NewTabDelegate;
class ProfileImpl;

#if defined(OS_ANDROID)
class BrowserControlsContainerView;
enum class ControlsVisibilityReason;
#endif

class TabImpl : public Tab,
                public content::WebContentsDelegate,
                public content::WebContentsObserver,
                public find_in_page::FindResultObserver {
 public:
  enum class ScreenShotErrors {
    kNone = 0,
    kScaleOutOfRange,
    kTabNotActive,
    kWebContentsNotVisible,
    kNoSurface,
    kNoRenderWidgetHostView,
    kNoWindowAndroid,
    kEmptyViewport,
    kHiddenByControls,
    kScaledToEmpty,
    kCaptureFailed,
    kBitmapAllocationFailed,
  };

  // TODO(sky): investigate a better way to not have so many ifdefs.
#if defined(OS_ANDROID)
  TabImpl(ProfileImpl* profile,
          const base::android::JavaParamRef<jobject>& java_impl);
#endif
  explicit TabImpl(ProfileImpl* profile,
                   std::unique_ptr<content::WebContents> = nullptr,
                   const std::string& guid = std::string());
  ~TabImpl() override;

  // Returns the TabImpl from the specified WebContents (which may be null), or
  // null if |web_contents| was not created by a TabImpl.
  static TabImpl* FromWebContents(content::WebContents* web_contents);

  ProfileImpl* profile() { return profile_; }

  void set_browser(BrowserImpl* browser) { browser_ = browser; }
  BrowserImpl* browser() { return browser_; }

  content::WebContents* web_contents() const { return web_contents_.get(); }

  bool has_new_tab_delegate() const { return new_tab_delegate_ != nullptr; }

  // Called from Browser when this Tab is losing active status.
  void OnLosingActive();

  bool IsActive();

  void ShowContextMenu(const content::ContextMenuParams& params);

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> GetJavaTab() {
    return java_impl_;
  }

  // Call this method to disable integration with the system-level Autofill
  // infrastructure. Useful in conjunction with InitializeAutofillForTests().
  // Should be called early in the lifetime of WebLayer, and in
  // particular, must be called before the TabImpl is attached to the browser
  // on the Java side to have the desired effect.
  static void DisableAutofillSystemIntegrationForTesting();

  base::android::ScopedJavaLocalRef<jobject> GetWebContents(JNIEnv* env);
  void SetBrowserControlsContainerViews(
      JNIEnv* env,
      jlong native_top_browser_controls_container_view,
      jlong native_bottom_browser_controls_container_view);
  void ExecuteScript(JNIEnv* env,
                     const base::android::JavaParamRef<jstring>& script,
                     bool use_separate_isolate,
                     const base::android::JavaParamRef<jobject>& callback);
  void SetJavaImpl(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& impl);

  // Invoked every time that the Java-side AutofillProvider instance is
  // changed (set to null or to a new object). On first invocation with a non-
  // null object initializes the native Autofill infrastructure. On
  // subsequent invocations updates the association of that native
  // infrastructure with its Java counterpart.
  void OnAutofillProviderChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& autofill_provider);

  void UpdateBrowserControlsState(JNIEnv* env, jint constraint);

  base::android::ScopedJavaLocalRef<jstring> GetGuid(JNIEnv* env);

  void CaptureScreenShot(
      JNIEnv* env,
      jfloat scale,
      const base::android::JavaParamRef<jobject>& valueCallback);
#endif

  ErrorPageDelegate* error_page_delegate() { return error_page_delegate_; }

  // Tab:
  void SetErrorPageDelegate(ErrorPageDelegate* delegate) override;
  void SetFullscreenDelegate(FullscreenDelegate* delegate) override;
  void SetNewTabDelegate(NewTabDelegate* delegate) override;
  void AddObserver(TabObserver* observer) override;
  void RemoveObserver(TabObserver* observer) override;
  NavigationController* GetNavigationController() override;
  void ExecuteScript(const base::string16& script,
                     bool use_separate_isolate,
                     JavaScriptResultCallback callback) override;
  const std::string& GetGuid() override;
#if !defined(OS_ANDROID)
  void AttachToView(views::WebView* web_view) override;
#endif

  void WebPreferencesChanged();
  void SetWebPreferences(content::WebPreferences* prefs);

  // Executes |script| with a user gesture.
  void ExecuteScriptWithUserGestureForTests(const base::string16& script);

  // Initializes the autofill system with |provider| for tests.
  void InitializeAutofillForTests(
      std::unique_ptr<autofill::AutofillProvider> provider);

 private:
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* web_contents) override;
  content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  int GetTopControlsHeight() override;
  int GetBottomControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* web_contents) override;
  bool EmbedsFullscreenWidget() override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void EnterFullscreenModeForTab(
      content::WebContents* web_contents,
      const GURL& origin,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
#if defined(OS_ANDROID)
  void FindMatchRectsReply(content::WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override;

  // Pointer arguments are outputs. Check the preconditions for capturing a
  // screenshot and either set all outputs, or return an error code, in which
  // case the state of output arguments is undefined.
  ScreenShotErrors PrepareForCaptureScreenShot(
      float scale,
      content::RenderWidgetHostView** rwhv,
      gfx::Rect* src_rect,
      gfx::Size* output_size);
#endif

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidChangeVisibleSecurityState() override;

  // find_in_page::FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;

  // Called from closure supplied to delegate to exit fullscreen.
  void OnExitFullscreen();

  void UpdateRendererPrefs(bool should_sync_prefs);

  void InitializeAutofill();

  // Returns the FindTabHelper for the page, or null if none exists.
  find_in_page::FindTabHelper* GetFindTabHelper();

  sessions::SessionTabHelperDelegate* GetSessionServiceTabHelperDelegate(
      content::WebContents* web_contents);

#if defined(OS_ANDROID)
  void SetBrowserControlsConstraint(ControlsVisibilityReason reason,
                                    content::BrowserControlsState constraint);
#endif

  void UpdateBrowserVisibleSecurityStateIfNecessary();

  BrowserImpl* browser_ = nullptr;
  ErrorPageDelegate* error_page_delegate_ = nullptr;
  FullscreenDelegate* fullscreen_delegate_ = nullptr;
  NewTabDelegate* new_tab_delegate_ = nullptr;
  ProfileImpl* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NavigationControllerImpl> navigation_controller_;
  base::ObserverList<TabObserver>::Unchecked observers_;
  std::unique_ptr<i18n::LocaleChangeSubscription> locale_change_subscription_;
#if defined(OS_ANDROID)
  BrowserControlsContainerView* top_controls_container_view_ = nullptr;
  BrowserControlsContainerView* bottom_controls_container_view_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
  base::OneShotTimer update_browser_controls_state_timer_;
#endif

  bool is_fullscreen_ = false;
  // Set to true doing EnterFullscreenModeForTab().
  bool processing_enter_fullscreen_ = false;

  std::unique_ptr<autofill::AutofillProvider> autofill_provider_;

  const std::string guid_;

  base::string16 title_;

  base::WeakPtrFactory<TabImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TAB_IMPL_H_
