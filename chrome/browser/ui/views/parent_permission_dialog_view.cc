// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/parent_permission_dialog_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extension_permissions_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {
constexpr int kSectionPaddingTop = 20;

// Whether to auto confirm the dialog for test.
bool auto_confirm_dialog_for_test = false;

// Status to use for auto-confirmation for test.
internal::ParentPermissionDialogViewResult::Status
    auto_confirm_status_for_test =
        internal::ParentPermissionDialogViewResult::Status::kAccepted;

views::Widget* widget_for_test = nullptr;

}  // namespace

void SetAutoConfirmParentPermissionDialogForTest(
    internal::ParentPermissionDialogViewResult::Status status) {
  auto_confirm_dialog_for_test = true;
  auto_confirm_status_for_test = status;
}

// Creates a view for the parent approvals section of the extension info and
// listens for updates to its controls. The view added to the parent contains a
// parent email selection drop-down box, and a password entry field.
class ParentPermissionSection : public views::TextfieldController {
 public:
  ParentPermissionSection(ParentPermissionDialogView* main_view,
                          const ParentPermissionDialogView::Params& params,
                          int available_width)
      : main_view_(main_view) {
    const std::vector<base::string16>& parent_email_addresses =
        params.parent_permission_email_addresses;
    DCHECK_GT(parent_email_addresses.size(), 0u);

    auto view = std::make_unique<views::View>();

    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    if (parent_email_addresses.size() > 1) {
      // If there is more than one parent listed, show radio buttons.
      auto select_parent_label = std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(
              IDS_PARENT_PERMISSION_PROMPT_SELECT_PARENT_LABEL),
          CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_PRIMARY);
      select_parent_label->SetHorizontalAlignment(
          gfx::HorizontalAlignment::ALIGN_LEFT);
      view->AddChildView(std::move(select_parent_label));

      // Add first parent radio button
      auto parent_0_radio_button = std::make_unique<views::RadioButton>(
          base::string16(parent_email_addresses[0]), 1 /* group */);

      // Add a subscription
      parent_0_subscription_ =
          parent_0_radio_button->AddCheckedChangedCallback(base::BindRepeating(
              [](ParentPermissionDialogView* main_view,
                 const base::string16& parent_email) {
                main_view->set_selected_parent_permission_email_address(
                    parent_email);
              },
              main_view, parent_email_addresses[0]));

      // Select parent 0 by default.
      parent_0_radio_button->SetChecked(true);
      view->AddChildView(std::move(parent_0_radio_button));

      // Add second parent radio button.
      auto parent_1_radio_button = std::make_unique<views::RadioButton>(
          base::string16(parent_email_addresses[1]), 1 /* group */);

      parent_1_subscription_ =
          parent_1_radio_button->AddCheckedChangedCallback(base::BindRepeating(
              [](ParentPermissionDialogView* main_view,
                 const base::string16& parent_email) {
                main_view->set_selected_parent_permission_email_address(
                    parent_email);
              },
              main_view, parent_email_addresses[1]));

      view->AddChildView(std::move(parent_1_radio_button));

      // Default to first parent in the response.
      main_view_->set_selected_parent_permission_email_address(
          parent_email_addresses[0]);
    } else {
      // If there is just one parent, show a label with that parent's email.
      auto parent_email_label = std::make_unique<views::Label>(
          parent_email_addresses[0], CONTEXT_BODY_TEXT_LARGE,
          views::style::STYLE_SECONDARY);
      parent_email_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      parent_email_label->SetMultiLine(true);
      parent_email_label->SizeToFit(available_width);
      view->AddChildView(std::move(parent_email_label));
      // Since there is only one parent, just set the output value of selected
      // parent email address here..
      main_view->set_selected_parent_permission_email_address(
          parent_email_addresses[0]);
    }

