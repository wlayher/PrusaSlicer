#include "libslic3r/libslic3r.h"
#if !ENABLE_GCODE_VIEWER
#include "libslic3r/GCode/PreviewData.hpp"
#endif // !ENABLE_GCODE_VIEWER
#include "GUI_Preview.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#include "I18N.hpp"
#include "3DScene.hpp"
#include "BackgroundSlicingProcess.hpp"
#include "OpenGLManager.hpp"
#include "GLCanvas3D.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "DoubleSlider.hpp"
#include "Plater.hpp"
#if ENABLE_GCODE_VIEWER
#include "MainFrame.hpp"
#endif // ENABLE_GCODE_VIEWER

#include <wx/notebook.h>
#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/combo.h>
#include <wx/checkbox.h>

// this include must follow the wxWidgets ones or it won't compile on Windows -> see http://trac.wxwidgets.org/ticket/2421
#include "libslic3r/Print.hpp"
#include "libslic3r/SLAPrint.hpp"

namespace Slic3r {
namespace GUI {

View3D::View3D(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process)
    : m_canvas_widget(nullptr)
    , m_canvas(nullptr)
{
    init(parent, model, config, process);
}

View3D::~View3D()
{
    if (m_canvas != nullptr)
        delete m_canvas;

    if (m_canvas_widget != nullptr)
        delete m_canvas_widget;
}

bool View3D::init(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process)
{
    if (!Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 /* disable wxTAB_TRAVERSAL */))
        return false;

    m_canvas_widget = OpenGLManager::create_wxglcanvas(*this);
    if (m_canvas_widget == nullptr)
        return false;

    m_canvas = new GLCanvas3D(m_canvas_widget);
    m_canvas->set_context(wxGetApp().init_glcontext(*m_canvas_widget));

    m_canvas->allow_multisample(OpenGLManager::can_multisample());
    // XXX: If have OpenGL
    m_canvas->enable_picking(true);
    m_canvas->enable_moving(true);
    // XXX: more config from 3D.pm
    m_canvas->set_model(model);
    m_canvas->set_process(process);
    m_canvas->set_config(config);
    m_canvas->enable_gizmos(true);
    m_canvas->enable_selection(true);
    m_canvas->enable_main_toolbar(true);
    m_canvas->enable_undoredo_toolbar(true);
    m_canvas->enable_labels(true);
    m_canvas->enable_slope(true);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(m_canvas_widget, 1, wxALL | wxEXPAND, 0);

    SetSizer(main_sizer);
    SetMinSize(GetSize());
    GetSizer()->SetSizeHints(this);

    return true;
}

void View3D::set_as_dirty()
{
    if (m_canvas != nullptr)
        m_canvas->set_as_dirty();
}

void View3D::bed_shape_changed()
{
    if (m_canvas != nullptr)
        m_canvas->bed_shape_changed();
}

void View3D::select_view(const std::string& direction)
{
    if (m_canvas != nullptr)
        m_canvas->select_view(direction);
}

void View3D::select_all()
{
    if (m_canvas != nullptr)
        m_canvas->select_all();
}

void View3D::deselect_all()
{
    if (m_canvas != nullptr)
        m_canvas->deselect_all();
}

void View3D::delete_selected()
{
    if (m_canvas != nullptr)
        m_canvas->delete_selected();
}

void View3D::mirror_selection(Axis axis)
{
    if (m_canvas != nullptr)
        m_canvas->mirror_selection(axis);
}

int View3D::check_volumes_outside_state() const
{
    return (m_canvas != nullptr) ? m_canvas->check_volumes_outside_state() : false;
}

bool View3D::is_layers_editing_enabled() const
{
    return (m_canvas != nullptr) ? m_canvas->is_layers_editing_enabled() : false;
}

bool View3D::is_layers_editing_allowed() const
{
    return (m_canvas != nullptr) ? m_canvas->is_layers_editing_allowed() : false;
}

void View3D::enable_layers_editing(bool enable)
{
    if (m_canvas != nullptr)
        m_canvas->enable_layers_editing(enable);
}

bool View3D::is_dragging() const
{
    return (m_canvas != nullptr) ? m_canvas->is_dragging() : false;
}

bool View3D::is_reload_delayed() const
{
    return (m_canvas != nullptr) ? m_canvas->is_reload_delayed() : false;
}

void View3D::reload_scene(bool refresh_immediately, bool force_full_scene_refresh)
{
    if (m_canvas != nullptr)
        m_canvas->reload_scene(refresh_immediately, force_full_scene_refresh);
}

void View3D::render()
{
    if (m_canvas != nullptr)
        //m_canvas->render();
        m_canvas->set_as_dirty();
}

#if ENABLE_GCODE_VIEWER
Preview::Preview(
    wxWindow* parent, Model* model, DynamicPrintConfig* config,
    BackgroundSlicingProcess* process, GCodeProcessor::Result* gcode_result, std::function<void()> schedule_background_process_func)
#else
Preview::Preview(
    wxWindow* parent, Model* model, DynamicPrintConfig* config,
    BackgroundSlicingProcess* process, GCodePreviewData* gcode_preview_data, std::function<void()> schedule_background_process_func)
#endif // ENABLE_GCODE_VIEWER
    : m_canvas_widget(nullptr)
    , m_canvas(nullptr)
#if ENABLE_GCODE_VIEWER
    , m_left_sizer(nullptr)
    , m_layers_slider_sizer(nullptr)
    , m_bottom_toolbar_panel(nullptr)
#else
    , m_double_slider_sizer(nullptr)
#endif // ENABLE_GCODE_VIEWER
    , m_label_view_type(nullptr)
    , m_choice_view_type(nullptr)
    , m_label_show(nullptr)
    , m_combochecklist_features(nullptr)
#if ENABLE_GCODE_VIEWER
    , m_combochecklist_features_pos(0)
    , m_combochecklist_options(nullptr)
#else
    , m_checkbox_travel(nullptr)
    , m_checkbox_retractions(nullptr)
    , m_checkbox_unretractions(nullptr)
    , m_checkbox_shells(nullptr)
    , m_checkbox_legend(nullptr)
#endif // ENABLE_GCODE_VIEWER
    , m_config(config)
    , m_process(process)
#if ENABLE_GCODE_VIEWER
    , m_gcode_result(gcode_result)
#else
    , m_gcode_preview_data(gcode_preview_data)
#endif // ENABLE_GCODE_VIEWER
    , m_number_extruders(1)
    , m_preferred_color_mode("feature")
    , m_loaded(false)
#if !ENABLE_GCODE_VIEWER
    , m_enabled(false)
#endif // !ENABLE_GCODE_VIEWER
    , m_schedule_background_process(schedule_background_process_func)
#ifdef __linux__
    , m_volumes_cleanup_required(false)
#endif // __linux__
{
    if (init(parent, model)) {
#if !ENABLE_GCODE_VIEWER
        show_hide_ui_elements("none");
#endif // !ENABLE_GCODE_VIEWER
        load_print();
    }
}

bool Preview::init(wxWindow* parent, Model* model)
{
    if (!Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 /* disable wxTAB_TRAVERSAL */))
        return false;

#if ENABLE_GCODE_VIEWER
    // to match the background of the sliders
#ifdef _WIN32 
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#else
    SetBackgroundColour(GetParent()->GetBackgroundColour());
#endif // _WIN32 
#endif // ENABLE_GCODE_VIEWER

    m_canvas_widget = OpenGLManager::create_wxglcanvas(*this);
    if (m_canvas_widget == nullptr)
        return false;

