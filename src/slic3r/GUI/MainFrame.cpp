#include "MainFrame.hpp"

#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/progdlg.h>
#include <wx/tooltip.h>
//#include <wx/glcanvas.h>
#include <wx/debug.h>

#include <boost/algorithm/string/predicate.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/SLAPrint.hpp"

#include "Tab.hpp"
#include "PresetBundle.hpp"
#include "ProgressStatusBar.hpp"
#include "3DScene.hpp"
#include "AppConfig.hpp"
#include "PrintHostDialogs.hpp"
#include "wxExtensions.hpp"
#include "GUI_ObjectList.hpp"
#include "Mouse3DController.hpp"
#include "RemovableDriveManager.hpp"
#include "InstanceCheck.hpp"
#include "I18N.hpp"
#include "GLCanvas3D.hpp"
#include "Plater.hpp"

#include <fstream>
#include "GUI_App.hpp"

#ifdef _WIN32
#include <dbt.h>
#include <shlobj.h>
#endif // _WIN32

namespace Slic3r {
namespace GUI {

#if ENABLE_LAYOUT_NO_RESTART
enum class ERescaleTarget
{
    Mainframe,
    SettingsDialog
};

static void rescale_dialog_after_dpi_change(MainFrame& mainframe, SettingsDialog& dialog, ERescaleTarget target)
{
    int mainframe_dpi = get_dpi_for_window(&mainframe);
    int dialog_dpi = get_dpi_for_window(&dialog);
    if (mainframe_dpi != dialog_dpi) {
        if (target == ERescaleTarget::SettingsDialog) {
            dialog.enable_force_rescale();
#if wxVERSION_EQUAL_OR_GREATER_THAN(3,1,3)
            dialog.GetEventHandler()->AddPendingEvent(wxDPIChangedEvent(wxSize(mainframe_dpi, mainframe_dpi), wxSize(dialog_dpi, dialog_dpi)));
#else
            dialog.GetEventHandler()->AddPendingEvent(DpiChangedEvent(EVT_DPI_CHANGED_SLICER, dialog_dpi, dialog.GetRect()));
#endif // wxVERSION_EQUAL_OR_GREATER_THAN
        } else {
#if wxVERSION_EQUAL_OR_GREATER_THAN(3,1,3)
            mainframe.GetEventHandler()->AddPendingEvent(wxDPIChangedEvent(wxSize(dialog_dpi, dialog_dpi), wxSize(mainframe_dpi, mainframe_dpi)));
#else
            mainframe.enable_force_rescale();
            mainframe.GetEventHandler()->AddPendingEvent(DpiChangedEvent(EVT_DPI_CHANGED_SLICER, mainframe_dpi, mainframe.GetRect()));
#endif // wxVERSION_EQUAL_OR_GREATER_THAN
        }
    }
}
#endif // ENABLE_LAYOUT_NO_RESTART

MainFrame::MainFrame() :
DPIFrame(NULL, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "mainframe"),
    m_printhost_queue_dlg(new PrintHostQueueDialog(this))
    , m_recent_projects(9)
#if ENABLE_LAYOUT_NO_RESTART
    , m_settings_dialog(this)
#endif // ENABLE_LAYOUT_NO_RESTART
{
    // Fonts were created by the DPIFrame constructor for the monitor, on which the window opened.
    wxGetApp().update_fonts(this);
/*
#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList 
    this->SetFont(this->normal_font());
#endif
    // Font is already set in DPIFrame constructor
*/
    // Load the icon either from the exe, or from the ico file.
#if _WIN32
    {
        TCHAR szExeFileName[MAX_PATH];
        GetModuleFileName(nullptr, szExeFileName, MAX_PATH);
        SetIcon(wxIcon(szExeFileName, wxBITMAP_TYPE_ICO));
    }
#else
    SetIcon(wxIcon(Slic3r::var("PrusaSlicer_128px.png"), wxBITMAP_TYPE_PNG));
#endif // _WIN32

	// initialize status bar
	m_statusbar = std::make_shared<ProgressStatusBar>(this);
    m_statusbar->set_font(GUI::wxGetApp().normal_font());
	m_statusbar->embed(this);
    m_statusbar->set_status_text(_(L("Version")) + " " +
		SLIC3R_VERSION +
		_(L(" - Remember to check for updates at http://github.com/prusa3d/PrusaSlicer/releases")));

    /* Load default preset bitmaps before a tabpanel initialization,
     * but after filling of an em_unit value 
     */
    wxGetApp().preset_bundle->load_default_preset_bitmaps();

    // initialize tabpanel and menubar
    init_tabpanel();
    init_menubar();

    // set default tooltip timer in msec
    // SetAutoPop supposedly accepts long integers but some bug doesn't allow for larger values
    // (SetAutoPop is not available on GTK.)
    wxToolTip::SetAutoPop(32767);

    m_loaded = true;

#if !ENABLE_LAYOUT_NO_RESTART
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    // So, if we haven't possibility to set MinSize() for the MainFrame, 
    // set the MinSize() as a half of regular  for the m_plater and m_tabpanel, when settings layout is in slNew mode
    // Otherwise, MainFrame will be maximized by height
    if (slNew) {
        wxSize size = wxGetApp().get_min_size();
        size.SetHeight(int(0.5*size.GetHeight()));
        m_plater->SetMinSize(size);
        m_tabpanel->SetMinSize(size);
    }
#endif
#endif // !ENABLE_LAYOUT_NO_RESTART

    // initialize layout
    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_main_sizer, 1, wxEXPAND);
#if ENABLE_LAYOUT_NO_RESTART
    SetSizer(sizer);
    // initialize layout from config
    update_layout();
    sizer->SetSizeHints(this);
    Fit();
#else
    if (m_plater && m_layout != slOld)
        sizer->Add(m_plater, 1, wxEXPAND);

    if (m_tabpanel && m_layout != slDlg)
        sizer->Add(m_tabpanel, 1, wxEXPAND);

    sizer->SetSizeHints(this);
    SetSizer(sizer);
    Fit();
#endif // !ENABLE_LAYOUT_NO_RESTART

    const wxSize min_size = wxGetApp().get_min_size(); //wxSize(76*wxGetApp().em_unit(), 49*wxGetApp().em_unit());
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    SetSize(min_size/*wxSize(760, 490)*/);
#else
    SetMinSize(min_size/*wxSize(760, 490)*/);
    SetSize(GetMinSize());
#endif
    Layout();

    update_title();

    // declare events
    Bind(wxEVT_CREATE, [this](wxWindowCreateEvent& event) {

#ifdef _WIN32
		//static GUID GUID_DEVINTERFACE_USB_DEVICE	= { 0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED };
		//static GUID GUID_DEVINTERFACE_DISK 		= { 0x53f56307, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b };
		//static GUID GUID_DEVINTERFACE_VOLUME 	    = { 0x71a27cdd, 0x812a, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f };
		static GUID GUID_DEVINTERFACE_HID			= { 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 };

    	// Register USB HID (Human Interface Devices) notifications to trigger the 3DConnexion enumeration.
		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_HID;
		m_hDeviceNotify = ::RegisterDeviceNotification(this->GetHWND(), &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

// or register for file handle change?
//		DEV_BROADCAST_HANDLE NotificationFilter = { 0 };
//		NotificationFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
//		NotificationFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;

		// Using Win32 Shell API to register for media insert / removal events.
		LPITEMIDLIST ppidl;
		if (SHGetSpecialFolderLocation(this->GetHWND(), CSIDL_DESKTOP, &ppidl) == NOERROR) {
			SHChangeNotifyEntry shCNE;
			shCNE.pidl       = ppidl;
			shCNE.fRecursive = TRUE;
			// Returns a positive integer registration identifier (ID).
			// Returns zero if out of memory or in response to invalid parameters.
			m_ulSHChangeNotifyRegister = SHChangeNotifyRegister(this->GetHWND(),		// Hwnd to receive notification
				SHCNE_DISKEVENTS,														// Event types of interest (sources)
				SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED,
				//SHCNE_UPDATEITEM,														// Events of interest - use SHCNE_ALLEVENTS for all events
				WM_USER_MEDIACHANGED,													// Notification message to be sent upon the event
				1,																		// Number of entries in the pfsne array
				&shCNE);																// Array of SHChangeNotifyEntry structures that 
																						// contain the notifications. This array should 
																						// always be set to one when calling SHChnageNotifyRegister
																						// or SHChangeNotifyDeregister will not work properly.
			assert(m_ulSHChangeNotifyRegister != 0);    // Shell notification failed
		} else {
			// Failed to get desktop location
			assert(false); 
		}

		{
			static constexpr int device_count = 1;
			RAWINPUTDEVICE devices[device_count] = { 0 };
			// multi-axis mouse (SpaceNavigator, etc.)
			devices[0].usUsagePage = 0x01;
			devices[0].usUsage = 0x08;
			if (! RegisterRawInputDevices(devices, device_count, sizeof(RAWINPUTDEVICE)))
				BOOST_LOG_TRIVIAL(error) << "RegisterRawInputDevices failed";
		}
#endif // _WIN32

        // propagate event
        event.Skip();
    });

    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        if (event.CanVeto() && !wxGetApp().check_unsaved_changes()) {
            event.Veto();
            return;
        }
        this->shutdown();
        // propagate event
        event.Skip();
    });

    Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& event) {
        if (m_plater != nullptr && event.GetActive())
            m_plater->on_activate();
        event.Skip();
    });

    wxGetApp().persist_window_geometry(this, true);
#if ENABLE_LAYOUT_NO_RESTART
    wxGetApp().persist_window_geometry(&m_settings_dialog, true);
#else
    if (m_settings_dialog != nullptr)
        wxGetApp().persist_window_geometry(m_settings_dialog, true);