    // Add the credential input field.
    base::string16 enter_password_string = l10n_util::GetStringUTF16(
        IDS_PARENT_PERMISSION_PROMPT_ENTER_PASSWORD_LABEL);
    auto enter_password_label = std::make_unique<views::Label>(
        enter_password_string, CONTEXT_BODY_TEXT_LARGE,
        views::style::STYLE_SECONDARY);
    enter_password_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    view->AddChildView(std::move(enter_password_label));

    auto credential_input_field = std::make_unique<views::Textfield>();
    credential_input_field->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    credential_input_field->SetAccessibleName(enter_password_string);
    credential_input_field->RequestFocus();
    credential_input_field->set_controller(this);
    view->AddChildView(std::move(credential_input_field));

    const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    const gfx::Insets content_insets =
        provider->GetDialogInsetsForContentType(views::CONTROL, views::CONTROL);
    view->SetBorder(views::CreateEmptyBorder(
        kSectionPaddingTop, content_insets.left(), 0, content_insets.right()));

    // Add to main view.
    main_view->AddChildView(std::move(view));
  }

  ParentPermissionSection(const ParentPermissionSection&) = delete;
  ParentPermissionSection& operator=(const ParentPermissionSection&) = delete;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    main_view_->set_parent_permission_credential(new_contents);
  }

 private:
  void OnParentRadioButtonSelected(ParentPermissionDialogView* main_view,
                                   const base::string16& parent_email) {
    main_view->set_selected_parent_permission_email_address(parent_email);
  }

  views::PropertyChangedSubscription parent_0_subscription_;
  views::PropertyChangedSubscription parent_1_subscription_;

  // Owned by the parent view class, not this class.
  ParentPermissionDialogView* main_view_;
};

// ParentPermissionDialogView::Params
ParentPermissionDialogView::Params::Params() = default;
ParentPermissionDialogView::Params::Params(const Params& params) = default;
ParentPermissionDialogView::Params::~Params() = default;

// ParentPermissionDialogView
ParentPermissionDialogView::ParentPermissionDialogView(
    std::unique_ptr<Params> params,
    ParentPermissionDialogView::DoneCallback done_callback)
    : params_(std::move(params)), done_callback_(std::move(done_callback)) {
  DialogDelegate::SetDefaultButton(ui::DIALOG_BUTTON_OK);
  DialogDelegate::set_draggable(true);
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_PARENT_PERMISSION_PROMPT_APPROVE_BUTTON));
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_PARENT_PERMISSION_PROMPT_CANCEL_BUTTON));
}

ParentPermissionDialogView::~ParentPermissionDialogView() = default;

base::string16 ParentPermissionDialogView::GetActiveUserFirstName() const {
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user = manager->GetActiveUser();
  return user->GetGivenName();
}

gfx::Size ParentPermissionDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void ParentPermissionDialogView::AddedToWidget() {
  auto message_container = std::make_unique<views::View>();

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  views::GridLayout* layout = message_container->SetLayoutManager(
      std::make_unique<views::GridLayout>());
  constexpr int kTitleColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kTitleColumnSetId);
  constexpr int icon_size = extension_misc::EXTENSION_ICON_SMALL;
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                        icon_size, 0);

  // Equalize padding on the left and the right of the icon.
  column_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetInsetsMetric(views::INSETS_DIALOG).left());
  // Set a resize weight so that the message label will be expanded to the
  // available width.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                        1.0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, kTitleColumnSetId);

  // Scale down to icon size, but allow smaller icons (don't scale up).
  if (!params().icon.isNull()) {
    const gfx::ImageSkia& image = params().icon;
    auto icon = std::make_unique<views::ImageView>();
    gfx::Size size(image.width(), image.height());
    size.SetToMin(gfx::Size(icon_size, icon_size));
    icon->SetImageSize(size);
    icon->SetImage(image);
    layout->AddView(std::move(icon));
  }

  DCHECK(!params().message.empty());
  std::unique_ptr<views::Label> message_label =
      views::BubbleFrameView::CreateDefaultTitleLabel(params().message);
  // Setting the message's preferred size to 0 ensures it won't influence the
  // overall size of the dialog. It will be expanded by GridLayout.
  message_label->SetPreferredSize(gfx::Size(0, 0));
  layout->AddView(std::move(message_label));

  GetBubbleFrameView()->SetTitleView(std::move(message_container));
}

