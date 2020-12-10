#include "libslic3r/libslic3r.h"
#include "GLShadersManager.hpp"
#include "3DScene.hpp"
#include "GUI_App.hpp"

#include <cassert>
#include <algorithm>
#include <string_view>
using namespace std::literals;

#include <GL/glew.h>

namespace Slic3r {

std::pair<bool, std::string> GLShadersManager::init()
{
    std::string error;

    auto append_shader = [this, &error](const std::string& name, const GLShaderProgram::ShaderFilenames& filenames, 
        const std::initializer_list<std::string_view> &defines = {}) {
        m_shaders.push_back(std::make_unique<GLShaderProgram>());
        if (!m_shaders.back()->init_from_files(name, filenames, defines)) {
            error += name + "\n";
            // if any error happens while initializating the shader, we remove it from the list
            m_shaders.pop_back();
            return false;
        }
        return true;
    };

    assert(m_shaders.empty());

    bool valid = true;

    // used to render bed axes and model, selection hints, gcode sequential view marker model, preview shells
    valid &= append_shader("gouraud_light", { "gouraud_light.vs", "gouraud_light.fs" });
    // used to render printbed
    valid &= append_shader("printbed", { "printbed.vs", "printbed.fs" });
    // used to render options in gcode preview
    valid &= append_shader("options_110", { "options_110.vs", "options_110.fs" });
    if (GUI::wxGetApp().is_glsl_version_greater_or_equal_to(1, 20))
        valid &= append_shader("options_120", { "options_120.vs", "options_120.fs" });
    // used to render extrusion and travel paths as lines in gcode preview
    valid &= append_shader("toolpaths_lines", { "toolpaths_lines.vs", "toolpaths_lines.fs" });
    // used to render objects in 3d editor
    valid &= append_shader("gouraud", { "gouraud.vs", "gouraud.fs" }
#if ENABLE_ENVIRONMENT_MAP
        , { "ENABLE_ENVIRONMENT_MAP"sv }
#endif
        );
    // used to render variable layers heights in 3d editor
    valid &= append_shader("variable_layer_height", { "variable_layer_height.vs", "variable_layer_height.fs" });

    return { valid, error };
}

void GLShadersManager::shutdown()
{
    m_shaders.clear();
}

GLShaderProgram* GLShadersManager::get_shader(const std::string& shader_name)
{
    auto it = std::find_if(m_shaders.begin(), m_shaders.end(), [&shader_name](std::unique_ptr<GLShaderProgram>& p) { return p->get_name() == shader_name; });
    return (it != m_shaders.end()) ? it->get() : nullptr;
}

GLShaderProgram* GLShadersManager::get_current_shader()
{
    GLint id = 0;
    glsafe(::glGetIntegerv(GL_CURRENT_PROGRAM, &id));
    if (id == 0)
        return nullptr;

    auto it = std::find_if(m_shaders.begin(), m_shaders.end(), [id](std::unique_ptr<GLShaderProgram>& p) { return static_cast<GLint>(p->get_id()) == id; });
    return (it != m_shaders.end()) ? it->get() : nullptr;
}

} // namespace Slic3r