    m_canvas = new GLCanvas3D(m_canvas_widget);
    m_canvas->set_context(wxGetApp().init_glcontext(*m_canvas_widget));
    m_canvas->allow_multisample(OpenGLManager::can_multisample());
    m_canvas->set_config(m_config);
    m_canvas->set_model(model);
    m_canvas->set_process(m_process);
    m_canvas->enable_legend_texture(true);
    m_canvas->enable_dynamic_background(true);

#if ENABLE_GCODE_VIEWER
    m_layers_slider_sizer = create_layers_slider_sizer();

    m_bottom_toolbar_panel = new wxPanel(this);
    m_label_view_type = new wxStaticText(m_bottom_toolbar_panel, wxID_ANY, _L("View"));
    m_choice_view_type = new wxChoice(m_bottom_toolbar_panel, wxID_ANY);
#else
    m_double_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
    create_double_slider();

    m_label_view_type = new wxStaticText(this, wxID_ANY, _L("View"));

    m_choice_view_type = new wxChoice(this, wxID_ANY);
#endif // ENABLE_GCODE_VIEWER
    m_choice_view_type->Append(_L("Feature type"));
    m_choice_view_type->Append(_L("Height"));
    m_choice_view_type->Append(_L("Width"));
    m_choice_view_type->Append(_L("Speed"));
    m_choice_view_type->Append(_L("Fan speed"));
    m_choice_view_type->Append(_L("Volumetric flow rate"));
    m_choice_view_type->Append(_L("Tool"));
    m_choice_view_type->Append(_L("Color Print"));
    m_choice_view_type->SetSelection(0);

#if ENABLE_GCODE_VIEWER
    m_label_show = new wxStaticText(m_bottom_toolbar_panel, wxID_ANY, _L("Show"));
#else
    m_label_show = new wxStaticText(this, wxID_ANY, _L("Show"));
#endif // ENABLE_GCODE_VIEWER

    m_combochecklist_features = new wxComboCtrl();
#if ENABLE_GCODE_VIEWER
    m_combochecklist_features->Create(m_bottom_toolbar_panel, wxID_ANY, _L("Feature types"), wxDefaultPosition, wxDefaultSize, wxCB_READONLY);
#else
    m_combochecklist_features->Create(this, wxID_ANY, _L("Feature types"), wxDefaultPosition, wxDefaultSize, wxCB_READONLY);
#endif // ENABLE_GCODE_VIEWER
    std::string feature_items = GUI::into_u8(
#if ENABLE_GCODE_VIEWER
        _L("Unknown") + "|1|" +
#endif // ENABLE_GCODE_VIEWER
        _L("Perimeter") + "|1|" +
        _L("External perimeter") + "|1|" +
        _L("Overhang perimeter") + "|1|" +
        _L("Internal infill") + "|1|" +
        _L("Solid infill") + "|1|" +
        _L("Top solid infill") + "|1|" +
        _L("Ironing") + "|1|" +
        _L("Bridge infill") + "|1|" +
        _L("Gap fill") + "|1|" +
        _L("Skirt") + "|1|" +
        _L("Support material") + "|1|" +
        _L("Support material interface") + "|1|" +
        _L("Wipe tower") + "|1|" +
        _L("Custom") + "|1"
    );
    Slic3r::GUI::create_combochecklist(m_combochecklist_features, GUI::into_u8(_L("Feature types")), feature_items);

#if ENABLE_GCODE_VIEWER
    m_combochecklist_options = new wxComboCtrl();
    m_combochecklist_options->Create(m_bottom_toolbar_panel, wxID_ANY, _L("Options"), wxDefaultPosition, wxDefaultSize, wxCB_READONLY);
    std::string options_items = GUI::into_u8(
        get_option_type_string(OptionType::Travel) + "|0|" +
        get_option_type_string(OptionType::Retractions) + "|0|" +
        get_option_type_string(OptionType::Unretractions) + "|0|" +
        get_option_type_string(OptionType::ToolChanges) + "|0|" +
        get_option_type_string(OptionType::ColorChanges) + "|0|" +
        get_option_type_string(OptionType::PausePrints) + "|0|" +
        get_option_type_string(OptionType::CustomGCodes) + "|0|" +
        get_option_type_string(OptionType::Shells) + "|0|" +
        get_option_type_string(OptionType::ToolMarker) + "|1|" +
        get_option_type_string(OptionType::Legend) + "|1"
);
    Slic3r::GUI::create_combochecklist(m_combochecklist_options, GUI::into_u8(_L("Options")), options_items);
#else
    m_checkbox_travel = new wxCheckBox(this, wxID_ANY, _L("Travel"));
    m_checkbox_retractions = new wxCheckBox(this, wxID_ANY, _L("Retractions"));
    m_checkbox_unretractions = new wxCheckBox(this, wxID_ANY, _L("Deretractions"));
    m_checkbox_shells = new wxCheckBox(this, wxID_ANY, _L("Shells"));
    m_checkbox_legend = new wxCheckBox(this, wxID_ANY, _L("Legend"));
    m_checkbox_legend->SetValue(true);
#endif // ENABLE_GCODE_VIEWER

#if ENABLE_GCODE_VIEWER
    m_left_sizer = new wxBoxSizer(wxVERTICAL);
    m_left_sizer->Add(m_canvas_widget, 1, wxALL | wxEXPAND, 0);

    wxBoxSizer* right_sizer = new wxBoxSizer(wxVERTICAL);
    right_sizer->Add(m_layers_slider_sizer, 1, wxEXPAND, 0);

    m_moves_slider = new DoubleSlider::Control(m_bottom_toolbar_panel, wxID_ANY, 0, 0, 0, 100, wxDefaultPosition, wxSize(-1, 3 * GetTextExtent("m").y), wxSL_HORIZONTAL);
    m_moves_slider->SetDrawMode(DoubleSlider::dmSequentialGCodeView);