bool ParentPermissionDialogView::Cancel() {
  // This can be called multiple times because ParentPermissionDialog
  // calls a callback pointing to OnDialogCloseClosure(), and if this object
  // still exists at that time, this method will get called again because
  // Cancel() is called by default when the dialog is explicitly asked to close.
  // Therefore, we null check the callback here before trying to use it.
  if (!done_callback_)
    return true;

  internal::ParentPermissionDialogViewResult result;
  result.status = internal::ParentPermissionDialogViewResult::Status::kCanceled;
  std::move(done_callback_).Run(result);
  return true;
}

bool ParentPermissionDialogView::Accept() {
  internal::ParentPermissionDialogViewResult result;
  result.status = internal::ParentPermissionDialogViewResult::Status::kAccepted;
  result.parent_permission_credential = parent_permission_credential_;
  result.selected_parent_permission_email = selected_parent_permission_email_;
  std::move(done_callback_).Run(result);
  return true;
}

bool ParentPermissionDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 ParentPermissionDialogView::GetAccessibleWindowTitle() const {
  return params().message;
}

ui::ModalType ParentPermissionDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ParentPermissionDialogView::CreateContents() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto section_container = std::make_unique<views::View>();
  const gfx::Insets content_insets =
      provider->GetDialogInsetsForContentType(views::CONTROL, views::CONTROL);
  set_margins(gfx::Insets(content_insets.top(), 0, content_insets.bottom(), 0));
  section_container->SetBorder(views::CreateEmptyBorder(
      kSectionPaddingTop, content_insets.left(), 0, content_insets.right()));
  section_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  const int content_width =
      GetPreferredSize().width() - section_container->GetInsets().width();

  if (params().extension) {
    if (!prompt_permissions_.permissions.empty()) {
      // Set up the permissions header string.
      const extensions::Extension* extension = params().extension;
      // Shouldn't be asking for permissions for theme installs.
      DCHECK(!extension->is_theme());
      base::string16 extension_type;
      if (extension->is_extension()) {
        extension_type = l10n_util::GetStringUTF16(
            IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_EXTENSION);
      } else if (extension->is_app()) {
        extension_type = l10n_util::GetStringUTF16(
            IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_APP);
      }
      base::string16 permission_header_label = l10n_util::GetStringFUTF16(
          IDS_PARENT_PERMISSION_PROMPT_CHILD_WANTS_TO_INSTALL_LABEL,
          GetActiveUserFirstName(), extension_type);

      views::Label* permissions_header =
          new views::Label(permission_header_label, CONTEXT_BODY_TEXT_LARGE);
      permissions_header->SetMultiLine(true);
      permissions_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      permissions_header->SizeToFit(content_width);
      permissions_header->SetBorder(views::CreateEmptyBorder(
          0, content_insets.left(), 0, content_insets.right()));

      // Add this outside the scrolling section, so it can't be obscured by
      // scrolling.
      AddChildView(permissions_header);

      // Create permissions view.
      auto permissions_view =
          std::make_unique<ExtensionPermissionsView>(content_width);
      permissions_view->AddPermissions(prompt_permissions_);

      // Add to the section container, so the permissions can scroll, since they
      // can be arbitrarily long.
      section_container->AddChildView(std::move(permissions_view));
    }

    // Add permissions view to the enclosing scroll view.
    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->SetHideHorizontalScrollBar(true);
    scroll_view->SetContents(std::move(section_container));
    scroll_view->ClipHeightTo(
        0, provider->GetDistanceMetric(
               views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
    AddChildView(std::move(scroll_view));
  }

  // Create the parent approval view, which adds itself
  // to the main view.
  parent_permission_section_ =
      std::make_unique<ParentPermissionSection>(this, params(), content_width);

  // Show the "password incorrect" label if needed.
  if (params().show_parent_password_incorrect) {
    auto password_incorrect_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(
            IDS_PARENT_PERMISSION_PROMPT_PASSWORD_INCORRECT_LABEL),
        CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY);
    password_incorrect_label->SetBorder(views::CreateEmptyBorder(
        0, content_insets.left(), 0, content_insets.right()));
    password_incorrect_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    password_incorrect_label->SetMultiLine(true);
    password_incorrect_label->SetEnabledColor(gfx::kGoogleRed500);
    password_incorrect_label->SizeToFit(content_width);
    AddChildView(std::move(password_incorrect_label));
  }
}