#endif // ENABLE_LAYOUT_NO_RESTART

    update_ui_from_settings();    // FIXME (?)

    if (m_plater != nullptr)
        m_plater->show_action_buttons(true);
}

#if ENABLE_LAYOUT_NO_RESTART
void MainFrame::update_layout()
{
    auto restore_to_creation = [this]() {
        auto clean_sizer = [](wxSizer* sizer) {
            while (!sizer->GetChildren().IsEmpty()) {
                sizer->Detach(0);
            }
        };

        // On Linux m_plater needs to be removed from m_tabpanel before to reparent it
        int plater_page_id = m_tabpanel->FindPage(m_plater);
        if (plater_page_id != wxNOT_FOUND)
            m_tabpanel->RemovePage(plater_page_id);

        if (m_plater->GetParent() != this)
            m_plater->Reparent(this);

        if (m_tabpanel->GetParent() != this)
            m_tabpanel->Reparent(this);

        plater_page_id = (m_plater_page != nullptr) ? m_tabpanel->FindPage(m_plater_page) : wxNOT_FOUND;
        if (plater_page_id != wxNOT_FOUND) {
            m_tabpanel->DeletePage(plater_page_id);
            m_plater_page = nullptr;
        }

        if (m_layout == ESettingsLayout::Dlg)
            rescale_dialog_after_dpi_change(*this, m_settings_dialog, ERescaleTarget::Mainframe);

        clean_sizer(m_main_sizer);
        clean_sizer(m_settings_dialog.GetSizer());

        if (m_settings_dialog.IsShown())
            m_settings_dialog.Close();

        m_tabpanel->Hide();
        m_plater->Hide();

        Layout();
    };

    ESettingsLayout layout = wxGetApp().app_config->get("old_settings_layout_mode") == "1" ? ESettingsLayout::Old :
        wxGetApp().app_config->get("new_settings_layout_mode") == "1" ? ESettingsLayout::New :
        wxGetApp().app_config->get("dlg_settings_layout_mode") == "1" ? ESettingsLayout::Dlg : ESettingsLayout::Old;

    if (m_layout == layout)
        return;

    wxBusyCursor busy;

    Freeze();

    // Remove old settings
    if (m_layout != ESettingsLayout::Unknown)
        restore_to_creation();

    m_layout = layout;

    // From the very beginning the Print settings should be selected
    m_last_selected_tab = m_layout == ESettingsLayout::Dlg ? 0 : 1;

    // Set new settings
    switch (m_layout)
    {
    case ESettingsLayout::Old:
    {
        m_plater->Reparent(m_tabpanel);
        m_tabpanel->InsertPage(0, m_plater, _L("Plater"));
        m_main_sizer->Add(m_tabpanel, 1, wxEXPAND);
        m_plater->Show();
        m_tabpanel->Show();
        break;
    }
    case ESettingsLayout::New:
    {
        m_main_sizer->Add(m_plater, 1, wxEXPAND);
        m_tabpanel->Hide();
        m_main_sizer->Add(m_tabpanel, 1, wxEXPAND);
        m_plater_page = new wxPanel(m_tabpanel);
        m_tabpanel->InsertPage(0, m_plater_page, _L("Plater")); // empty panel just for Plater tab */
        m_plater->Show();
        break;
    }
    case ESettingsLayout::Dlg:
    {
        m_main_sizer->Add(m_plater, 1, wxEXPAND);
        m_tabpanel->Reparent(&m_settings_dialog);
        m_settings_dialog.GetSizer()->Add(m_tabpanel, 1, wxEXPAND);

        rescale_dialog_after_dpi_change(*this, m_settings_dialog, ERescaleTarget::SettingsDialog);

        m_tabpanel->Show();
        m_plater->Show();
        break;
    }
    }

//#ifdef __APPLE__
//    // Using SetMinSize() on Mac messes up the window position in some cases
//    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
//    // So, if we haven't possibility to set MinSize() for the MainFrame, 
//    // set the MinSize() as a half of regular  for the m_plater and m_tabpanel, when settings layout is in slNew mode
//    // Otherwise, MainFrame will be maximized by height
//    if (m_layout == ESettingsLayout::New) {
//        wxSize size = wxGetApp().get_min_size();
//        size.SetHeight(int(0.5 * size.GetHeight()));
//        m_plater->SetMinSize(size);
//        m_tabpanel->SetMinSize(size);
//    }
//#endif
    
    Layout();
    Thaw();
}
#endif // ENABLE_LAYOUT_NO_RESTART

// Called when closing the application and when switching the application language.
void MainFrame::shutdown()
{
#ifdef _WIN32
	if (m_hDeviceNotify) {
		::UnregisterDeviceNotification(HDEVNOTIFY(m_hDeviceNotify));
		m_hDeviceNotify = nullptr;
	}
 	if (m_ulSHChangeNotifyRegister) {
        SHChangeNotifyDeregister(m_ulSHChangeNotifyRegister);
        m_ulSHChangeNotifyRegister = 0;
 	}
#endif // _WIN32

    if (m_plater)
    	m_plater->stop_jobs();

    // Unbinding of wxWidgets event handling in canvases needs to be done here because on MAC,
    // when closing the application using Command+Q, a mouse event is triggered after this lambda is completed,
    // causing a crash
    if (m_plater) m_plater->unbind_canvas_event_handlers();

    // Cleanup of canvases' volumes needs to be done here or a crash may happen on some Linux Debian flavours
    // see: https://github.com/prusa3d/PrusaSlicer/issues/3964
    if (m_plater) m_plater->reset_canvas_volumes();

    // Weird things happen as the Paint messages are floating around the windows being destructed.
    // Avoid the Paint messages by hiding the main window.
    // Also the application closes much faster without these unnecessary screen refreshes.
    // In addition, there were some crashes due to the Paint events sent to already destructed windows.
    this->Show(false);

#if ENABLE_LAYOUT_NO_RESTART
    if (m_settings_dialog.IsShown())
        // call Close() to trigger call to lambda defined into GUI_App::persist_window_geometry()
        m_settings_dialog.Close();
#else
    if (m_settings_dialog != nullptr)
    {
        if (m_settings_dialog->IsShown())
            // call Close() to trigger call to lambda defined into GUI_App::persist_window_geometry()
            m_settings_dialog->Close();

        m_settings_dialog->Destroy();
    }
#endif // ENABLE_LAYOUT_NO_RESTART

	// Stop the background thread (Windows and Linux).
	// Disconnect from a 3DConnextion driver (OSX).
    m_plater->get_mouse3d_controller().shutdown();
	// Store the device parameter database back to appconfig.
    m_plater->get_mouse3d_controller().save_config(*wxGetApp().app_config);

    // Stop the background thread of the removable drive manager, so that no new updates will be sent to the Plater.
    wxGetApp().removable_drive_manager()->shutdown();
	//stop listening for messages from other instances
	wxGetApp().other_instance_message_handler()->shutdown(this);
    // Save the slic3r.ini.Usually the ini file is saved from "on idle" callback,
    // but in rare cases it may not have been called yet.
    wxGetApp().app_config->save();
//         if (m_plater)
//             m_plater->print = undef;
//         Slic3r::GUI::deregister_on_request_update_callback();

    // set to null tabs and a plater
    // to avoid any manipulations with them from App->wxEVT_IDLE after of the mainframe closing 
    wxGetApp().tabs_list.clear();
    wxGetApp().plater_ = nullptr;
}

void MainFrame::update_title()
{
    wxString title = wxEmptyString;
    if (m_plater != nullptr)
    {
        // m_plater->get_project_filename() produces file name including path, but excluding extension.
        // Don't try to remove the extension, it would remove part of the file name after the last dot!
        wxString project = from_path(into_path(m_plater->get_project_filename()).filename());
        if (!project.empty())
            title += (project + " - ");
    }

    std::string build_id = SLIC3R_BUILD_ID;
    size_t 		idx_plus = build_id.find('+');
    if (idx_plus != build_id.npos) {
    	// Parse what is behind the '+'. If there is a number, then it is a build number after the label, and full build ID is shown.
    	int commit_after_label;
    	if (! boost::starts_with(build_id.data() + idx_plus + 1, "UNKNOWN") && 
            (build_id.at(idx_plus + 1) == '-' || sscanf(build_id.data() + idx_plus + 1, "%d-", &commit_after_label) == 0)) {
    		// It is a release build.
    		build_id.erase(build_id.begin() + idx_plus, build_id.end());    		
#if defined(_WIN32) && ! defined(_WIN64)
    		// People are using 32bit slicer on a 64bit machine by mistake. Make it explicit.
            build_id += " 32 bit";
#endif
    	}
    }
    title += (wxString(build_id) + " " + _(L("based on Slic3r")));

    SetTitle(title);
}