    wxBoxSizer* bottom_toolbar_sizer = new wxBoxSizer(wxHORIZONTAL);
    bottom_toolbar_sizer->AddSpacer(5);
    bottom_toolbar_sizer->Add(m_label_view_type, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    bottom_toolbar_sizer->Add(m_choice_view_type, 0, wxALIGN_CENTER_VERTICAL, 0);
    bottom_toolbar_sizer->AddSpacer(5);
    bottom_toolbar_sizer->Add(m_label_show, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
    bottom_toolbar_sizer->Add(m_combochecklist_options, 0, wxALIGN_CENTER_VERTICAL, 0);
    // change the following number if editing the layout of the bottom toolbar sizer. It is used into update_bottom_toolbar()
    m_combochecklist_features_pos = 6;
    bottom_toolbar_sizer->Add(m_combochecklist_features, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    bottom_toolbar_sizer->Hide(m_combochecklist_features);
    bottom_toolbar_sizer->AddSpacer(5);
    bottom_toolbar_sizer->Add(m_moves_slider, 1, wxALL | wxEXPAND, 0);
    m_bottom_toolbar_panel->SetSizer(bottom_toolbar_sizer);

    m_left_sizer->Add(m_bottom_toolbar_panel, 0, wxALL | wxEXPAND, 0);
    m_left_sizer->Hide(m_bottom_toolbar_panel);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);
    main_sizer->Add(m_left_sizer, 1, wxALL | wxEXPAND, 0);
    main_sizer->Add(right_sizer, 0, wxALL | wxEXPAND, 0);
#else
    wxBoxSizer* top_sizer = new wxBoxSizer(wxHORIZONTAL);
    top_sizer->Add(m_canvas_widget, 1, wxALL | wxEXPAND, 0);
    top_sizer->Add(m_double_slider_sizer, 0, wxEXPAND, 0);

    wxBoxSizer* bottom_sizer = new wxBoxSizer(wxHORIZONTAL);
    bottom_sizer->Add(m_label_view_type, 0, wxALIGN_CENTER_VERTICAL, 5);
    bottom_sizer->Add(m_choice_view_type, 0, wxEXPAND | wxALL, 5);
    bottom_sizer->AddSpacer(10);
    bottom_sizer->Add(m_label_show, 0, wxALIGN_CENTER_VERTICAL, 5);
    bottom_sizer->Add(m_combochecklist_features, 0, wxEXPAND | wxALL, 5);
    bottom_sizer->AddSpacer(20);
    bottom_sizer->Add(m_checkbox_travel, 0, wxEXPAND | wxALL, 5);
    bottom_sizer->AddSpacer(10);
    bottom_sizer->Add(m_checkbox_retractions, 0, wxEXPAND | wxALL, 5);
    bottom_sizer->AddSpacer(10);
    bottom_sizer->Add(m_checkbox_unretractions, 0, wxEXPAND | wxALL, 5);
    bottom_sizer->AddSpacer(10);
    bottom_sizer->Add(m_checkbox_shells, 0, wxEXPAND | wxALL, 5);
    bottom_sizer->AddSpacer(20);
    bottom_sizer->Add(m_checkbox_legend, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(top_sizer, 1, wxALL | wxEXPAND, 0);
    main_sizer->Add(bottom_sizer, 0, wxALL | wxEXPAND, 0);
#endif // ENABLE_GCODE_VIEWER

    SetSizer(main_sizer);
    SetMinSize(GetSize());
    GetSizer()->SetSizeHints(this);

    bind_event_handlers();
    
#if !ENABLE_GCODE_VIEWER
    // sets colors for gcode preview extrusion roles
    std::vector<std::string> extrusion_roles_colors = {
        "Perimeter", "FFFF66",
        "External perimeter", "FFA500",
        "Overhang perimeter", "0000FF",
        "Internal infill", "B1302A",
        "Solid infill", "D732D7",
        "Top solid infill", "FF1A1A",
        "Bridge infill", "9999FF",
        "Gap fill", "FFFFFF",
        "Skirt", "845321",
        "Support material", "00FF00",
        "Support material interface", "008000",
        "Wipe tower", "B3E3AB",
        "Custom", "28CC94"
    };
    m_gcode_preview_data->set_extrusion_paths_colors(extrusion_roles_colors);
#endif // !ENABLE_GCODE_VIEWER

    return true;
}

Preview::~Preview()
{
    unbind_event_handlers();

    if (m_canvas != nullptr)
        delete m_canvas;

    if (m_canvas_widget != nullptr)
        delete m_canvas_widget;
}

void Preview::set_as_dirty()
{
    if (m_canvas != nullptr)
        m_canvas->set_as_dirty();
}

void Preview::set_number_extruders(unsigned int number_extruders)
{
    if (m_number_extruders != number_extruders)
    {
        m_number_extruders = number_extruders;
        int tool_idx = m_choice_view_type->FindString(_(L("Tool")));
        int type = (number_extruders > 1) ? tool_idx /* color by a tool number */  : 0; // color by a feature type
        m_choice_view_type->SetSelection(type);
#if ENABLE_GCODE_VIEWER
        if ((0 <= type) && (type < static_cast<int>(GCodeViewer::EViewType::Count)))
            m_canvas->set_gcode_view_preview_type(static_cast<GCodeViewer::EViewType>(type));
#else
        if ((0 <= type) && (type < (int)GCodePreviewData::Extrusion::Num_View_Types))
            m_gcode_preview_data->extrusion.view_type = (GCodePreviewData::Extrusion::EViewType)type;
#endif // ENABLE_GCODE_VIEWER

        m_preferred_color_mode = (type == tool_idx) ? "tool_or_feature" : "feature";
    }
}

#if !ENABLE_GCODE_VIEWER
void Preview::set_enabled(bool enabled)
{
    m_enabled = enabled;
}
#endif // !ENABLE_GCODE_VIEWER

void Preview::bed_shape_changed()
{
    if (m_canvas != nullptr)
        m_canvas->bed_shape_changed();
}

void Preview::select_view(const std::string& direction)
{
    m_canvas->select_view(direction);
}

void Preview::set_drop_target(wxDropTarget* target)
{
    if (target != nullptr)
        SetDropTarget(target);
}

void Preview::load_print(bool keep_z_range)
{
    PrinterTechnology tech = m_process->current_printer_technology();
    if (tech == ptFFF)
        load_print_as_fff(keep_z_range);
    else if (tech == ptSLA)
        load_print_as_sla();

#if ENABLE_GCODE_VIEWER
    update_bottom_toolbar();
#endif // ENABLE_GCODE_VIEWER
    Layout();
}

void Preview::reload_print(bool keep_volumes)
{
#ifdef __linux__
    // We are getting mysterious crashes on Linux in gtk due to OpenGL context activation GH #1874 #1955.
    // So we are applying a workaround here: a delayed release of OpenGL vertex buffers.
    if (!IsShown())
    {
        m_volumes_cleanup_required = !keep_volumes;
        return;
    }
#endif /* __linux __ */
    if (
#ifdef __linux__
        m_volumes_cleanup_required || 
#endif /* __linux__ */
        !keep_volumes)
    {
        m_canvas->reset_volumes();
#if !ENABLE_GCODE_VIEWER
        m_canvas->reset_legend_texture();
#endif // !ENABLE_GCODE_VIEWER
        m_loaded = false;
#ifdef __linux__
        m_volumes_cleanup_required = false;
#endif /* __linux__ */
    }

    load_print();
}

void Preview::refresh_print()
{
    m_loaded = false;

    if (!IsShown())
        return;

    load_print(true);
}

void Preview::msw_rescale()
{
    // rescale slider
#if ENABLE_GCODE_VIEWER
    if (m_layers_slider != nullptr) m_layers_slider->msw_rescale();
    if (m_moves_slider != nullptr) m_moves_slider->msw_rescale();
#else
    if (m_slider) m_slider->msw_rescale();
#endif // ENABLE_GCODE_VIEWER

    // rescale warning legend on the canvas
    get_canvas3d()->msw_rescale();

    // rescale legend
    refresh_print();
}

void Preview::jump_layers_slider(wxKeyEvent& evt)
{
#if ENABLE_GCODE_VIEWER
    if (m_layers_slider) m_layers_slider->OnChar(evt);
#else
    if (m_slider)
        m_slider->OnKeyDown(evt);
#endif // ENABLE_GCODE_VIEWER
}

#if ENABLE_GCODE_VIEWER
void Preview::move_layers_slider(wxKeyEvent& evt)
{
    if (m_layers_slider != nullptr) m_layers_slider->OnKeyDown(evt);
}
#else
void Preview::move_double_slider(wxKeyEvent& evt)
{
    if (m_slider)
        m_slider->OnKeyDown(evt);
}
#endif // ENABLE_GCODE_VIEWER

#if ENABLE_GCODE_VIEWER
void Preview::edit_layers_slider(wxKeyEvent& evt)
{
    if (m_layers_slider != nullptr) m_layers_slider->OnChar(evt);
}
#else
void Preview::edit_double_slider(wxKeyEvent& evt)
{
    if (m_slider)
        m_slider->OnChar(evt);
}
#endif // ENABLE_GCODE_VIEWER

void Preview::bind_event_handlers()
{
    this->Bind(wxEVT_SIZE, &Preview::on_size, this);
    m_choice_view_type->Bind(wxEVT_CHOICE, &Preview::on_choice_view_type, this);
    m_combochecklist_features->Bind(wxEVT_CHECKLISTBOX, &Preview::on_combochecklist_features, this);
#if ENABLE_GCODE_VIEWER
    m_combochecklist_options->Bind(wxEVT_CHECKLISTBOX, &Preview::on_combochecklist_options, this);
    m_moves_slider->Bind(wxEVT_SCROLL_CHANGED, &Preview::on_moves_slider_scroll_changed, this);
#else
    m_checkbox_travel->Bind(wxEVT_CHECKBOX, &Preview::on_checkbox_travel, this);
    m_checkbox_retractions->Bind(wxEVT_CHECKBOX, &Preview::on_checkbox_retractions, this);
    m_checkbox_unretractions->Bind(wxEVT_CHECKBOX, &Preview::on_checkbox_unretractions, this);
    m_checkbox_shells->Bind(wxEVT_CHECKBOX, &Preview::on_checkbox_shells, this);
    m_checkbox_legend->Bind(wxEVT_CHECKBOX, &Preview::on_checkbox_legend, this);
#endif // ENABLE_GCODE_VIEWER
}

void Preview::unbind_event_handlers()
{
    this->Unbind(wxEVT_SIZE, &Preview::on_size, this);
    m_choice_view_type->Unbind(wxEVT_CHOICE, &Preview::on_choice_view_type, this);
    m_combochecklist_features->Unbind(wxEVT_CHECKLISTBOX, &Preview::on_combochecklist_features, this);
#if ENABLE_GCODE_VIEWER
    m_combochecklist_options->Unbind(wxEVT_CHECKLISTBOX, &Preview::on_combochecklist_options, this);
    m_moves_slider->Unbind(wxEVT_SCROLL_CHANGED, &Preview::on_moves_slider_scroll_changed, this);
#else
    m_checkbox_travel->Unbind(wxEVT_CHECKBOX, &Preview::on_checkbox_travel, this);
    m_checkbox_retractions->Unbind(wxEVT_CHECKBOX, &Preview::on_checkbox_retractions, this);
    m_checkbox_unretractions->Unbind(wxEVT_CHECKBOX, &Preview::on_checkbox_unretractions, this);
    m_checkbox_shells->Unbind(wxEVT_CHECKBOX, &Preview::on_checkbox_shells, this);
    m_checkbox_legend->Unbind(wxEVT_CHECKBOX, &Preview::on_checkbox_legend, this);
#endif // ENABLE_GCODE_VIEWER
}

#if !ENABLE_GCODE_VIEWER
void Preview::show_hide_ui_elements(const std::string& what)
{
    bool enable = (what == "full");
    m_label_show->Enable(enable);
    m_combochecklist_features->Enable(enable);
    m_checkbox_travel->Enable(enable);
    m_checkbox_retractions->Enable(enable);
    m_checkbox_unretractions->Enable(enable);
    m_checkbox_shells->Enable(enable);
    m_checkbox_legend->Enable(enable);

    enable = (what != "none");
    m_label_view_type->Enable(enable);
    m_choice_view_type->Enable(enable);

    bool visible = (what != "none");
    m_label_show->Show(visible);
    m_combochecklist_features->Show(visible);
    m_checkbox_travel->Show(visible);
    m_checkbox_retractions->Show(visible);
    m_checkbox_unretractions->Show(visible);
    m_checkbox_shells->Show(visible);
    m_checkbox_legend->Show(visible);
    m_label_view_type->Show(visible);
    m_choice_view_type->Show(visible);
}
#endif // !ENABLE_GCODE_VIEWER

#if ENABLE_GCODE_VIEWER
void Preview::hide_layers_slider()
{
    m_layers_slider_sizer->Hide((size_t)0);
    Layout();
}
#else
void Preview::reset_sliders(bool reset_all)
{
    m_enabled = false;
    //    reset_double_slider();
    if (reset_all)
        m_double_slider_sizer->Hide((size_t)0);
    else
        m_double_slider_sizer->GetItem(size_t(0))->GetSizer()->Hide(1);
}
#endif // ENABLE_GCODE_VIEWER

#if !ENABLE_GCODE_VIEWER
void Preview::update_sliders(const std::vector<double>& layers_z, bool keep_z_range)
{
    m_enabled = true;
    update_double_slider(layers_z, keep_z_range);

    m_double_slider_sizer->Show((size_t)0);

    Layout();
}
#endif // !ENABLE_GCODE_VIEWER

void Preview::on_size(wxSizeEvent& evt)
{
    evt.Skip();
    Refresh();
}

void Preview::on_choice_view_type(wxCommandEvent& evt)
{
    m_preferred_color_mode = (m_choice_view_type->GetStringSelection() == L("Tool")) ? "tool" : "feature";
    int selection = m_choice_view_type->GetCurrentSelection();
#if ENABLE_GCODE_VIEWER
    if (0 <= selection && selection < static_cast<int>(GCodeViewer::EViewType::Count))
        m_canvas->set_toolpath_view_type(static_cast<GCodeViewer::EViewType>(selection));

    refresh_print();
#else
    if ((0 <= selection) && (selection < (int)GCodePreviewData::Extrusion::Num_View_Types))
        m_gcode_preview_data->extrusion.view_type = (GCodePreviewData::Extrusion::EViewType)selection;

    reload_print();
#endif // ENABLE_GCODE_VIEWER
}

void Preview::on_combochecklist_features(wxCommandEvent& evt)
{
    unsigned int flags = Slic3r::GUI::combochecklist_get_flags(m_combochecklist_features);
#if ENABLE_GCODE_VIEWER
    m_canvas->set_toolpath_role_visibility_flags(flags);
#else
    m_gcode_preview_data->extrusion.role_flags = flags;
#endif // ENABLE_GCODE_VIEWER
    refresh_print();
}

#if ENABLE_GCODE_VIEWER
void Preview::on_combochecklist_options(wxCommandEvent& evt)
{
    auto xored = [](unsigned int flags1, unsigned int flags2, unsigned int flag) {
        auto is_flag_set = [](unsigned int flags, unsigned int flag) {
            return (flags & (1 << flag)) != 0;
        };
        return !is_flag_set(flags1, flag) != !is_flag_set(flags2, flag);
    };

    unsigned int curr_flags = m_canvas->get_gcode_options_visibility_flags();
    unsigned int new_flags = Slic3r::GUI::combochecklist_get_flags(m_combochecklist_options);
    if (curr_flags == new_flags)
        return;

    m_canvas->set_gcode_options_visibility_from_flags(new_flags);

    bool skip_refresh = xored(curr_flags, new_flags, static_cast<unsigned int>(OptionType::Shells)) ||
        xored(curr_flags, new_flags, static_cast<unsigned int>(OptionType::ToolMarker));

    if (!skip_refresh)
        refresh_print();
    else
        m_canvas->set_as_dirty();
}
#else
void Preview::on_checkbox_travel(wxCommandEvent& evt)
{
    m_gcode_preview_data->travel.is_visible = m_checkbox_travel->IsChecked();
    m_gcode_preview_data->ranges.feedrate.set_mode(GCodePreviewData::FeedrateKind::TRAVEL, m_gcode_preview_data->travel.is_visible);
    // Rather than refresh, reload print so that speed color ranges get recomputed (affected by travel visibility)
    reload_print();
}

void Preview::on_checkbox_retractions(wxCommandEvent& evt)
{
    m_gcode_preview_data->retraction.is_visible = m_checkbox_retractions->IsChecked();
    refresh_print();
}

void Preview::on_checkbox_unretractions(wxCommandEvent& evt)
{
    m_gcode_preview_data->unretraction.is_visible = m_checkbox_unretractions->IsChecked();
    refresh_print();
}

void Preview::on_checkbox_shells(wxCommandEvent& evt)
{
    m_gcode_preview_data->shell.is_visible = m_checkbox_shells->IsChecked();
    refresh_print();
}

void Preview::on_checkbox_legend(wxCommandEvent& evt)
{
    m_canvas->enable_legend_texture(m_checkbox_legend->IsChecked());
    m_canvas_widget->Refresh();
}
#endif // ENABLE_GCODE_VIEWER

void Preview::update_view_type(bool keep_volumes)
{
    const DynamicPrintConfig& config = wxGetApp().preset_bundle->project_config;

    const wxString& choice = !wxGetApp().plater()->model().custom_gcode_per_print_z.gcodes.empty() /*&&
                             (wxGetApp().extruders_edited_cnt()==1 || !slice_completed) */? 
                                _L("Color Print") :
                                config.option<ConfigOptionFloats>("wiping_volumes_matrix")->values.size() > 1 ?
                                    _L("Tool") : 
                                    _L("Feature type");

    int type = m_choice_view_type->FindString(choice);
    if (m_choice_view_type->GetSelection() != type) {
        m_choice_view_type->SetSelection(type);
#if ENABLE_GCODE_VIEWER
        if ((0 <= type) && (type < static_cast<int>(GCodeViewer::EViewType::Count)))
            m_canvas->set_gcode_view_preview_type(static_cast<GCodeViewer::EViewType>(type));
#else
        if (0 <= type && type < (int)GCodePreviewData::Extrusion::Num_View_Types)
            m_gcode_preview_data->extrusion.view_type = (GCodePreviewData::Extrusion::EViewType)type;
#endif // ENABLE_GCODE_VIEWER
        m_preferred_color_mode = "feature";
    }

#if ENABLE_GCODE_VIEWER
    reload_print(keep_volumes);
#else
    reload_print();
#endif // ENABLE_GCODE_VIEWER
}

#if ENABLE_GCODE_VIEWER
void Preview::update_bottom_toolbar()
{
    combochecklist_set_flags(m_combochecklist_features, m_canvas->get_toolpath_role_visibility_flags());
    combochecklist_set_flags(m_combochecklist_options, m_canvas->get_gcode_options_visibility_flags());

    // updates visibility of features combobox
    if (m_bottom_toolbar_panel->IsShown())
    {
        wxSizer* sizer = m_bottom_toolbar_panel->GetSizer();
        bool show = !m_canvas->is_gcode_legend_enabled() || m_canvas->get_gcode_view_type() != GCodeViewer::EViewType::FeatureType;

        if (show)
        {
            if (sizer->GetItem(m_combochecklist_features) == nullptr)
            {
                sizer->Insert(m_combochecklist_features_pos, m_combochecklist_features, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
                sizer->Show(m_combochecklist_features);
                sizer->Layout();
                Refresh();
            }
        }
        else
        {
            if (sizer->GetItem(m_combochecklist_features) != nullptr)
            {
                sizer->Hide(m_combochecklist_features);
                sizer->Detach(m_combochecklist_features);
                sizer->Layout();
                Refresh();
            }
        }
    }
}
#endif // ENABLE_GCODE_VIEWER

#if ENABLE_GCODE_VIEWER
wxBoxSizer* Preview::create_layers_slider_sizer()
{
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    m_layers_slider = new DoubleSlider::Control(this, wxID_ANY, 0, 0, 0, 100);

    m_layers_slider->SetDrawMode(wxGetApp().preset_bundle->printers.get_edited_preset().printer_technology() == ptSLA,
        wxGetApp().preset_bundle->prints.get_edited_preset().config.opt_bool("complete_objects"));
    m_layers_slider->enable_action_icon(wxGetApp().is_editor());

    sizer->Add(m_layers_slider, 0, wxEXPAND, 0);

    // sizer, m_canvas_widget
    m_canvas_widget->Bind(wxEVT_KEY_DOWN, &Preview::update_layers_slider_from_canvas, this);
    m_canvas_widget->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_SHIFT)
            m_layers_slider->UseDefaultColors(true);
        event.Skip();
        });

    m_layers_slider->Bind(wxEVT_SCROLL_CHANGED, &Preview::on_layers_slider_scroll_changed, this);

    Bind(DoubleSlider::wxCUSTOMEVT_TICKSCHANGED, [this](wxEvent&) {
        Model& model = wxGetApp().plater()->model();
        model.custom_gcode_per_print_z = m_layers_slider->GetTicksValues();
        m_schedule_background_process();

        update_view_type(false);
        });

    return sizer;
}
#else
void Preview::create_double_slider()
{
    m_slider = new DoubleSlider::Control(this, wxID_ANY, 0, 0, 0, 100);

    bool sla_print_technology = wxGetApp().preset_bundle->printers.get_edited_preset().printer_technology() == ptSLA;
    bool sequential_print = wxGetApp().preset_bundle->prints.get_edited_preset().config.opt_bool("complete_objects");
    m_slider->SetDrawMode(sla_print_technology, sequential_print);

    m_double_slider_sizer->Add(m_slider, 0, wxEXPAND, 0);


    // sizer, m_canvas_widget
    m_canvas_widget->Bind(wxEVT_KEY_DOWN, &Preview::update_double_slider_from_canvas, this);
    m_canvas_widget->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_SHIFT)
            m_slider->UseDefaultColors(true);
        event.Skip();
        });

    m_slider->Bind(wxEVT_SCROLL_CHANGED, &Preview::on_sliders_scroll_changed, this);

    Bind(DoubleSlider::wxCUSTOMEVT_TICKSCHANGED, [this](wxEvent&) {
        Model& model = wxGetApp().plater()->model();
        model.custom_gcode_per_print_z = m_slider->GetTicksValues();
        m_schedule_background_process();

        update_view_type(false);
    });
}
#endif // ENABLE_GCODE_VIEWER