void ParentPermissionDialogView::ShowDialog() {
  if (params().extension)
    InitializeExtensionData(params().extension);
  ShowDialogInternal();
}

void ParentPermissionDialogView::ShowDialogInternal() {
  // The contents have to be created here, instead of during construction
  // because they can potentially rely on the side effects of loading info
  // from an extension.
  CreateContents();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PARENT_PERMISSION);
  views::Widget* widget =
      constrained_window::CreateBrowserModalDialogViews(this, params().window);
  widget->Show();

  // If we are in a test, auto-confirm the dialog since we can't click
  // on it directly.
  if (auto_confirm_dialog_for_test) {
    widget_for_test = widget;
    switch (auto_confirm_status_for_test) {
      case internal::ParentPermissionDialogViewResult::Status::kCanceled:
        CancelDialog();
        break;
      case internal::ParentPermissionDialogViewResult::Status::kAccepted:
        AcceptDialog();
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void ParentPermissionDialogView::CloseDialogView() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

base::OnceClosure ParentPermissionDialogView::GetCloseDialogClosure() {
  return base::BindOnce(&ParentPermissionDialogView::CloseDialogView,
                        weak_factory_.GetWeakPtr());
}

void ParentPermissionDialogView::InitializeExtensionData(
    scoped_refptr<const extensions::Extension> extension) {
  DCHECK(extension);

  // Load Permissions.
  std::unique_ptr<const extensions::PermissionSet> permissions_to_display =
      extensions::util::GetInstallPromptPermissionSetForExtension(
          extension.get(), params().profile,
          true /* include_optional_permissions */);
  extensions::Manifest::Type type = extension->GetType();
  prompt_permissions_.LoadFromPermissionSet(permissions_to_display.get(), type);

  params_->message = l10n_util::GetStringFUTF16(
      IDS_PARENT_PERMISSION_PROMPT_GO_GET_A_PARENT_FOR_EXTENSION_LABEL,
      base::UTF8ToUTF16(extension->name()));
}

base::OnceClosure ShowParentPermissionDialog(
    Profile* profile,
    gfx::NativeWindow window,
    const std::vector<base::string16>& parent_permission_email_addresses,
    bool show_parent_password_incorrect,
    const gfx::ImageSkia& icon,
    const base::string16& message,
    const extensions::Extension* extension,
    base::OnceCallback<void(internal::ParentPermissionDialogViewResult result)>
        view_done_callback) {
  auto params = std::make_unique<ParentPermissionDialogView::Params>();
  params->parent_permission_email_addresses = parent_permission_email_addresses;
  params->show_parent_password_incorrect = show_parent_password_incorrect;
  params->extension = extension;
  params->message = message;
  params->icon = icon;
  params->profile = profile;
  params->window = window;

  ParentPermissionDialogView* dialog_view = new ParentPermissionDialogView(
      std::move(params), std::move(view_done_callback));

  // Ownership of dialog_view is passed to the views system when the dialog is
  // shown.
  dialog_view->ShowDialog();

  return dialog_view->GetCloseDialogClosure();
}