void MainFrame::init_tabpanel()
{
#if ENABLE_LAYOUT_NO_RESTART
    // wxNB_NOPAGETHEME: Disable Windows Vista theme for the Notebook background. The theme performance is terrible on Windows 10
    // with multiple high resolution displays connected.
    m_tabpanel = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
    m_tabpanel->SetFont(Slic3r::GUI::wxGetApp().normal_font());
#endif
    m_tabpanel->Hide();
    m_settings_dialog.set_tabpanel(m_tabpanel);
#else
    m_layout = wxGetApp().app_config->get("old_settings_layout_mode") == "1" ? slOld :
        wxGetApp().app_config->get("new_settings_layout_mode") == "1" ? slNew :
        wxGetApp().app_config->get("dlg_settings_layout_mode") == "1" ? slDlg : slOld;

    // From the very beginning the Print settings should be selected
    m_last_selected_tab = m_layout == slDlg ? 0 : 1;

    if (m_layout == slDlg) {
        m_settings_dialog = new SettingsDialog(this);
        m_tabpanel = m_settings_dialog->get_tabpanel();
    }
    else {
        // wxNB_NOPAGETHEME: Disable Windows Vista theme for the Notebook background. The theme performance is terrible on Windows 10
        // with multiple high resolution displays connected.
        m_tabpanel = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
        m_tabpanel->SetFont(Slic3r::GUI::wxGetApp().normal_font());
#endif
    }
#endif // ENABLE_LAYOUT_NO_RESTART

    m_tabpanel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxEvent&) {
        wxWindow* panel = m_tabpanel->GetCurrentPage();
        Tab* tab = dynamic_cast<Tab*>(panel);

        // There shouldn't be a case, when we try to select a tab, which doesn't support a printer technology
        if (panel == nullptr || (tab != nullptr && !tab->supports_printer_technology(m_plater->printer_technology())))
            return;

        auto& tabs_list = wxGetApp().tabs_list;
        if (tab && std::find(tabs_list.begin(), tabs_list.end(), tab) != tabs_list.end()) {
            // On GTK, the wxEVT_NOTEBOOK_PAGE_CHANGED event is triggered
            // before the MainFrame is fully set up.
            tab->OnActivate();
            m_last_selected_tab = m_tabpanel->GetSelection();
        }
        else
            select_tab(0); // select Plater
    });

#if ENABLE_LAYOUT_NO_RESTART
    m_plater = new Plater(this, this);
    m_plater->Hide();
#else
    if (m_layout == slOld) {
        m_plater = new Plater(m_tabpanel, this);
        m_tabpanel->AddPage(m_plater, _L("Plater"));
    }
    else {
        m_plater = new Plater(this, this);
        if (m_layout == slNew)
            m_tabpanel->AddPage(new wxPanel(m_tabpanel), _L("Plater")); // empty panel just for Plater tab
    }
#endif // ENABLE_LAYOUT_NO_RESTART
    wxGetApp().plater_ = m_plater;

    wxGetApp().obj_list()->create_popup_menus();

    // The following event is emited by Tab implementation on config value change.
    Bind(EVT_TAB_VALUE_CHANGED, &MainFrame::on_value_changed, this); // #ys_FIXME_to_delete

    // The following event is emited by Tab on preset selection,
    // or when the preset's "modified" status changes.
    Bind(EVT_TAB_PRESETS_CHANGED, &MainFrame::on_presets_changed, this); // #ys_FIXME_to_delete

    create_preset_tabs();

    if (m_plater) {
        // load initial config
        auto full_config = wxGetApp().preset_bundle->full_config();
        m_plater->on_config_change(full_config);

        // Show a correct number of filament fields.
        // nozzle_diameter is undefined when SLA printer is selected
        if (full_config.has("nozzle_diameter")) {
            m_plater->on_extruders_change(full_config.option<ConfigOptionFloats>("nozzle_diameter")->values.size());
        }
    }
}

void MainFrame::create_preset_tabs()
{
    wxGetApp().update_label_colours_from_appconfig();
    add_created_tab(new TabPrint(m_tabpanel));
    add_created_tab(new TabFilament(m_tabpanel));
    add_created_tab(new TabSLAPrint(m_tabpanel));
    add_created_tab(new TabSLAMaterial(m_tabpanel));
    add_created_tab(new TabPrinter(m_tabpanel));
}

void MainFrame::add_created_tab(Tab* panel)
{
    panel->create_preset_tab();

    const auto printer_tech = wxGetApp().preset_bundle->printers.get_edited_preset().printer_technology();

    if (panel->supports_printer_technology(printer_tech))
        m_tabpanel->AddPage(panel, panel->title());
}

bool MainFrame::can_start_new_project() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainFrame::can_save() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainFrame::can_export_model() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainFrame::can_export_toolpaths() const
{
    return (m_plater != nullptr) && (m_plater->printer_technology() == ptFFF) && m_plater->is_preview_shown() && m_plater->is_preview_loaded() && m_plater->has_toolpaths_to_export();
}

bool MainFrame::can_export_supports() const
{
    if ((m_plater == nullptr) || (m_plater->printer_technology() != ptSLA) || m_plater->model().objects.empty())
        return false;

    bool can_export = false;
    const PrintObjects& objects = m_plater->sla_print().objects();
    for (const SLAPrintObject* object : objects)
    {
        if (object->has_mesh(slaposPad) || object->has_mesh(slaposSupportTree))
        {
            can_export = true;
            break;
        }
    }
    return can_export;
}

bool MainFrame::can_export_gcode() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    if (m_plater->is_export_gcode_scheduled())
        return false;

    // TODO:: add other filters

    return true;
}

bool MainFrame::can_send_gcode() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    const auto print_host_opt = wxGetApp().preset_bundle->printers.get_edited_preset().config.option<ConfigOptionString>("print_host");
    return print_host_opt != nullptr && !print_host_opt->value.empty();
}

bool MainFrame::can_export_gcode_sd() const
{
	if (m_plater == nullptr)
		return false;

	if (m_plater->model().objects.empty())
		return false;

	if (m_plater->is_export_gcode_scheduled())
		return false;

	// TODO:: add other filters

	return wxGetApp().removable_drive_manager()->status().has_removable_drives;
}

bool MainFrame::can_eject() const
{
	return wxGetApp().removable_drive_manager()->status().has_eject;
}

bool MainFrame::can_slice() const
{
    bool bg_proc = wxGetApp().app_config->get("background_processing") == "1";
    return (m_plater != nullptr) ? !m_plater->model().objects.empty() && !bg_proc : false;
}

bool MainFrame::can_change_view() const
{
#if ENABLE_LAYOUT_NO_RESTART
    switch (m_layout)
    {
    default:                   { return false; }
    case ESettingsLayout::New: { return m_plater->IsShown(); }
    case ESettingsLayout::Dlg: { return true; }
    case ESettingsLayout::Old: { 
        int page_id = m_tabpanel->GetSelection();
        return page_id != wxNOT_FOUND && dynamic_cast<const Slic3r::GUI::Plater*>(m_tabpanel->GetPage((size_t)page_id)) != nullptr;
    }
    }
#else
    if (m_layout == slNew)
        return m_plater->IsShown();
    if (m_layout == slDlg)
        return true;
    // slOld layout mode
    int page_id = m_tabpanel->GetSelection();
    return page_id != wxNOT_FOUND && dynamic_cast<const Slic3r::GUI::Plater*>(m_tabpanel->GetPage((size_t)page_id)) != nullptr;
#endif // ENABLE_LAYOUT_NO_RESTART
}

bool MainFrame::can_select() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainFrame::can_deselect() const
{
    return (m_plater != nullptr) && !m_plater->is_selection_empty();
}

bool MainFrame::can_delete() const
{
    return (m_plater != nullptr) && !m_plater->is_selection_empty();
}

bool MainFrame::can_delete_all() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainFrame::can_reslice() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

void MainFrame::on_dpi_changed(const wxRect& suggested_rect)
{
#if ENABLE_WX_3_1_3_DPI_CHANGED_EVENT
    wxGetApp().update_fonts(this);
#else
    wxGetApp().update_fonts();
#endif // ENABLE_WX_3_1_3_DPI_CHANGED_EVENT
    this->SetFont(this->normal_font());

    /* Load default preset bitmaps before a tabpanel initialization,
     * but after filling of an em_unit value
     */
    wxGetApp().preset_bundle->load_default_preset_bitmaps();

    // update Plater
    wxGetApp().plater()->msw_rescale();

    // update Tabs
#if ENABLE_LAYOUT_NO_RESTART
    if (m_layout != ESettingsLayout::Dlg) // Do not update tabs if the Settings are in the separated dialog
#else
    if (m_layout != slDlg) // Update tabs later, from the SettingsDialog, when the Settings are in the separated dialog
#endif // ENABLE_LAYOUT_NO_RESTART
        for (auto tab : wxGetApp().tabs_list)
            tab->msw_rescale();

    wxMenuBar* menu_bar = this->GetMenuBar();
    for (size_t id = 0; id < menu_bar->GetMenuCount(); id++)
        msw_rescale_menu(menu_bar->GetMenu(id));

    // Workarounds for correct Window rendering after rescale

    /* Even if Window is maximized during moving,
     * first of all we should imitate Window resizing. So:
     * 1. cancel maximization, if it was set
     * 2. imitate resizing
     * 3. set maximization, if it was set
     */
    const bool is_maximized = this->IsMaximized();
    if (is_maximized)
        this->Maximize(false);

    /* To correct window rendering (especially redraw of a status bar)
     * we should imitate window resizing.
     */
    const wxSize& sz = this->GetSize();
    this->SetSize(sz.x + 1, sz.y + 1);
    this->SetSize(sz);

    this->Maximize(is_maximized);

#if ENABLE_LAYOUT_NO_RESTART
    if (m_layout == ESettingsLayout::Dlg)
        rescale_dialog_after_dpi_change(*this, m_settings_dialog, ERescaleTarget::SettingsDialog);
#endif // ENABLE_LAYOUT_NO_RESTART
}

void MainFrame::on_sys_color_changed()
{
    wxBusyCursor wait;

    // update label colors in respect to the system mode
    wxGetApp().init_label_colours();

    wxGetApp().preset_bundle->load_default_preset_bitmaps();

    // update Plater
    wxGetApp().plater()->sys_color_changed();

    // update Tabs
    for (auto tab : wxGetApp().tabs_list)
        tab->sys_color_changed();

    // msw_rescale_menu updates just icons, so use it
    wxMenuBar* menu_bar = this->GetMenuBar();
    for (size_t id = 0; id < menu_bar->GetMenuCount(); id++)
        msw_rescale_menu(menu_bar->GetMenu(id));
}