// Find an index of a value in a sorted vector, which is in <z-eps, z+eps>.
// Returns -1 if there is no such member.
static int find_close_layer_idx(const std::vector<double>& zs, double &z, double eps)
{
    if (zs.empty())
        return -1;
    auto it_h = std::lower_bound(zs.begin(), zs.end(), z);
    if (it_h == zs.end()) {
        auto it_l = it_h;
        -- it_l;
        if (z - *it_l < eps)
            return int(zs.size() - 1);
    } else if (it_h == zs.begin()) {
        if (*it_h - z < eps)
            return 0;
    } else {
        auto it_l = it_h;
        -- it_l;
        double dist_l = z - *it_l;
        double dist_h = *it_h - z;
        if (std::min(dist_l, dist_h) < eps) {
            return (dist_l < dist_h) ? int(it_l - zs.begin()) : int(it_h - zs.begin());
        }
    }
    return -1;
}

#if ENABLE_GCODE_VIEWER
void Preview::check_layers_slider_values(std::vector<CustomGCode::Item>& ticks_from_model, const std::vector<double>& layers_z)
#else
void Preview::check_slider_values(std::vector<CustomGCode::Item>& ticks_from_model,
                                  const std::vector<double>& layers_z)
#endif // ENABLE_GCODE_VIEWER
{
    // All ticks that would end up outside the slider range should be erased.
    // TODO: this should be placed into more appropriate part of code,
    // this function is e.g. not called when the last object is deleted
    unsigned int old_size = ticks_from_model.size();
    ticks_from_model.erase(std::remove_if(ticks_from_model.begin(), ticks_from_model.end(),
                     [layers_z](CustomGCode::Item val)
        {
            auto it = std::lower_bound(layers_z.begin(), layers_z.end(), val.print_z - DoubleSlider::epsilon());
            return it == layers_z.end();
        }),
        ticks_from_model.end());
    if (ticks_from_model.size() != old_size)
        m_schedule_background_process();
}

