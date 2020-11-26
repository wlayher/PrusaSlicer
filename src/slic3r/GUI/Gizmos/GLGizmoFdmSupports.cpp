#include "GLGizmoFdmSupports.hpp"

#include "libslic3r/Model.hpp"

//#include "slic3r/GUI/3DScene.hpp"
#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/ImGuiWrapper.hpp"
#include "slic3r/GUI/Plater.hpp"


#include <GL/glew.h>


namespace Slic3r {

namespace GUI {



void GLGizmoFdmSupports::on_shutdown()
{
    if (m_setting_angle) {
        m_setting_angle = false;
        m_parent.use_slope(false);
    }
}



std::string GLGizmoFdmSupports::on_get_name() const
{
    return (_L("Paint-on supports") + " [L]").ToUTF8().data();
}



bool GLGizmoFdmSupports::on_init()
{
    m_shortcut_key = WXK_CONTROL_L;

    m_desc["clipping_of_view"] = _L("Clipping of view") + ": ";
    m_desc["reset_direction"]  = _L("Reset direction");
    m_desc["cursor_size"]      = _L("Brush size") + ": ";
    m_desc["cursor_type"]      = _L("Brush shape") + ": ";
    m_desc["enforce_caption"]  = _L("Left mouse button") + ": ";
    m_desc["enforce"]          = _L("Enforce supports");
    m_desc["block_caption"]    = _L("Right mouse button") + ": ";
    m_desc["block"]            = _L("Block supports");
    m_desc["remove_caption"]   = _L("Shift + Left mouse button") + ": ";
    m_desc["remove"]           = _L("Remove selection");
    m_desc["remove_all"]       = _L("Remove all selection");
    m_desc["circle"]           = _L("Circle");
    m_desc["sphere"]           = _L("Sphere");

    return true;
}



void GLGizmoFdmSupports::render_painter_gizmo() const
{
    const Selection& selection = m_parent.get_selection();

    glsafe(::glEnable(GL_BLEND));
    glsafe(::glEnable(GL_DEPTH_TEST));

    if (! m_setting_angle)
        render_triangles(selection);

    m_c->object_clipper()->render_cut();
    render_cursor();

    glsafe(::glDisable(GL_BLEND));
}



void GLGizmoFdmSupports::on_render_input_window(float x, float y, float bottom_limit)
{
    if (! m_c->selection_info()->model_object())
        return;

    const float approx_height = m_imgui->scaled(14.0f);
    y = std::min(y, bottom_limit - approx_height);
    m_imgui->set_next_window_pos(x, y, ImGuiCond_Always);

    if (! m_setting_angle) {
        m_imgui->begin(on_get_name(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

        // First calculate width of all the texts that are could possibly be shown. We will decide set the dialog width based on that:
        const float clipping_slider_left = std::max(m_imgui->calc_text_size(m_desc.at("clipping_of_view")).x,
                                                    m_imgui->calc_text_size(m_desc.at("reset_direction")).x)
                                              + m_imgui->scaled(1.5f);
        const float cursor_slider_left = m_imgui->calc_text_size(m_desc.at("cursor_size")).x + m_imgui->scaled(1.f);
        const float cursor_type_radio_left  = m_imgui->calc_text_size(m_desc.at("cursor_type")).x + m_imgui->scaled(1.f);
        const float cursor_type_radio_width1 = m_imgui->calc_text_size(m_desc["circle"]).x
                                                 + m_imgui->scaled(2.5f);
        const float cursor_type_radio_width2 = m_imgui->calc_text_size(m_desc["sphere"]).x
                                                 + m_imgui->scaled(2.5f);
        const float button_width = m_imgui->calc_text_size(m_desc.at("remove_all")).x + m_imgui->scaled(1.f);
        const float minimal_slider_width = m_imgui->scaled(4.f);

        float caption_max = 0.f;
        float total_text_max = 0.;
        for (const std::string& t : {"enforce", "block", "remove"}) {
            caption_max = std::max(caption_max, m_imgui->calc_text_size(m_desc.at(t+"_caption")).x);
            total_text_max = std::max(total_text_max, caption_max + m_imgui->calc_text_size(m_desc.at(t)).x);
        }
        caption_max += m_imgui->scaled(1.f);
        total_text_max += m_imgui->scaled(1.f);

        float window_width = minimal_slider_width + std::max(cursor_slider_left, clipping_slider_left);
        window_width = std::max(window_width, total_text_max);
        window_width = std::max(window_width, button_width);
        window_width = std::max(window_width, cursor_type_radio_left + cursor_type_radio_width1 + cursor_type_radio_width2);

        auto draw_text_with_caption = [this, &caption_max](const wxString& caption, const wxString& text) {
            m_imgui->text_colored(ImGuiWrapper::COL_ORANGE_LIGHT, caption);
            ImGui::SameLine(caption_max);
            m_imgui->text(text);
        };

        for (const std::string& t : {"enforce", "block", "remove"})
            draw_text_with_caption(m_desc.at(t + "_caption"), m_desc.at(t));

        m_imgui->text("");

        if (m_imgui->button(_L("Autoset by angle") + "...")) {
            m_setting_angle = true;
        }

        ImGui::SameLine();

        if (m_imgui->button(m_desc.at("remove_all"))) {
            Plater::TakeSnapshot(wxGetApp().plater(), wxString(_L("Reset selection")));
            ModelObject* mo = m_c->selection_info()->model_object();
            int idx = -1;
            for (ModelVolume* mv : mo->volumes) {
                if (mv->is_model_part()) {
                    ++idx;
                    m_triangle_selectors[idx]->reset();
                }
            }

            update_model_object();
            m_parent.set_as_dirty();
        }

        const float max_tooltip_width = ImGui::GetFontSize() * 20.0f;

        ImGui::AlignTextToFramePadding();
        m_imgui->text(m_desc.at("cursor_size"));
        ImGui::SameLine(cursor_slider_left);
        ImGui::PushItemWidth(window_width - cursor_slider_left);
        ImGui::SliderFloat(" ", &m_cursor_radius, CursorRadiusMin, CursorRadiusMax, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(max_tooltip_width);
            ImGui::TextUnformatted(_L("Alt + Mouse wheel").ToUTF8().data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }


        ImGui::AlignTextToFramePadding();
        m_imgui->text(m_desc.at("cursor_type"));
        ImGui::SameLine(cursor_type_radio_left + m_imgui->scaled(0.f));
        ImGui::PushItemWidth(cursor_type_radio_width1);

        bool sphere_sel = m_cursor_type == TriangleSelector::CursorType::SPHERE;
        if (m_imgui->radio_button(m_desc["sphere"], sphere_sel))
            sphere_sel = true;

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(max_tooltip_width);
            ImGui::TextUnformatted(_L("Paints all facets inside, regardless of their orientation.").ToUTF8().data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::SameLine(cursor_type_radio_left + cursor_type_radio_width2 + m_imgui->scaled(0.f));
        ImGui::PushItemWidth(cursor_type_radio_width2);

        if (m_imgui->radio_button(m_desc["circle"], ! sphere_sel))
            sphere_sel = false;

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(max_tooltip_width);
            ImGui::TextUnformatted(_L("Ignores facets facing away from the camera.").ToUTF8().data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        m_cursor_type = sphere_sel
                ? TriangleSelector::CursorType::SPHERE
                : TriangleSelector::CursorType::CIRCLE;




        ImGui::Separator();
        if (m_c->object_clipper()->get_position() == 0.f) {
            ImGui::AlignTextToFramePadding();
            m_imgui->text(m_desc.at("clipping_of_view"));
        }
        else {
            if (m_imgui->button(m_desc.at("reset_direction"))) {
                wxGetApp().CallAfter([this](){
                        m_c->object_clipper()->set_position(-1., false);
                    });
            }
        }

        ImGui::SameLine(clipping_slider_left);
        ImGui::PushItemWidth(window_width - clipping_slider_left);
        float clp_dist = m_c->object_clipper()->get_position();
        if (ImGui::SliderFloat("  ", &clp_dist, 0.f, 1.f, "%.2f"))
        m_c->object_clipper()->set_position(clp_dist, true);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(max_tooltip_width);
            ImGui::TextUnformatted(_L("Ctrl + Mouse wheel").ToUTF8().data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        m_imgui->end();
    }
    else {
        m_imgui->begin(_L("Autoset custom supports"), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::AlignTextToFramePadding();
        m_imgui->text(_L("Threshold:"));
        std::string format_str = std::string("%.f") + I18N::translate_utf8("°",
            "Degree sign to use in the respective slider in FDM supports gizmo,"
            "placed after the number with no whitespace in between.");
        ImGui::SameLine();
        if (m_imgui->slider_float("", &m_angle_threshold_deg, 0.f, 90.f, format_str.data()))
            m_parent.set_slope_normal_angle(90.f - m_angle_threshold_deg);
        if (m_imgui->button(_L("Enforce")))
            select_facets_by_angle(m_angle_threshold_deg, false);
        ImGui::SameLine();
        if (m_imgui->button(_L("Block")))
            select_facets_by_angle(m_angle_threshold_deg, true);
        ImGui::SameLine();
        if (m_imgui->button(_L("Cancel")))
            m_setting_angle = false;
        m_imgui->end();
        bool needs_update = !(m_setting_angle && m_parent.is_using_slope());
        if (needs_update) {
            m_parent.use_slope(m_setting_angle);
            m_parent.set_as_dirty();
        }
    }
}



void GLGizmoFdmSupports::select_facets_by_angle(float threshold_deg, bool block)
{
    float threshold = (M_PI/180.)*threshold_deg;
    const Selection& selection = m_parent.get_selection();
    const ModelObject* mo = m_c->selection_info()->model_object();
    const ModelInstance* mi = mo->instances[selection.get_instance_idx()];

    int mesh_id = -1;
    for (const ModelVolume* mv : mo->volumes) {
        if (! mv->is_model_part())
            continue;

        ++mesh_id;

        const Transform3d trafo_matrix = mi->get_matrix(true) * mv->get_matrix(true);
        Vec3f down  = (trafo_matrix.inverse() * (-Vec3d::UnitZ())).cast<float>().normalized();
        Vec3f limit = (trafo_matrix.inverse() * Vec3d(std::sin(threshold), 0, -std::cos(threshold))).cast<float>().normalized();

        float dot_limit = limit.dot(down);

        // Now calculate dot product of vert_direction and facets' normals.
        int idx = -1;
        for (const stl_facet& facet : mv->mesh().stl.facet_start) {
            ++idx;
            if (facet.normal.dot(down) > dot_limit)
                m_triangle_selectors[mesh_id]->set_facet(idx,
                                                         block
                                                         ? EnforcerBlockerType::BLOCKER
                                                         : EnforcerBlockerType::ENFORCER);
        }
    }

    activate_internal_undo_redo_stack(true);

    Plater::TakeSnapshot(wxGetApp().plater(), block ? _L("Block supports by angle")
                                                    : _L("Add supports by angle"));
    update_model_object();
    m_parent.set_as_dirty();
    m_setting_angle = false;
}



void GLGizmoFdmSupports::update_model_object() const
{
    bool updated = false;
    ModelObject* mo = m_c->selection_info()->model_object();
    int idx = -1;
    for (ModelVolume* mv : mo->volumes) {
        if (! mv->is_model_part())
            continue;
        ++idx;
        updated |= mv->supported_facets.set(*m_triangle_selectors[idx].get());
    }

    if (updated)
        m_parent.post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));
}



void GLGizmoFdmSupports::update_from_model_object()
{
    wxBusyCursor wait;

    const ModelObject* mo = m_c->selection_info()->model_object();
    m_triangle_selectors.clear();

    int volume_id = -1;
    for (const ModelVolume* mv : mo->volumes) {
        if (! mv->is_model_part())
            continue;

        ++volume_id;

        // This mesh does not account for the possible Z up SLA offset.
        const TriangleMesh* mesh = &mv->mesh();

        m_triangle_selectors.emplace_back(std::make_unique<TriangleSelectorGUI>(*mesh));
        m_triangle_selectors.back()->deserialize(mv->supported_facets.get_data());
    }
}



PainterGizmoType GLGizmoFdmSupports::get_painter_type() const
{
    return PainterGizmoType::FDM_SUPPORTS;
}


} // namespace GUI
} // namespace Slic3r
