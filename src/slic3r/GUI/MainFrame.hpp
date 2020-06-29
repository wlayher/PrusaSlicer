#ifndef slic3r_MainFrame_hpp_
#define slic3r_MainFrame_hpp_

#include "libslic3r/PrintConfig.hpp"

#include <wx/frame.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/filehistory.h>

#include <string>
#include <map>

#include "GUI_Utils.hpp"
#include "Event.hpp"

class wxNotebook;
class wxProgressDialog;

namespace Slic3r {

class ProgressStatusBar;

namespace GUI
{

class Tab;
class PrintHostQueueDialog;
class Plater;
class MainFrame;

enum QuickSlice
{
    qsUndef = 0,
    qsReslice = 1,
    qsSaveAs = 2,
    qsExportSVG = 4,
    qsExportPNG = 8
};

struct PresetTab {
    std::string       name;
    Tab*              panel;
    PrinterTechnology technology;
};

// ----------------------------------------------------------------------------
// SettingsDialog
// ----------------------------------------------------------------------------

class SettingsDialog : public DPIDialog
{
    wxNotebook* m_tabpanel { nullptr };
    MainFrame*  m_main_frame { nullptr };
public:
    SettingsDialog(MainFrame* mainframe);
    ~SettingsDialog() {}
#if ENABLE_LAYOUT_NO_RESTART
    void set_tabpanel(wxNotebook* tabpanel) { m_tabpanel = tabpanel; }
#else
    wxNotebook* get_tabpanel() { return m_tabpanel; }
#endif // ENABLE_LAYOUT_NO_RESTART

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;
};

class MainFrame : public DPIFrame
{
    bool        m_loaded {false};

    wxString    m_qs_last_input_file = wxEmptyString;
    wxString    m_qs_last_output_file = wxEmptyString;
    wxString    m_last_config = wxEmptyString;
#if 0
    wxMenuItem* m_menu_item_repeat { nullptr }; // doesn't used now
#endif
    wxMenuItem* m_menu_item_reslice_now { nullptr };
    wxSizer*    m_main_sizer{ nullptr };

    PrintHostQueueDialog *m_printhost_queue_dlg;

    size_t      m_last_selected_tab;

    std::string     get_base_name(const wxString &full_name, const char *extension = nullptr) const;
    std::string     get_dir_name(const wxString &full_name) const;

    void on_presets_changed(SimpleEvent&);
    void on_value_changed(wxCommandEvent&);

    bool can_start_new_project() const;
    bool can_save() const;
    bool can_export_model() const;
    bool can_export_toolpaths() const;
    bool can_export_supports() const;
    bool can_export_gcode() const;
    bool can_send_gcode() const;
	bool can_export_gcode_sd() const;
	bool can_eject() const;
    bool can_slice() const;
    bool can_change_view() const;
    bool can_select() const;
    bool can_deselect() const;
    bool can_delete() const;
    bool can_delete_all() const;
    bool can_reslice() const;

    // MenuBar items changeable in respect to printer technology 
    enum MenuItems
    {                   //   FFF                  SLA
        miExport = 0,   // Export G-code        Export
        miSend,         // Send G-code          Send to print
        miMaterialTab,  // Filament Settings    Material Settings
        miPrinterTab,   // Different bitmap for Printer Settings
    };

    // vector of a MenuBar items changeable in respect to printer technology 
    std::vector<wxMenuItem*> m_changeable_menu_items;

    wxFileHistory m_recent_projects;

#if ENABLE_LAYOUT_NO_RESTART
    enum class ESettingsLayout
    {
        Unknown,
        Old,
        New,
        Dlg,
    };
    
    ESettingsLayout m_layout{ ESettingsLayout::Unknown };
#else
    enum SettingsLayout {
        slOld = 0,
        slNew,
        slDlg,
    }               m_layout;
#endif // ENABLE_LAYOUT_NO_RESTART

protected:
    virtual void on_dpi_changed(const wxRect &suggested_rect);
    virtual void on_sys_color_changed() override;

public:
    MainFrame();
    ~MainFrame() = default;

#if ENABLE_LAYOUT_NO_RESTART
    void update_layout();
#endif // ENABLE_LAYOUT_NO_RESTART

	// Called when closing the application and when switching the application language.
	void 		shutdown();

    Plater*     plater() { return m_plater; }

    void        update_title();

    void        init_tabpanel();
    void        create_preset_tabs();
    void        add_created_tab(Tab* panel);
    void        init_menubar();
    void        update_menubar();

    void        update_ui_from_settings();
    bool        is_loaded() const { return m_loaded; }
    bool        is_last_input_file() const  { return !m_qs_last_input_file.IsEmpty(); }

    void        quick_slice(const int qs = qsUndef);
    void        reslice_now();
    void        repair_stl();
    void        export_config();
    // Query user for the config file and open it.
    void        load_config_file();
    // Open a config file. Return true if loaded.
    bool        load_config_file(const std::string &path);
    void        export_configbundle();
    void        load_configbundle(wxString file = wxEmptyString);
    void        load_config(const DynamicPrintConfig& config);
    // Select tab in m_tabpanel
    // When tab == -1, will be selected last selected tab
    void        select_tab(size_t tab = size_t(-1));
    void        select_view(const std::string& direction);
    // Propagate changed configuration from the Tab to the Plater and save changes to the AppConfig
    void        on_config_changed(DynamicPrintConfig* cfg) const ;

    void        add_to_recent_projects(const wxString& filename);

    PrintHostQueueDialog* printhost_queue_dlg() { return m_printhost_queue_dlg; }

    Plater*             m_plater { nullptr };
    wxNotebook*         m_tabpanel { nullptr };
#if ENABLE_LAYOUT_NO_RESTART
    SettingsDialog      m_settings_dialog;
    wxWindow*           m_plater_page{ nullptr };
#else
    SettingsDialog*     m_settings_dialog { nullptr };
#endif // ENABLE_LAYOUT_NO_RESTART
    wxProgressDialog*   m_progress_dialog { nullptr };
    std::shared_ptr<ProgressStatusBar>  m_statusbar;

#ifdef _WIN32
    void*				m_hDeviceNotify { nullptr };
    uint32_t  			m_ulSHChangeNotifyRegister { 0 };
	static constexpr int WM_USER_MEDIACHANGED { 0x7FFF }; // WM_USER from 0x0400 to 0x7FFF, picking the last one to not interfere with wxWidgets allocation
#endif // _WIN32
};

} // GUI
} //Slic3r

#endif // slic3r_MainFrame_hpp_