#if ENABLE_GCODE_VIEWER
void Preview::update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range)
{
    // Save the initial slider span.
    double z_low = m_layers_slider->GetLowerValueD();
    double z_high = m_layers_slider->GetHigherValueD();
    bool   was_empty = m_layers_slider->GetMaxValue() == 0;

    bool force_sliders_full_range = was_empty;
    if (!keep_z_range)
    {
        bool span_changed = layers_z.empty() || std::abs(layers_z.back() - m_layers_slider->GetMaxValueD()) > DoubleSlider::epsilon()/*1e-6*/;
        force_sliders_full_range |= span_changed;
    }
    bool   snap_to_min = force_sliders_full_range || m_layers_slider->is_lower_at_min();
    bool   snap_to_max = force_sliders_full_range || m_layers_slider->is_higher_at_max();

    // Detect and set manipulation mode for double slider
    update_layers_slider_mode();

    CustomGCode::Info& ticks_info_from_model = wxGetApp().plater()->model().custom_gcode_per_print_z;
    check_layers_slider_values(ticks_info_from_model.gcodes, layers_z);

    m_layers_slider->SetSliderValues(layers_z);
    assert(m_layers_slider->GetMinValue() == 0);
    m_layers_slider->SetMaxValue(layers_z.empty() ? 0 : layers_z.size() - 1);

    int idx_low = 0;
    int idx_high = m_layers_slider->GetMaxValue();
    if (!layers_z.empty()) {
        if (!snap_to_min) {
            int idx_new = find_close_layer_idx(layers_z, z_low, DoubleSlider::epsilon()/*1e-6*/);
            if (idx_new != -1)
                idx_low = idx_new;
        }
        if (!snap_to_max) {
            int idx_new = find_close_layer_idx(layers_z, z_high, DoubleSlider::epsilon()/*1e-6*/);
            if (idx_new != -1)
                idx_high = idx_new;
        }
    }
    m_layers_slider->SetSelectionSpan(idx_low, idx_high);
    m_layers_slider->SetTicksValues(ticks_info_from_model);

    bool sla_print_technology = wxGetApp().plater()->printer_technology() == ptSLA;
    bool sequential_print = wxGetApp().preset_bundle->prints.get_edited_preset().config.opt_bool("complete_objects");
    m_layers_slider->SetDrawMode(sla_print_technology, sequential_print);
    m_layers_slider->SetExtruderColors(wxGetApp().plater()->get_extruder_colors_from_plater_config());

    m_layers_slider_sizer->Show((size_t)0);
    Layout();
}
#else
void Preview::update_double_slider(const std::vector<double> & layers_z, bool keep_z_range)
{
    // Save the initial slider span.
    double z_low        = m_slider->GetLowerValueD();
    double z_high       = m_slider->GetHigherValueD();
    bool   was_empty    = m_slider->GetMaxValue() == 0;

    bool force_sliders_full_range = was_empty;
    if (!keep_z_range)
    {
        bool span_changed = layers_z.empty() || std::abs(layers_z.back() - m_slider->GetMaxValueD()) > DoubleSlider::epsilon()/*1e-6*/;
        force_sliders_full_range |= span_changed;
    }
    bool   snap_to_min = force_sliders_full_range || m_slider->is_lower_at_min();
    bool   snap_to_max = force_sliders_full_range || m_slider->is_higher_at_max();

    // Detect and set manipulation mode for double slider
    update_double_slider_mode();

    CustomGCode::Info& ticks_info_from_model = wxGetApp().plater()->model().custom_gcode_per_print_z;
    check_slider_values(ticks_info_from_model.gcodes, layers_z);

    m_slider->SetSliderValues(layers_z);
    assert(m_slider->GetMinValue() == 0);
    m_slider->SetMaxValue(layers_z.empty() ? 0 : layers_z.size() - 1);

    int idx_low = 0;
    int idx_high = m_slider->GetMaxValue();
    if (!layers_z.empty()) {
        if (!snap_to_min) {
            int idx_new = find_close_layer_idx(layers_z, z_low, DoubleSlider::epsilon()/*1e-6*/);
            if (idx_new != -1)
                idx_low = idx_new;
        }
        if (!snap_to_max) {
            int idx_new = find_close_layer_idx(layers_z, z_high, DoubleSlider::epsilon()/*1e-6*/);
            if (idx_new != -1)
                idx_high = idx_new;
        }
    }
    m_slider->SetSelectionSpan(idx_low, idx_high);

    m_slider->SetTicksValues(ticks_info_from_model);

    bool sla_print_technology = wxGetApp().plater()->printer_technology() == ptSLA;
    bool sequential_print = wxGetApp().preset_bundle->prints.get_edited_preset().config.opt_bool("complete_objects");
    m_slider->SetDrawMode(sla_print_technology, sequential_print);

    m_slider->SetExtruderColors(wxGetApp().plater()->get_extruder_colors_from_plater_config());
}
#endif // ENABLE_GCODE_VIEWER

