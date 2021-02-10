#ifndef slic3r_GLGizmoCut_hpp_
#define slic3r_GLGizmoCut_hpp_

#include "GLGizmoBase.hpp"


namespace Slic3r {
namespace GUI {

class GLGizmoCut : public GLGizmoBase
{
    static const double Offset;
    static const double Margin;
    static const std::array<float, 4> GrabberColor;

    mutable double m_cut_z;
    double m_start_z;
    mutable double m_max_z;
    Vec3d m_drag_pos;
    Vec3d m_drag_center;
    bool m_keep_upper;
    bool m_keep_lower;
    bool m_rotate_lower;

public:
    GLGizmoCut(GLCanvas3D& parent, const std::string& icon_filename, unsigned int sprite_id);

    double get_cut_z() const { return m_cut_z; }
    void set_cut_z(double cut_z) const;

    std::string get_tooltip() const override;

protected:
    virtual bool on_init() override;
    virtual void on_load(cereal::BinaryInputArchive& ar)  override{ ar(m_cut_z, m_keep_upper, m_keep_lower, m_rotate_lower); }
    virtual void on_save(cereal::BinaryOutputArchive& ar) const override { ar(m_cut_z, m_keep_upper, m_keep_lower, m_rotate_lower); }
    virtual std::string on_get_name() const override;
    virtual void on_set_state() override;
    virtual bool on_is_activable() const override;
    virtual void on_start_dragging() override;
    virtual void on_update(const UpdateData& data) override;
    virtual void on_render() const override;
    virtual void on_render_for_picking() const override;
    virtual void on_render_input_window(float x, float y, float bottom_limit) override;

private:
    void update_max_z(const Selection& selection) const;
    void perform_cut(const Selection& selection);
    double calc_projection(const Linef3& mouse_ray) const;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GLGizmoCut_hpp_