void MainFrame::init_menubar()
{
#ifdef __APPLE__
    wxMenuBar::SetAutoWindowMenu(false);
#endif

    // File menu
    wxMenu* fileMenu = new wxMenu;
    {
        append_menu_item(fileMenu, wxID_ANY, _(L("&New Project")) + "\tCtrl+N", _(L("Start a new project")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->new_project(); }, "", nullptr,
            [this](){return m_plater != nullptr && can_start_new_project(); }, this);
        append_menu_item(fileMenu, wxID_ANY, _(L("&Open Project")) + dots + "\tCtrl+O", _(L("Open a project file")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->load_project(); }, "open", nullptr,
            [this](){return m_plater != nullptr; }, this);

        wxMenu* recent_projects_menu = new wxMenu();
        wxMenuItem* recent_projects_submenu = append_submenu(fileMenu, recent_projects_menu, wxID_ANY, _(L("Recent projects")), "");
        m_recent_projects.UseMenu(recent_projects_menu);
        Bind(wxEVT_MENU, [this](wxCommandEvent& evt) {
            size_t file_id = evt.GetId() - wxID_FILE1;
            wxString filename = m_recent_projects.GetHistoryFile(file_id);
            if (wxFileExists(filename))
                m_plater->load_project(filename);
            else
            {
                wxMessageDialog msg(this, _(L("The selected project is no longer available.\nDo you want to remove it from the recent projects list?")), _(L("Error")), wxYES_NO | wxYES_DEFAULT);
                if (msg.ShowModal() == wxID_YES)
                {
                    m_recent_projects.RemoveFileFromHistory(file_id);
                        std::vector<std::string> recent_projects;
                        size_t count = m_recent_projects.GetCount();
                        for (size_t i = 0; i < count; ++i)
                        {
                            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
                        }
                    wxGetApp().app_config->set_recent_projects(recent_projects);
                    wxGetApp().app_config->save();
                }
            }
            }, wxID_FILE1, wxID_FILE9);

        std::vector<std::string> recent_projects = wxGetApp().app_config->get_recent_projects();
        std::reverse(recent_projects.begin(), recent_projects.end());
        for (const std::string& project : recent_projects)
        {
            m_recent_projects.AddFileToHistory(from_u8(project));
        }

        Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& evt) { evt.Enable(m_recent_projects.GetCount() > 0); }, recent_projects_submenu->GetId());

        append_menu_item(fileMenu, wxID_ANY, _(L("&Save Project")) + "\tCtrl+S", _(L("Save current project file")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_3mf(into_path(m_plater->get_project_filename(".3mf"))); }, "save", nullptr,
            [this](){return m_plater != nullptr && can_save(); }, this);
#ifdef __APPLE__
        append_menu_item(fileMenu, wxID_ANY, _(L("Save Project &as")) + dots + "\tCtrl+Shift+S", _(L("Save current project file as")),
#else
        append_menu_item(fileMenu, wxID_ANY, _(L("Save Project &as")) + dots + "\tCtrl+Alt+S", _(L("Save current project file as")),
#endif // __APPLE__
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_3mf(); }, "save", nullptr,
            [this](){return m_plater != nullptr && can_save(); }, this);

        fileMenu->AppendSeparator();

        wxMenu* import_menu = new wxMenu();
        append_menu_item(import_menu, wxID_ANY, _(L("Import STL/OBJ/AM&F/3MF")) + dots + "\tCtrl+I", _(L("Load a model")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->add_model(); }, "import_plater", nullptr,
            [this](){return m_plater != nullptr; }, this);
        
        append_menu_item(import_menu, wxID_ANY, _L("Import STL (imperial units)"), _L("Load an model saved with imperial units"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->add_model(true); }, "import_plater", nullptr,
            [this](){return m_plater != nullptr; }, this);
        
        append_menu_item(import_menu, wxID_ANY, _(L("Import SL1 archive")) + dots, _(L("Load an SL1 output archive")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->import_sl1_archive(); }, "import_plater", nullptr,
            [this](){return m_plater != nullptr; }, this);    
    
        import_menu->AppendSeparator();
        append_menu_item(import_menu, wxID_ANY, _(L("Import &Config")) + dots + "\tCtrl+L", _(L("Load exported configuration file")),
            [this](wxCommandEvent&) { load_config_file(); }, "import_config", nullptr,
            [this]() {return true; }, this);
        append_menu_item(import_menu, wxID_ANY, _(L("Import Config from &project")) + dots +"\tCtrl+Alt+L", _(L("Load configuration from project file")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->extract_config_from_project(); }, "import_config", nullptr,
            [this]() {return true; }, this);
        import_menu->AppendSeparator();
        append_menu_item(import_menu, wxID_ANY, _(L("Import Config &Bundle")) + dots, _(L("Load presets from a bundle")),
            [this](wxCommandEvent&) { load_configbundle(); }, "import_config_bundle", nullptr,
            [this]() {return true; }, this);
        append_submenu(fileMenu, import_menu, wxID_ANY, _(L("&Import")), "");

        wxMenu* export_menu = new wxMenu();
        wxMenuItem* item_export_gcode = append_menu_item(export_menu, wxID_ANY, _(L("Export &G-code")) + dots +"\tCtrl+G", _(L("Export current plate as G-code")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_gcode(); }, "export_gcode", nullptr,
            [this](){return can_export_gcode(); }, this);
        m_changeable_menu_items.push_back(item_export_gcode);
        wxMenuItem* item_send_gcode = append_menu_item(export_menu, wxID_ANY, _(L("S&end G-code")) + dots +"\tCtrl+Shift+G", _(L("Send to print current plate as G-code")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->send_gcode(); }, "export_gcode", nullptr,
            [this](){return can_send_gcode(); }, this);
        m_changeable_menu_items.push_back(item_send_gcode);
		append_menu_item(export_menu, wxID_ANY, _(L("Export G-code to SD card / Flash drive")) + dots + "\tCtrl+U", _(L("Export current plate as G-code to SD card / Flash drive")),
			[this](wxCommandEvent&) { if (m_plater) m_plater->export_gcode(true); }, "export_to_sd", nullptr,
			[this]() {return can_export_gcode_sd(); }, this);
        export_menu->AppendSeparator();
        append_menu_item(export_menu, wxID_ANY, _(L("Export plate as &STL")) + dots, _(L("Export current plate as STL")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_stl(); }, "export_plater", nullptr,
            [this](){return can_export_model(); }, this);
        append_menu_item(export_menu, wxID_ANY, _(L("Export plate as STL &including supports")) + dots, _(L("Export current plate as STL including supports")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_stl(true); }, "export_plater", nullptr,
            [this](){return can_export_supports(); }, this);
        append_menu_item(export_menu, wxID_ANY, _(L("Export plate as &AMF")) + dots, _(L("Export current plate as AMF")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_amf(); }, "export_plater", nullptr,
            [this](){return can_export_model(); }, this);
        export_menu->AppendSeparator();
        append_menu_item(export_menu, wxID_ANY, _(L("Export &toolpaths as OBJ")) + dots, _(L("Export toolpaths as OBJ")),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_toolpaths_to_obj(); }, "export_plater", nullptr,
            [this]() {return can_export_toolpaths(); }, this);
        export_menu->AppendSeparator();
        append_menu_item(export_menu, wxID_ANY, _(L("Export &Config")) +dots +"\tCtrl+E", _(L("Export current configuration to file")),
            [this](wxCommandEvent&) { export_config(); }, "export_config", nullptr,
            [this]() {return true; }, this);
        append_menu_item(export_menu, wxID_ANY, _(L("Export Config &Bundle")) + dots, _(L("Export all presets to file")),
            [this](wxCommandEvent&) { export_configbundle(); }, "export_config_bundle", nullptr,
            [this]() {return true; }, this);
        append_submenu(fileMenu, export_menu, wxID_ANY, _(L("&Export")), "");

		append_menu_item(fileMenu, wxID_ANY, _(L("Ejec&t SD card / Flash drive")) + dots + "\tCtrl+T", _(L("Eject SD card / Flash drive after the G-code was exported to it.")),
			[this](wxCommandEvent&) { if (m_plater) m_plater->eject_drive(); }, "eject_sd", nullptr,
			[this]() {return can_eject(); }, this);

        fileMenu->AppendSeparator();

#if 0
        m_menu_item_repeat = nullptr;
        append_menu_item(fileMenu, wxID_ANY, _(L("Quick Slice")) +dots+ "\tCtrl+U", _(L("Slice a file into a G-code")),
            [this](wxCommandEvent&) {
                wxTheApp->CallAfter([this]() {
                    quick_slice();
                    m_menu_item_repeat->Enable(is_last_input_file());
                }); }, "cog_go.png");
        append_menu_item(fileMenu, wxID_ANY, _(L("Quick Slice and Save As")) +dots +"\tCtrl+Alt+U", _(L("Slice a file into a G-code, save as")),
            [this](wxCommandEvent&) {
            wxTheApp->CallAfter([this]() {
                    quick_slice(qsSaveAs);
                    m_menu_item_repeat->Enable(is_last_input_file());
                }); }, "cog_go.png");
        m_menu_item_repeat = append_menu_item(fileMenu, wxID_ANY, _(L("Repeat Last Quick Slice")) +"\tCtrl+Shift+U", _(L("Repeat last quick slice")),
            [this](wxCommandEvent&) {
            wxTheApp->CallAfter([this]() {
                quick_slice(qsReslice);
            }); }, "cog_go.png");
        m_menu_item_repeat->Enable(false);
        fileMenu->AppendSeparator();