#if ENABLE_GCODE_VIEWER
void Preview::update_layers_slider_mode()
#else
void Preview::update_double_slider_mode()
#endif // ENABLE_GCODE_VIEWER
{
    //    true  -> single-extruder printer profile OR 
    //             multi-extruder printer profile , but whole model is printed by only one extruder
    //    false -> multi-extruder printer profile , and model is printed by several extruders
    bool    one_extruder_printed_model = true;

    // extruder used for whole model for multi-extruder printer profile
    int     only_extruder = -1; 

    if (wxGetApp().extruders_edited_cnt() > 1)
    {
        const ModelObjectPtrs& objects = wxGetApp().plater()->model().objects;

        // check if whole model uses just only one extruder
        if (!objects.empty())
        {
            const int extruder = objects[0]->config.has("extruder") ?
                                 objects[0]->config.option("extruder")->getInt() : 0;

            auto is_one_extruder_printed_model = [objects, extruder]()
            {
                for (ModelObject* object : objects)
                {
                    if (object->config.has("extruder") &&
                        object->config.option("extruder")->getInt() != extruder)
                        return false;

                    if (object->volumes.size() > 1)
                        for (ModelVolume* volume : object->volumes)
                            if (volume->config.has("extruder") &&
                                volume->config.option("extruder")->getInt() != extruder)
                                return false;

                    for (const auto& range : object->layer_config_ranges)
                        if (range.second.has("extruder") &&
                            range.second.option("extruder")->getInt() != extruder)
                            return false;
                }
                return true;
            };

            if (is_one_extruder_printed_model())
                only_extruder = extruder;
            else
                one_extruder_printed_model = false;
        }
    }

#if ENABLE_GCODE_VIEWER
    m_layers_slider->SetModeAndOnlyExtruder(one_extruder_printed_model, only_extruder);
#else
    m_slider->SetModeAndOnlyExtruder(one_extruder_printed_model, only_extruder);
#endif // ENABLE_GCODE_VIEWER
}