#endif
        m_menu_item_reslice_now = append_menu_item(fileMenu, wxID_ANY, _(L("(Re)Slice No&w")) + "\tCtrl+R", _(L("Start new slicing process")),
            [this](wxCommandEvent&) { reslice_now(); }, "re_slice", nullptr,
            [this](){return m_plater != nullptr && can_reslice(); }, this);
        fileMenu->AppendSeparator();
        append_menu_item(fileMenu, wxID_ANY, _(L("&Repair STL file")) + dots, _(L("Automatically repair an STL file")),
            [this](wxCommandEvent&) { repair_stl(); }, "wrench", nullptr,
            [this]() {return true; }, this);
        fileMenu->AppendSeparator();
        append_menu_item(fileMenu, wxID_EXIT, _(L("&Quit")), wxString::Format(_(L("Quit %s")), SLIC3R_APP_NAME),
            [this](wxCommandEvent&) { Close(false); });
    }

#ifdef _MSC_VER
    // \xA0 is a non-breaking space. It is entered here to spoil the automatic accelerators,
    // as the simple numeric accelerators spoil all numeric data entry.
    wxString sep = "\t\xA0";
    wxString sep_space = "\xA0";
#else
    wxString sep = " - ";
    wxString sep_space = "";
#endif

    // Edit menu
    wxMenu* editMenu = nullptr;
    if (m_plater != nullptr)
    {
        editMenu = new wxMenu();
    #ifdef __APPLE__
        // Backspace sign
        wxString hotkey_delete = "\u232b";
    #else
        wxString hotkey_delete = "Del";
    #endif
        append_menu_item(editMenu, wxID_ANY, _(L("&Select all")) + sep + GUI::shortkey_ctrl_prefix() + sep_space + "A",
            _(L("Selects all objects")), [this](wxCommandEvent&) { m_plater->select_all(); },
            "", nullptr, [this](){return can_select(); }, this);
        append_menu_item(editMenu, wxID_ANY, _(L("D&eselect all")) + sep + "Esc",
            _(L("Deselects all objects")), [this](wxCommandEvent&) { m_plater->deselect_all(); },
            "", nullptr, [this](){return can_deselect(); }, this);
        editMenu->AppendSeparator();
        append_menu_item(editMenu, wxID_ANY, _(L("&Delete selected")) + sep + hotkey_delete,
            _(L("Deletes the current selection")),[this](wxCommandEvent&) { m_plater->remove_selected(); },
            "remove_menu", nullptr, [this](){return can_delete(); }, this);
        append_menu_item(editMenu, wxID_ANY, _(L("Delete &all")) + sep + GUI::shortkey_ctrl_prefix() + sep_space + hotkey_delete,
            _(L("Deletes all objects")), [this](wxCommandEvent&) { m_plater->reset_with_confirm(); },
            "delete_all_menu", nullptr, [this](){return can_delete_all(); }, this);

        editMenu->AppendSeparator();
        append_menu_item(editMenu, wxID_ANY, _(L("&Undo")) + sep + GUI::shortkey_ctrl_prefix() + sep_space + "Z",
            _(L("Undo")), [this](wxCommandEvent&) { m_plater->undo(); },
            "undo_menu", nullptr, [this](){return m_plater->can_undo(); }, this);
        append_menu_item(editMenu, wxID_ANY, _(L("&Redo")) + sep + GUI::shortkey_ctrl_prefix() + sep_space + "Y",
            _(L("Redo")), [this](wxCommandEvent&) { m_plater->redo(); },
            "redo_menu", nullptr, [this](){return m_plater->can_redo(); }, this);

        editMenu->AppendSeparator();
        append_menu_item(editMenu, wxID_ANY, _(L("&Copy")) + sep + GUI::shortkey_ctrl_prefix() + sep_space + "C",
            _(L("Copy selection to clipboard")), [this](wxCommandEvent&) { m_plater->copy_selection_to_clipboard(); },
            "copy_menu", nullptr, [this](){return m_plater->can_copy_to_clipboard(); }, this);
        append_menu_item(editMenu, wxID_ANY, _(L("&Paste")) + sep + GUI::shortkey_ctrl_prefix() + sep_space + "V",
            _(L("Paste clipboard")), [this](wxCommandEvent&) { m_plater->paste_from_clipboard(); },
            "paste_menu", nullptr, [this](){return m_plater->can_paste_from_clipboard(); }, this);
        
        editMenu->AppendSeparator();
        append_menu_item(editMenu, wxID_ANY, _(L("Re&load from disk")) + sep + "F5",
            _(L("Reload the plater from disk")), [this](wxCommandEvent&) { m_plater->reload_all_from_disk(); },
            "", nullptr, [this]() {return !m_plater->model().objects.empty(); }, this);

        editMenu->AppendSeparator();
        append_menu_item(editMenu, wxID_ANY, _(L("Searc&h")) + "\tCtrl+F",
            _(L("Find option")), [this](wxCommandEvent&) { m_plater->search(/*m_tabpanel->GetCurrentPage() == */m_plater->IsShown()); },
            "search", nullptr, [this]() {return true; }, this);
    }

    // Window menu
    auto windowMenu = new wxMenu();
    {
        if (m_plater) {
            append_menu_item(windowMenu, wxID_HIGHEST + 1, _(L("&Plater Tab")) + "\tCtrl+1", _(L("Show the plater")),
                [this](wxCommandEvent&) { select_tab(0); }, "plater", nullptr,
                [this]() {return true; }, this);
            windowMenu->AppendSeparator();
        }
        append_menu_item(windowMenu, wxID_HIGHEST + 2, _(L("P&rint Settings Tab")) + "\tCtrl+2", _(L("Show the print settings")),
            [this/*, tab_offset*/](wxCommandEvent&) { select_tab(1); }, "cog", nullptr,
            [this]() {return true; }, this);
        wxMenuItem* item_material_tab = append_menu_item(windowMenu, wxID_HIGHEST + 3, _(L("&Filament Settings Tab")) + "\tCtrl+3", _(L("Show the filament settings")),
            [this/*, tab_offset*/](wxCommandEvent&) { select_tab(2); }, "spool", nullptr,
            [this]() {return true; }, this);
        m_changeable_menu_items.push_back(item_material_tab);
        wxMenuItem* item_printer_tab = append_menu_item(windowMenu, wxID_HIGHEST + 4, _(L("Print&er Settings Tab")) + "\tCtrl+4", _(L("Show the printer settings")),
            [this/*, tab_offset*/](wxCommandEvent&) { select_tab(3); }, "printer", nullptr,
            [this]() {return true; }, this);
        m_changeable_menu_items.push_back(item_printer_tab);
        if (m_plater) {
            windowMenu->AppendSeparator();
            append_menu_item(windowMenu, wxID_HIGHEST + 5, _(L("3&D")) + "\tCtrl+5", _(L("Show the 3D editing view")),
                [this](wxCommandEvent&) { m_plater->select_view_3D("3D"); }, "editor_menu", nullptr,
                [this](){return can_change_view(); }, this);
            append_menu_item(windowMenu, wxID_HIGHEST + 6, _(L("Pre&view")) + "\tCtrl+6", _(L("Show the 3D slices preview")),
                [this](wxCommandEvent&) { m_plater->select_view_3D("Preview"); }, "preview_menu", nullptr,
                [this](){return can_change_view(); }, this);
        }

#if _WIN32
        // This is needed on Windows to fake the CTRL+# of the window menu when using the numpad
        wxAcceleratorEntry entries[6];
        entries[0].Set(wxACCEL_CTRL, WXK_NUMPAD1, wxID_HIGHEST + 1);
        entries[1].Set(wxACCEL_CTRL, WXK_NUMPAD2, wxID_HIGHEST + 2);
        entries[2].Set(wxACCEL_CTRL, WXK_NUMPAD3, wxID_HIGHEST + 3);
        entries[3].Set(wxACCEL_CTRL, WXK_NUMPAD4, wxID_HIGHEST + 4);
        entries[4].Set(wxACCEL_CTRL, WXK_NUMPAD5, wxID_HIGHEST + 5);
        entries[5].Set(wxACCEL_CTRL, WXK_NUMPAD6, wxID_HIGHEST + 6);
        wxAcceleratorTable accel(6, entries);
        SetAcceleratorTable(accel);
#endif // _WIN32

        windowMenu->AppendSeparator();
        append_menu_item(windowMenu, wxID_ANY, _(L("Print &Host Upload Queue")) + "\tCtrl+J", _(L("Display the Print Host Upload Queue window")),
            [this](wxCommandEvent&) { m_printhost_queue_dlg->Show(); }, "upload_queue", nullptr,
            [this]() {return true; }, this);
    }

    // View menu
    wxMenu* viewMenu = nullptr;
    if (m_plater) {
        viewMenu = new wxMenu();
        // The camera control accelerators are captured by GLCanvas3D::on_char().
        append_menu_item(viewMenu, wxID_ANY, _(L("Iso")) + sep + "&0", _(L("Iso View")),[this](wxCommandEvent&) { select_view("iso"); }, 
            "", nullptr, [this](){return can_change_view(); }, this);
        viewMenu->AppendSeparator();
        //TRN To be shown in the main menu View->Top 
        append_menu_item(viewMenu, wxID_ANY, _(L("Top")) + sep + "&1", _(L("Top View")), [this](wxCommandEvent&) { select_view("top"); },
            "", nullptr, [this](){return can_change_view(); }, this);
		//TRN To be shown in the main menu View->Bottom 
        append_menu_item(viewMenu, wxID_ANY, _(L("Bottom")) + sep + "&2", _(L("Bottom View")), [this](wxCommandEvent&) { select_view("bottom"); },
            "", nullptr, [this](){return can_change_view(); }, this);
        append_menu_item(viewMenu, wxID_ANY, _(L("Front")) + sep + "&3", _(L("Front View")), [this](wxCommandEvent&) { select_view("front"); },
            "", nullptr, [this](){return can_change_view(); }, this);
        append_menu_item(viewMenu, wxID_ANY, _(L("Rear")) + sep + "&4", _(L("Rear View")), [this](wxCommandEvent&) { select_view("rear"); },
            "", nullptr, [this](){return can_change_view(); }, this);
        append_menu_item(viewMenu, wxID_ANY, _(L("Left")) + sep + "&5", _(L("Left View")), [this](wxCommandEvent&) { select_view("left"); },
            "", nullptr, [this](){return can_change_view(); }, this);
        append_menu_item(viewMenu, wxID_ANY, _(L("Right")) + sep + "&6", _(L("Right View")), [this](wxCommandEvent&) { select_view("right"); },
            "", nullptr, [this](){return can_change_view(); }, this);
        viewMenu->AppendSeparator();
#if ENABLE_SLOPE_RENDERING
        wxMenu* options_menu = new wxMenu();
        append_menu_check_item(options_menu, wxID_ANY, _(L("Show &labels")) + sep + "E", _(L("Show object/instance labels in 3D scene")),
            [this](wxCommandEvent&) { m_plater->show_view3D_labels(!m_plater->are_view3D_labels_shown()); }, this,
            [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->are_view3D_labels_shown(); }, this);
        append_menu_check_item(options_menu, wxID_ANY, _(L("Show &slope")) + sep + "D", _(L("Objects coloring using faces' slope")),
            [this](wxCommandEvent&) { m_plater->show_view3D_slope(!m_plater->is_view3D_slope_shown()); }, this,
            [this]() { return m_plater->is_view3D_shown() && !m_plater->is_view3D_layers_editing_enabled(); }, [this]() { return m_plater->is_view3D_slope_shown(); }, this);
        append_submenu(viewMenu, options_menu, wxID_ANY, _(L("&Options")), "");
#else
        append_menu_check_item(viewMenu, wxID_ANY, _(L("Show &labels")) + sep + "E", _(L("Show object/instance labels in 3D scene")),
            [this](wxCommandEvent&) { m_plater->show_view3D_labels(!m_plater->are_view3D_labels_shown()); }, this,
            [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->are_view3D_labels_shown(); }, this);
#endif // ENABLE_SLOPE_RENDERING
        append_menu_check_item(viewMenu, wxID_ANY, _(L("&Collapse sidebar")), _(L("Collapse sidebar")),
            [this](wxCommandEvent&) { m_plater->collapse_sidebar(!m_plater->is_sidebar_collapsed()); }, this,
            [this]() { return true; }, [this]() { return m_plater->is_sidebar_collapsed(); }, this);
    }

    // Help menu
    auto helpMenu = new wxMenu();
    {
        append_menu_item(helpMenu, wxID_ANY, _(L("Prusa 3D &Drivers")), _(L("Open the Prusa3D drivers download page in your browser")), 
            [this](wxCommandEvent&) { wxGetApp().open_web_page_localized("https://www.prusa3d.com/downloads"); }); 
        append_menu_item(helpMenu, wxID_ANY, _(L("Software &Releases")), _(L("Open the software releases page in your browser")), 
            [this](wxCommandEvent&) { wxLaunchDefaultBrowser("http://github.com/prusa3d/PrusaSlicer/releases"); });
//#        my $versioncheck = $self->_append_menu_item($helpMenu, "Check for &Updates...", "Check for new Slic3r versions", sub{
//#            wxTheApp->check_version(1);
//#        });
//#        $versioncheck->Enable(wxTheApp->have_version_check);
        append_menu_item(helpMenu, wxID_ANY, wxString::Format(_(L("%s &Website")), SLIC3R_APP_NAME), 
                                             wxString::Format(_(L("Open the %s website in your browser")), SLIC3R_APP_NAME),
            [this](wxCommandEvent&) { wxGetApp().open_web_page_localized("https://www.prusa3d.com/slicerweb"); });
//        append_menu_item(helpMenu, wxID_ANY, wxString::Format(_(L("%s &Manual")), SLIC3R_APP_NAME),
//                                             wxString::Format(_(L("Open the %s manual in your browser")), SLIC3R_APP_NAME),
//            [this](wxCommandEvent&) { wxLaunchDefaultBrowser("http://manual.slic3r.org/"); });
        helpMenu->AppendSeparator();
        append_menu_item(helpMenu, wxID_ANY, _(L("System &Info")), _(L("Show system information")), 
            [this](wxCommandEvent&) { wxGetApp().system_info(); });
        append_menu_item(helpMenu, wxID_ANY, _(L("Show &Configuration Folder")), _(L("Show user configuration folder (datadir)")),
            [this](wxCommandEvent&) { Slic3r::GUI::desktop_open_datadir_folder(); });
        append_menu_item(helpMenu, wxID_ANY, _(L("Report an I&ssue")), wxString::Format(_(L("Report an issue on %s")), SLIC3R_APP_NAME), 
            [this](wxCommandEvent&) { wxLaunchDefaultBrowser("http://github.com/prusa3d/slic3r/issues/new"); });
        append_menu_item(helpMenu, wxID_ANY, wxString::Format(_(L("&About %s")), SLIC3R_APP_NAME), _(L("Show about dialog")),
            [this](wxCommandEvent&) { Slic3r::GUI::about(); });
        helpMenu->AppendSeparator();
        append_menu_item(helpMenu, wxID_ANY, _(L("Keyboard Shortcuts")) + sep + "&?", _(L("Show the list of the keyboard shortcuts")),
            [this](wxCommandEvent&) { wxGetApp().keyboard_shortcuts(); });
#if ENABLE_THUMBNAIL_GENERATOR_DEBUG
        helpMenu->AppendSeparator();
        append_menu_item(helpMenu, wxID_ANY, "DEBUG gcode thumbnails", "DEBUG ONLY - read the selected gcode file and generates png for the contained thumbnails",
            [this](wxCommandEvent&) { wxGetApp().gcode_thumbnails_debug(); });
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG
    }

    // menubar
    // assign menubar to frame after appending items, otherwise special items
    // will not be handled correctly
    auto menubar = new wxMenuBar();
    menubar->Append(fileMenu, _(L("&File")));
    if (editMenu) menubar->Append(editMenu, _(L("&Edit")));
    menubar->Append(windowMenu, _(L("&Window")));
    if (viewMenu) menubar->Append(viewMenu, _(L("&View")));
    // Add additional menus from C++
    wxGetApp().add_config_menu(menubar);
    menubar->Append(helpMenu, _(L("&Help")));
    SetMenuBar(menubar);