#if ENABLE_GCODE_VIEWER
void Preview::reset_layers_slider()
{
    m_layers_slider->SetHigherValue(0);
    m_layers_slider->SetLowerValue(0);
}
#else
void Preview::reset_double_slider()
{
    m_slider->SetHigherValue(0);
    m_slider->SetLowerValue(0);
}
#endif // ENABLE_GCODE_VIEWER

#if ENABLE_GCODE_VIEWER
void Preview::update_layers_slider_from_canvas(wxKeyEvent& event)
{
    if (event.HasModifiers()) {
        event.Skip();
        return;
    }

    const auto key = event.GetKeyCode();

    if (key == 'U' || key == 'D') {
        const int new_pos = key == 'U' ? m_layers_slider->GetHigherValue() + 1 : m_layers_slider->GetHigherValue() - 1;
        m_layers_slider->SetHigherValue(new_pos);
        if (event.ShiftDown() || m_layers_slider->is_one_layer()) m_layers_slider->SetLowerValue(m_layers_slider->GetHigherValue());
    }
    else if (key == 'S')
        m_layers_slider->ChangeOneLayerLock();
    else if (key == WXK_SHIFT)
        m_layers_slider->UseDefaultColors(false);
    else
        event.Skip();
}

void Preview::update_moves_slider()
{
    const GCodeViewer::SequentialView& view = m_canvas->get_gcode_sequential_view();
    // this should not be needed, but it is here to try to prevent rambling crashes on Mac Asan
    if (view.endpoints.last < view.endpoints.first)
        return;

    std::vector<double> values(view.endpoints.last - view.endpoints.first + 1);
    unsigned int count = 0;
    for (unsigned int i = view.endpoints.first; i <= view.endpoints.last; ++i) {
        values[count++] = static_cast<double>(i + 1);
    }

    m_moves_slider->SetSliderValues(values);
    m_moves_slider->SetMaxValue(view.endpoints.last - view.endpoints.first);
    m_moves_slider->SetSelectionSpan(view.current.first - view.endpoints.first, view.current.last - view.endpoints.first);
}

void Preview::enable_moves_slider(bool enable)
{
    bool render_as_disabled = !enable;
    if (m_moves_slider != nullptr && m_moves_slider->is_rendering_as_disabled() != render_as_disabled) {
        m_moves_slider->set_render_as_disabled(render_as_disabled);
        m_moves_slider->Refresh();
    }
}
#else
void Preview::update_double_slider_from_canvas(wxKeyEvent & event)
{
    if (event.HasModifiers()) {
        event.Skip();
        return;
    }

    const auto key = event.GetKeyCode();

    if (key == 'U' || key == 'D') {
        const int new_pos = key == 'U' ? m_slider->GetHigherValue() + 1 : m_slider->GetHigherValue() - 1;
        m_slider->SetHigherValue(new_pos);
		if (event.ShiftDown() || m_slider->is_one_layer()) m_slider->SetLowerValue(m_slider->GetHigherValue());
    }
    else if (key == 'L') {
        m_checkbox_legend->SetValue(!m_checkbox_legend->GetValue());
        auto evt = wxCommandEvent();
        on_checkbox_legend(evt);
    }
    else if (key == 'S')
        m_slider->ChangeOneLayerLock();
    else if (key == WXK_SHIFT)
        m_slider->UseDefaultColors(false);
    else
        event.Skip();
}
#endif // ENABLE_GCODE_VIEWER