#ifdef __APPLE__
    // This fixes a bug on Mac OS where the quit command doesn't emit window close events
    // wx bug: https://trac.wxwidgets.org/ticket/18328
    wxMenu *apple_menu = menubar->OSXGetAppleMenu();
    if (apple_menu != nullptr) {
        apple_menu->Bind(wxEVT_MENU, [this](wxCommandEvent &) {
            Close();
        }, wxID_EXIT);
    }
#endif

    if (plater()->printer_technology() == ptSLA)
        update_menubar();
}

void MainFrame::update_menubar()
{
    const bool is_fff = plater()->printer_technology() == ptFFF;

    m_changeable_menu_items[miExport]       ->SetItemLabel((is_fff ? _(L("Export &G-code"))         : _(L("E&xport"))       ) + dots     + "\tCtrl+G");
    m_changeable_menu_items[miSend]         ->SetItemLabel((is_fff ? _(L("S&end G-code"))           : _(L("S&end to print"))) + dots    + "\tCtrl+Shift+G");

    m_changeable_menu_items[miMaterialTab]  ->SetItemLabel((is_fff ? _(L("&Filament Settings Tab")) : _(L("Mate&rial Settings Tab")))   + "\tCtrl+3");
    m_changeable_menu_items[miMaterialTab]  ->SetBitmap(create_scaled_bitmap(is_fff ? "spool"   : "resin"));

    m_changeable_menu_items[miPrinterTab]   ->SetBitmap(create_scaled_bitmap(is_fff ? "printer" : "sla_printer"));
}

// To perform the "Quck Slice", "Quick Slice and Save As", "Repeat last Quick Slice" and "Slice to SVG".
void MainFrame::quick_slice(const int qs)
{
//     my $progress_dialog;
    wxString input_file;
//  eval
//     {
    // validate configuration
    auto config = wxGetApp().preset_bundle->full_config();
    auto valid = config.validate();
    if (! valid.empty()) {
        show_error(this, valid);
        return;
    }

    // select input file
    if (!(qs & qsReslice)) {
        wxFileDialog dlg(this, _(L("Choose a file to slice (STL/OBJ/AMF/3MF/PRUSA):")),
            wxGetApp().app_config->get_last_dir(), "",
            file_wildcards(FT_MODEL), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;
        input_file = dlg.GetPath();
        if (!(qs & qsExportSVG))
            m_qs_last_input_file = input_file;
    }
    else {
        if (m_qs_last_input_file.IsEmpty()) {
            wxMessageDialog dlg(this, _(L("No previously sliced file.")),
                _(L("Error")), wxICON_ERROR | wxOK);
            dlg.ShowModal();
            return;
        }
        if (std::ifstream(m_qs_last_input_file.ToUTF8().data())) {
            wxMessageDialog dlg(this, _(L("Previously sliced file ("))+m_qs_last_input_file+_(L(") not found.")),
                _(L("File Not Found")), wxICON_ERROR | wxOK);
            dlg.ShowModal();
            return;
        }
        input_file = m_qs_last_input_file;
    }
    auto input_file_basename = get_base_name(input_file);
    wxGetApp().app_config->update_skein_dir(get_dir_name(input_file));

    auto bed_shape = Slic3r::Polygon::new_scale(config.option<ConfigOptionPoints>("bed_shape")->values);
//     auto print_center = Slic3r::Pointf->new_unscale(bed_shape.bounding_box().center());
// 
//     auto sprint = new Slic3r::Print::Simple(
//         print_center = > print_center,
//         status_cb = > [](int percent, const wxString& msg) {
//         m_progress_dialog->Update(percent, msg+"…");
//     });

    // keep model around
    auto model = Slic3r::Model::read_from_file(input_file.ToUTF8().data());

//     sprint->apply_config(config);
//     sprint->set_model(model);

    // Copy the names of active presets into the placeholder parser.
//     wxGetApp().preset_bundle->export_selections(sprint->placeholder_parser);

    // select output file
    wxString output_file;
    if (qs & qsReslice) {
        if (!m_qs_last_output_file.IsEmpty())
            output_file = m_qs_last_output_file;
    } 
    else if (qs & qsSaveAs) {
        // The following line may die if the output_filename_format template substitution fails.
        wxFileDialog dlg(this, from_u8((boost::format(_utf8(L("Save %s file as:"))) % ((qs & qsExportSVG) ? _(L("SVG")) : _(L("G-code")))).str()),
            wxGetApp().app_config->get_last_output_dir(get_dir_name(output_file)), get_base_name(input_file), 
            qs & qsExportSVG ? file_wildcards(FT_SVG) : file_wildcards(FT_GCODE),
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return;
        output_file = dlg.GetPath();
        if (!(qs & qsExportSVG))
            m_qs_last_output_file = output_file;
        wxGetApp().app_config->update_last_output_dir(get_dir_name(output_file));
    } 
    else if (qs & qsExportPNG) {
        wxFileDialog dlg(this, _(L("Save zip file as:")),
            wxGetApp().app_config->get_last_output_dir(get_dir_name(output_file)),
            get_base_name(output_file), "*.sl1", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return;
        output_file = dlg.GetPath();
    }

    // show processbar dialog
    m_progress_dialog = new wxProgressDialog(_(L("Slicing")) + dots, 
    // TRN "Processing input_file_basename"
                                             from_u8((boost::format(_utf8(L("Processing %s"))) % (input_file_basename + dots)).str()),
        100, this, 4);
    m_progress_dialog->Pulse();
    {
//         my @warnings = ();
//         local $SIG{ __WARN__ } = sub{ push @warnings, $_[0] };

//         sprint->output_file(output_file);
//         if (export_svg) {
//             sprint->export_svg();
//         }
//         else if(export_png) {
//             sprint->export_png();
//         }
//         else {
//             sprint->export_gcode();
//         }
//         sprint->status_cb(undef);
//         Slic3r::GUI::warning_catcher($self)->($_) for @warnings;
    }
    m_progress_dialog->Destroy();
    m_progress_dialog = nullptr;

    auto message = input_file_basename + _(L(" was successfully sliced."));
//     wxTheApp->notify(message);
    wxMessageDialog(this, message, _(L("Slicing Done!")), wxOK | wxICON_INFORMATION).ShowModal();
//     };
//     Slic3r::GUI::catch_error(this, []() { if (m_progress_dialog) m_progress_dialog->Destroy(); });
}

void MainFrame::reslice_now()
{
    if (m_plater)
        m_plater->reslice();
}

void MainFrame::repair_stl()
{
    wxString input_file;
    {
        wxFileDialog dlg(this, _(L("Select the STL file to repair:")),
            wxGetApp().app_config->get_last_dir(), "",
            file_wildcards(FT_STL), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;
        input_file = dlg.GetPath();
    }

    wxString output_file = input_file;
    {
        wxFileDialog dlg( this, L("Save OBJ file (less prone to coordinate errors than STL) as:"),
                                        get_dir_name(output_file), get_base_name(output_file, ".obj"),
                                        file_wildcards(FT_OBJ), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return;
        output_file = dlg.GetPath();
    }

    auto tmesh = new Slic3r::TriangleMesh();
    tmesh->ReadSTLFile(input_file.ToUTF8().data());
    tmesh->repair();
    tmesh->WriteOBJFile(output_file.ToUTF8().data());
    Slic3r::GUI::show_info(this, L("Your file was repaired."), L("Repair"));
}

void MainFrame::export_config()
{
    // Generate a cummulative configuration for the selected print, filaments and printer.
    auto config = wxGetApp().preset_bundle->full_config();
    // Validate the cummulative configuration.
    auto valid = config.validate();
    if (! valid.empty()) {
        show_error(this, valid);
        return;
    }
    // Ask user for the file name for the config file.
    wxFileDialog dlg(this, _(L("Save configuration as:")),
        !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(),
        !m_last_config.IsEmpty() ? get_base_name(m_last_config) : "config.ini",
        file_wildcards(FT_INI), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    wxString file;
    if (dlg.ShowModal() == wxID_OK)
        file = dlg.GetPath();
    if (!file.IsEmpty()) {
        wxGetApp().app_config->update_config_dir(get_dir_name(file));
        m_last_config = file;
        config.save(file.ToUTF8().data());
    }
}

// Load a config file containing a Print, Filament & Printer preset.
void MainFrame::load_config_file()
{
    if (!wxGetApp().check_unsaved_changes())
        return;
    wxFileDialog dlg(this, _(L("Select configuration to load:")),
        !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(),
        "config.ini", "INI files (*.ini, *.gcode)|*.ini;*.INI;*.gcode;*.g", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	wxString file;
    if (dlg.ShowModal() == wxID_OK)
        file = dlg.GetPath();
    if (! file.IsEmpty() && this->load_config_file(file.ToUTF8().data())) {
        wxGetApp().app_config->update_config_dir(get_dir_name(file));
        m_last_config = file;
    }
}

// Load a config file containing a Print, Filament & Printer preset from command line.
bool MainFrame::load_config_file(const std::string &path)
{
    try {
        wxGetApp().preset_bundle->load_config_file(path); 
    } catch (const std::exception &ex) {
        show_error(this, ex.what());
        return false;
    }
    wxGetApp().load_current_presets();
    return true;
}

void MainFrame::export_configbundle()
{
    if (!wxGetApp().check_unsaved_changes())
        return;
    // validate current configuration in case it's dirty
    auto err = wxGetApp().preset_bundle->full_config().validate();
    if (! err.empty()) {
        show_error(this, err);
        return;
    }
    // Ask user for a file name.
    wxFileDialog dlg(this, _(L("Save presets bundle as:")),
        !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(),
        SLIC3R_APP_KEY "_config_bundle.ini",
        file_wildcards(FT_INI), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    wxString file;
    if (dlg.ShowModal() == wxID_OK)
        file = dlg.GetPath();
    if (!file.IsEmpty()) {
        // Export the config bundle.
        wxGetApp().app_config->update_config_dir(get_dir_name(file));
        try {
            wxGetApp().preset_bundle->export_configbundle(file.ToUTF8().data()); 
        } catch (const std::exception &ex) {
			show_error(this, ex.what());
        }
    }
}

// Loading a config bundle with an external file name used to be used
// to auto - install a config bundle on a fresh user account,
// but that behavior was not documented and likely buggy.
void MainFrame::load_configbundle(wxString file/* = wxEmptyString, const bool reset_user_profile*/)
{
    if (!wxGetApp().check_unsaved_changes())
        return;
    if (file.IsEmpty()) {
        wxFileDialog dlg(this, _(L("Select configuration to load:")),
            !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(),
            "config.ini", file_wildcards(FT_INI), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;
        file = dlg.GetPath();
	}

    wxGetApp().app_config->update_config_dir(get_dir_name(file));

    auto presets_imported = 0;
    try {
        presets_imported = wxGetApp().preset_bundle->load_configbundle(file.ToUTF8().data());
    } catch (const std::exception &ex) {
        show_error(this, ex.what());
        return;
    }

    // Load the currently selected preset into the GUI, update the preset selection box.
	wxGetApp().load_current_presets();

    const auto message = wxString::Format(_(L("%d presets successfully imported.")), presets_imported);
    Slic3r::GUI::show_info(this, message, wxString("Info"));
}

// Load a provied DynamicConfig into the Print / Filament / Printer tabs, thus modifying the active preset.
// Also update the plater with the new presets.
void MainFrame::load_config(const DynamicPrintConfig& config)
{
	PrinterTechnology printer_technology = wxGetApp().preset_bundle->printers.get_edited_preset().printer_technology();
	const auto       *opt_printer_technology = config.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology");
	if (opt_printer_technology != nullptr && opt_printer_technology->value != printer_technology) {
		printer_technology = opt_printer_technology->value;
		this->plater()->set_printer_technology(printer_technology);
	}
#if 0
	for (auto tab : wxGetApp().tabs_list)
		if (tab->supports_printer_technology(printer_technology)) {
			if (tab->type() == Slic3r::Preset::TYPE_PRINTER)
				static_cast<TabPrinter*>(tab)->update_pages();
			tab->load_config(config);
		}
    if (m_plater)
        m_plater->on_config_change(config);
#else
	// Load the currently selected preset into the GUI, update the preset selection box.
    //FIXME this is not quite safe for multi-extruder printers,
    // as the number of extruders is not adjusted for the vector values.
    // (see PresetBundle::update_multi_material_filament_presets())
    // Better to call PresetBundle::load_config() instead?
    for (auto tab : wxGetApp().tabs_list)
        if (tab->supports_printer_technology(printer_technology)) {
            // Only apply keys, which are present in the tab's config. Ignore the other keys.
			for (const std::string &opt_key : tab->get_config()->diff(config))
				// Ignore print_settings_id, printer_settings_id, filament_settings_id etc.
				if (! boost::algorithm::ends_with(opt_key, "_settings_id"))
					tab->get_config()->option(opt_key)->set(config.option(opt_key));
        }
    
    wxGetApp().load_current_presets();
#endif
}

void MainFrame::select_tab(size_t tab/* = size_t(-1)*/)
{
#if ENABLE_LAYOUT_NO_RESTART
    if (m_layout == ESettingsLayout::Dlg) {
#else
    if (m_layout == slDlg) {
#endif // ENABLE_LAYOUT_NO_RESTART
        if (tab==0) {
#if ENABLE_LAYOUT_NO_RESTART
            if (m_settings_dialog.IsShown())
                this->SetFocus();
#else
            if (m_settings_dialog->IsShown())
                this->SetFocus();
#endif // ENABLE_LAYOUT_NO_RESTART
            // plater should be focused for correct navigation inside search window
            if (m_plater->canvas3D()->is_search_pressed())
                m_plater->SetFocus();
            return;
        }
#if ENABLE_LAYOUT_NO_RESTART
        // Show/Activate Settings Dialog
#ifdef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
        if (m_settings_dialog.IsShown())
            m_settings_dialog.Hide();

        m_tabpanel->Show();
        m_settings_dialog.Show();
#else
        if (m_settings_dialog.IsShown())
            m_settings_dialog.SetFocus();
        else {
            m_tabpanel->Show();
            m_settings_dialog.Show();
        }
#endif
#else
        // Show/Activate Settings Dialog
        if (m_settings_dialog->IsShown())
#ifdef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
            m_settings_dialog->Hide();
#else
            m_settings_dialog->SetFocus();
        else
#endif
        m_settings_dialog->Show();
#endif // ENABLE_LAYOUT_NO_RESTART
    }
#if ENABLE_LAYOUT_NO_RESTART
    else if (m_layout == ESettingsLayout::New) {
        m_main_sizer->Show(m_plater, tab == 0);
        m_main_sizer->Show(m_tabpanel, tab != 0);
#else
    else if (m_layout == slNew) {
        m_plater->Show(tab == 0);
        m_tabpanel->Show(tab != 0);
#endif // ENABLE_LAYOUT_NO_RESTART

        // plater should be focused for correct navigation inside search window
        if (tab == 0 && m_plater->canvas3D()->is_search_pressed())
            m_plater->SetFocus();
        Layout();
    }

    // when tab == -1, it means we should show the last selected tab
#if ENABLE_LAYOUT_NO_RESTART
    m_tabpanel->SetSelection(tab == (size_t)(-1) ? m_last_selected_tab : (m_layout == ESettingsLayout::Dlg && tab != 0) ? tab - 1 : tab);
#else
    m_tabpanel->SetSelection(tab == (size_t)(-1) ? m_last_selected_tab : (m_layout == slDlg && tab != 0) ? tab-1 : tab);
#endif // ENABLE_LAYOUT_NO_RESTART
}

// Set a camera direction, zoom to all objects.
void MainFrame::select_view(const std::string& direction)
{
     if (m_plater)
         m_plater->select_view(direction);
}

// #ys_FIXME_to_delete
void MainFrame::on_presets_changed(SimpleEvent &event)
{
    auto *tab = dynamic_cast<Tab*>(event.GetEventObject());
    wxASSERT(tab != nullptr);
    if (tab == nullptr) {
        return;
    }

    // Update preset combo boxes(Print settings, Filament, Material, Printer) from their respective tabs.
    auto presets = tab->get_presets();
    if (m_plater != nullptr && presets != nullptr) {

        // FIXME: The preset type really should be a property of Tab instead
        Slic3r::Preset::Type preset_type = tab->type();
        if (preset_type == Slic3r::Preset::TYPE_INVALID) {
            wxASSERT(false);
            return;
        }

        m_plater->on_config_change(*tab->get_config());
        m_plater->sidebar().update_presets(preset_type);
    }
}

// #ys_FIXME_to_delete
void MainFrame::on_value_changed(wxCommandEvent& event)
{
    auto *tab = dynamic_cast<Tab*>(event.GetEventObject());
    wxASSERT(tab != nullptr);
    if (tab == nullptr)
        return;

    auto opt_key = event.GetString();
    if (m_plater) {
        m_plater->on_config_change(*tab->get_config()); // propagate config change events to the plater
        if (opt_key == "extruders_count") {
            auto value = event.GetInt();
            m_plater->on_extruders_change(value);
        }
    }
}

void MainFrame::on_config_changed(DynamicPrintConfig* config) const
{
    if (m_plater)
        m_plater->on_config_change(*config); // propagate config change events to the plater
}

void MainFrame::add_to_recent_projects(const wxString& filename)
{
    if (wxFileExists(filename))
    {
        m_recent_projects.AddFileToHistory(filename);
        std::vector<std::string> recent_projects;
        size_t count = m_recent_projects.GetCount();
        for (size_t i = 0; i < count; ++i)
        {
            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
        }
        wxGetApp().app_config->set_recent_projects(recent_projects);
        wxGetApp().app_config->save();
    }
}

//
// Called after the Preferences dialog is closed and the program settings are saved.
// Update the UI based on the current preferences.
void MainFrame::update_ui_from_settings()
{
//    const bool bp_on = wxGetApp().app_config->get("background_processing") == "1";
//     m_menu_item_reslice_now->Enable(!bp_on);
//    m_plater->sidebar().show_reslice(!bp_on);
//    m_plater->sidebar().show_export(bp_on);
//    m_plater->sidebar().Layout();

    if (m_plater)
        m_plater->update_ui_from_settings();
    for (auto tab: wxGetApp().tabs_list)
        tab->update_ui_from_settings();
}

std::string MainFrame::get_base_name(const wxString &full_name, const char *extension) const 
{
    boost::filesystem::path filename = boost::filesystem::path(full_name.wx_str()).filename();
    if (extension != nullptr)
		filename = filename.replace_extension(extension);
    return filename.string();
}

std::string MainFrame::get_dir_name(const wxString &full_name) const 
{
    return boost::filesystem::path(full_name.wx_str()).parent_path().string();
}


// ----------------------------------------------------------------------------
// SettingsDialog
// ----------------------------------------------------------------------------

SettingsDialog::SettingsDialog(MainFrame* mainframe)
: DPIDialog(mainframe, wxID_ANY, wxString(SLIC3R_APP_NAME) + " - " + _L("Settings"), wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX, "settings_dialog"),
    m_main_frame(mainframe)
{
    this->SetFont(wxGetApp().normal_font());
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    // Load the icon either from the exe, or from the ico file.
#if _WIN32
    {
        TCHAR szExeFileName[MAX_PATH];
        GetModuleFileName(nullptr, szExeFileName, MAX_PATH);
        SetIcon(wxIcon(szExeFileName, wxBITMAP_TYPE_ICO));
    }
#else
    SetIcon(wxIcon(var("PrusaSlicer_128px.png"), wxBITMAP_TYPE_PNG));
#endif // _WIN32

#if !ENABLE_LAYOUT_NO_RESTART
    // wxNB_NOPAGETHEME: Disable Windows Vista theme for the Notebook background. The theme performance is terrible on Windows 10
    // with multiple high resolution displays connected.
    m_tabpanel = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxGetApp().get_min_size(), wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
    m_tabpanel->SetFont(Slic3r::GUI::wxGetApp().normal_font());
#endif

    m_tabpanel->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& evt) {
        if ((evt.GetModifiers() & wxMOD_CONTROL) != 0) {
            switch (evt.GetKeyCode()) {
            case '1': { m_main_frame->select_tab(0); break; }
            case '2': { m_main_frame->select_tab(1); break; }
            case '3': { m_main_frame->select_tab(2); break; }
            case '4': { m_main_frame->select_tab(3); break; }
#ifdef __APPLE__
            case 'f':
#else /* __APPLE__ */
            case WXK_CONTROL_F:
#endif /* __APPLE__ */
            case 'F': { m_main_frame->plater()->search(false); break; }
            default:break;
            }
        }
    });
#endif // !ENABLE_LAYOUT_NO_RESTART

#if ENABLE_LAYOUT_NO_RESTART
    this->Bind(wxEVT_SHOW, [this](wxShowEvent& evt) {

        auto key_up_handker = [this](wxKeyEvent& evt) {
            if ((evt.GetModifiers() & wxMOD_CONTROL) != 0) {
                switch (evt.GetKeyCode()) {
                case '1': { m_main_frame->select_tab(0); break; }
                case '2': { m_main_frame->select_tab(1); break; }
                case '3': { m_main_frame->select_tab(2); break; }
                case '4': { m_main_frame->select_tab(3); break; }
#ifdef __APPLE__
                case 'f':
#else /* __APPLE__ */
                case WXK_CONTROL_F:
#endif /* __APPLE__ */
                case 'F': { m_main_frame->plater()->search(false); break; }
                default:break;
                }
            }
        };

        if (evt.IsShown()) {
            if (m_tabpanel != nullptr)
                m_tabpanel->Bind(wxEVT_KEY_UP, key_up_handker);
        }
        else {
            if (m_tabpanel != nullptr)
                m_tabpanel->Unbind(wxEVT_KEY_UP, key_up_handker);
        }
        });
#endif // ENABLE_LAYOUT_NO_RESTART

    // initialize layout
    auto sizer = new wxBoxSizer(wxVERTICAL);
#if !ENABLE_LAYOUT_NO_RESTART
    sizer->Add(m_tabpanel, 1, wxEXPAND);
#endif // !ENABLE_LAYOUT_NO_RESTART
    sizer->SetSizeHints(this);
    SetSizer(sizer);
    Fit();

    const wxSize min_size = wxSize(85 * em_unit(), 50 * em_unit());
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    SetSize(min_size);
#else
    SetMinSize(min_size);
    SetSize(GetMinSize());
#endif
    Layout();
}

void SettingsDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    const int& em = em_unit();
    const wxSize& size = wxSize(85 * em, 50 * em);

    // update Tabs
    for (auto tab : wxGetApp().tabs_list)
        tab->msw_rescale();

    SetMinSize(size);
    Fit();
    Refresh();
}


} // GUI
} // Slic3r