void Preview::load_print_as_fff(bool keep_z_range)
{
#if ENABLE_GCODE_VIEWER
    if (wxGetApp().mainframe == nullptr || wxGetApp().is_recreating_gui())
        // avoid processing while mainframe is being constructed
        return;
#endif // ENABLE_GCODE_VIEWER

    if (m_loaded || m_process->current_printer_technology() != ptFFF)
        return;

    // we require that there's at least one object and the posSlice step
    // is performed on all of them(this ensures that _shifted_copies was
    // populated and we know the number of layers)
    bool has_layers = false;
    const Print *print = m_process->fff_print();
    if (print->is_step_done(posSlice)) {
        for (const PrintObject* print_object : print->objects())
            if (! print_object->layers().empty()) {
                has_layers = true;
                break;
            }
    }
	if (print->is_step_done(posSupportMaterial)) {
        for (const PrintObject* print_object : print->objects())
            if (! print_object->support_layers().empty()) {
                has_layers = true;
                break;
            }
    }

#if ENABLE_GCODE_VIEWER
    if (wxGetApp().is_editor() && !has_layers) {
        hide_layers_slider();
        m_left_sizer->Hide(m_bottom_toolbar_panel);
        m_left_sizer->Layout();
        Refresh();
#else
    if (! has_layers) {
        reset_sliders(true);
        m_canvas->reset_legend_texture();
#endif // ENABLE_GCODE_VIEWER
        m_canvas_widget->Refresh();
        return;
    }

    if (m_preferred_color_mode == "tool_or_feature") {
        // It is left to Slic3r to decide whether the print shall be colored by the tool or by the feature.
        // Color by feature if it is a single extruder print.
        unsigned int number_extruders = (unsigned int)print->extruders().size();
        int tool_idx = m_choice_view_type->FindString(_L("Tool"));
        int type = (number_extruders > 1) ? tool_idx /* color by a tool number */ : 0; // color by a feature type
        m_choice_view_type->SetSelection(type);
#if ENABLE_GCODE_VIEWER
        if (0 <= type && type < static_cast<int>(GCodeViewer::EViewType::Count))
            m_canvas->set_gcode_view_preview_type(static_cast<GCodeViewer::EViewType>(type));
#else
        if ((0 <= type) && (type < (int)GCodePreviewData::Extrusion::Num_View_Types))
            m_gcode_preview_data->extrusion.view_type = (GCodePreviewData::Extrusion::EViewType)type;
#endif // ENABLE_GCODE_VIEWER
        // If the->SetSelection changed the following line, revert it to "decide yourself".
        m_preferred_color_mode = "tool_or_feature";
    }

#if ENABLE_GCODE_VIEWER
    GCodeViewer::EViewType gcode_view_type = m_canvas->get_gcode_view_preview_type();
    bool gcode_preview_data_valid = !m_gcode_result->moves.empty();
#else
    bool gcode_preview_data_valid = print->is_step_done(psGCodeExport) && ! m_gcode_preview_data->empty();
#endif // ENABLE_GCODE_VIEWER
    // Collect colors per extruder.
    std::vector<std::string> colors;
    std::vector<CustomGCode::Item> color_print_values = {};
    // set color print values, if it si selected "ColorPrint" view type
#if ENABLE_GCODE_VIEWER
    if (gcode_view_type == GCodeViewer::EViewType::ColorPrint) {
#else
    if (m_gcode_preview_data->extrusion.view_type == GCodePreviewData::Extrusion::ColorPrint) {
#endif // ENABLE_GCODE_VIEWER
        colors = wxGetApp().plater()->get_colors_for_color_print();
#if !ENABLE_GCODE_VIEWER
        colors.push_back("#808080"); // gray color for pause print or custom G-code 
#endif // !ENABLE_GCODE_VIEWER

        if (!gcode_preview_data_valid) {
            color_print_values = wxGetApp().plater()->model().custom_gcode_per_print_z.gcodes;
#if ENABLE_GCODE_VIEWER
            colors.push_back("#808080"); // gray color for pause print or custom G-code 
#endif // ENABLE_GCODE_VIEWER
        }
    }
#if ENABLE_GCODE_VIEWER
    else if (gcode_preview_data_valid || gcode_view_type == GCodeViewer::EViewType::Tool) {
#else
    else if (gcode_preview_data_valid || (m_gcode_preview_data->extrusion.view_type == GCodePreviewData::Extrusion::Tool) ) {
#endif // ENABLE_GCODE_VIEWER
        colors = wxGetApp().plater()->get_extruder_colors_from_plater_config();
        color_print_values.clear();
    }

    if (IsShown()) {
#if ENABLE_GCODE_VIEWER
        std::vector<double> zs;
#endif // ENABLE_GCODE_VIEWER

        m_canvas->set_selected_extruder(0);
        if (gcode_preview_data_valid) {
            // Load the real G-code preview.
#if ENABLE_GCODE_VIEWER
            m_canvas->load_gcode_preview(*m_gcode_result);
            m_canvas->refresh_gcode_preview(*m_gcode_result, colors);
            m_left_sizer->Show(m_bottom_toolbar_panel);
            m_left_sizer->Layout();
            Refresh();
            zs = m_canvas->get_gcode_layers_zs();
#else
            m_canvas->load_gcode_preview(*m_gcode_preview_data, colors);
#endif // ENABLE_GCODE_VIEWER
            m_loaded = true;
        } else {
            // Load the initial preview based on slices, not the final G-code.
            m_canvas->load_preview(colors, color_print_values);
#if ENABLE_GCODE_VIEWER
            m_left_sizer->Hide(m_bottom_toolbar_panel);
            m_left_sizer->Layout();
            Refresh();
            zs = m_canvas->get_volumes_print_zs(true);
#endif // ENABLE_GCODE_VIEWER
        }
#if !ENABLE_GCODE_VIEWER
        show_hide_ui_elements(gcode_preview_data_valid ? "full" : "simple");
        std::vector<double> zs = m_canvas->get_current_print_zs(true);
#endif // !ENABLE_GCODE_VIEWER
        if (zs.empty()) {
            // all layers filtered out
#if ENABLE_GCODE_VIEWER
            hide_layers_slider();
#else
            reset_sliders(true);
#endif // ENABLE_GCODE_VIEWER
            m_canvas_widget->Refresh();
        } else
#if ENABLE_GCODE_VIEWER
            update_layers_slider(zs, keep_z_range);
#else
            update_sliders(zs, keep_z_range);
#endif // ENABLE_GCODE_VIEWER
    }
}

void Preview::load_print_as_sla()
{
    if (m_loaded || (m_process->current_printer_technology() != ptSLA))
        return;

    unsigned int n_layers = 0;
    const SLAPrint* print = m_process->sla_print();

    std::vector<double> zs;
    double initial_layer_height = print->material_config().initial_layer_height.value;
    for (const SLAPrintObject* obj : print->objects())
        if (obj->is_step_done(slaposSliceSupports) && !obj->get_slice_index().empty())
        {
            auto low_coord = obj->get_slice_index().front().print_level();
            for (auto& rec : obj->get_slice_index())
                zs.emplace_back(initial_layer_height + (rec.print_level() - low_coord) * SCALING_FACTOR);
        }
    sort_remove_duplicates(zs);

    m_canvas->reset_clipping_planes_cache();

    n_layers = (unsigned int)zs.size();
    if (n_layers == 0)
    {
#if ENABLE_GCODE_VIEWER
        hide_layers_slider();
#else
        reset_sliders(true);
#endif // ENABLE_GCODE_VIEWER
        m_canvas_widget->Refresh();
    }

    if (IsShown())
    {
        m_canvas->load_sla_preview();
#if ENABLE_GCODE_VIEWER
        m_left_sizer->Hide(m_bottom_toolbar_panel);
        m_left_sizer->Hide(m_bottom_toolbar_panel);
        m_left_sizer->Layout();
        Refresh();
#else
        show_hide_ui_elements("none");
#endif // ENABLE_GCODE_VIEWER

        if (n_layers > 0)
#if ENABLE_GCODE_VIEWER
            update_layers_slider(zs);
#else
            update_sliders(zs);
#endif // ENABLE_GCODE_VIEWER

        m_loaded = true;
    }
}

#if ENABLE_GCODE_VIEWER
void Preview::on_layers_slider_scroll_changed(wxCommandEvent& event)
#else
void Preview::on_sliders_scroll_changed(wxCommandEvent& event)
#endif // ENABLE_GCODE_VIEWER
{
    if (IsShown())
    {
        PrinterTechnology tech = m_process->current_printer_technology();
        if (tech == ptFFF)
        {
#if ENABLE_GCODE_VIEWER
            m_canvas->set_toolpaths_z_range({ m_layers_slider->GetLowerValueD(), m_layers_slider->GetHigherValueD() });
            m_canvas->set_as_dirty();
#else
            m_canvas->set_toolpaths_range(m_slider->GetLowerValueD() - 1e-6, m_slider->GetHigherValueD() + 1e-6);
            m_canvas->render();
            m_canvas->set_use_clipping_planes(false);
#endif // ENABLE_GCODE_VIEWER
        }
        else if (tech == ptSLA)
        {
#if ENABLE_GCODE_VIEWER
            m_canvas->set_clipping_plane(0, ClippingPlane(Vec3d::UnitZ(), -m_layers_slider->GetLowerValueD()));
            m_canvas->set_clipping_plane(1, ClippingPlane(-Vec3d::UnitZ(), m_layers_slider->GetHigherValueD()));
            m_canvas->set_use_clipping_planes(m_layers_slider->GetHigherValue() != 0);
#else
            m_canvas->set_clipping_plane(0, ClippingPlane(Vec3d::UnitZ(), -m_slider->GetLowerValueD()));
            m_canvas->set_clipping_plane(1, ClippingPlane(-Vec3d::UnitZ(), m_slider->GetHigherValueD()));
            m_canvas->set_use_clipping_planes(m_slider->GetHigherValue() != 0);
#endif // ENABLE_GCODE_VIEWER
            m_canvas->render();
        }
    }
}

#if ENABLE_GCODE_VIEWER
void Preview::on_moves_slider_scroll_changed(wxCommandEvent& event)
{
    m_canvas->update_gcode_sequential_view_current(static_cast<unsigned int>(m_moves_slider->GetLowerValueD() - 1.0), static_cast<unsigned int>(m_moves_slider->GetHigherValueD() - 1.0));
    m_canvas->render();
}

wxString Preview::get_option_type_string(OptionType type) const
{
    switch (type)
    {
    case OptionType::Travel:        { return _L("Travel"); }
    case OptionType::Retractions:   { return _L("Retractions"); }
    case OptionType::Unretractions: { return _L("Deretractions"); }
    case OptionType::ToolChanges:   { return _L("Tool changes"); }
    case OptionType::ColorChanges:  { return _L("Color changes"); }
    case OptionType::PausePrints:   { return _L("Print pauses"); }
    case OptionType::CustomGCodes:  { return _L("Custom G-codes"); }
    case OptionType::Shells:        { return _L("Shells"); }
    case OptionType::ToolMarker:    { return _L("Tool marker"); }
    case OptionType::Legend:        { return _L("Legend/Estimated printing time"); }
    default:                        { return ""; }
    }
}
#endif // ENABLE_GCODE_VIEWER

} // namespace GUI
} // namespace Slic3r
