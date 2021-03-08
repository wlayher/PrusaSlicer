#ifndef SLA_HOLLOWING_HPP
#define SLA_HOLLOWING_HPP

#include <memory>
#include <libslic3r/SLA/Contour3D.hpp>
#include <libslic3r/SLA/JobController.hpp>

namespace Slic3r {

class TriangleMesh;

namespace sla {

struct HollowingConfig
{
    double min_thickness    = 2.;
    double quality          = 0.5;
    double closing_distance = 0.5;
    bool enabled = true;
};

enum HollowingFlags { hfRemoveInsideTriangles = 0x1 };

// All data related to a generated mesh interior. Includes the 3D grid and mesh
// and various metadata. No need to manipulate from outside.
struct Interior;
struct InteriorDeleter { void operator()(Interior *p); };
using  InteriorPtr = std::unique_ptr<Interior, InteriorDeleter>;

TriangleMesh &      get_mesh(Interior &interior);
const TriangleMesh &get_mesh(const Interior &interior);

struct DrainHole
{
    Vec3f pos;
    Vec3f normal;
    float radius;
    float height;

    DrainHole()
        : pos(Vec3f::Zero()), normal(Vec3f::UnitZ()), radius(5.f), height(10.f)
    {}

    DrainHole(Vec3f p, Vec3f n, float r, float h)
        : pos(p), normal(n), radius(r), height(h)
    {}

    DrainHole(const DrainHole& rhs) :
        DrainHole(rhs.pos, rhs.normal, rhs.radius, rhs.height) {}
    
    bool operator==(const DrainHole &sp) const;
    
    bool operator!=(const DrainHole &sp) const { return !(sp == (*this)); }

    bool is_inside(const Vec3f& pt) const;

    bool get_intersections(const Vec3f& s, const Vec3f& dir,
                           std::array<std::pair<float, Vec3d>, 2>& out) const;
    
    Contour3D to_mesh() const;
    
    template<class Archive> inline void serialize(Archive &ar)
    {
        ar(pos, normal, radius, height);
    }

    static constexpr size_t steps = 32;
};

using DrainHoles = std::vector<DrainHole>;

constexpr float HoleStickOutLength = 1.f;

InteriorPtr generate_interior(const TriangleMesh &mesh,
                              const HollowingConfig &  = {},
                              const JobController &ctl = {});

// Will do the hollowing
void hollow_mesh(TriangleMesh &mesh, const HollowingConfig &cfg, int flags = 0);

// Hollowing prepared in "interior", merge with original mesh
void hollow_mesh(TriangleMesh &mesh, const Interior &interior, int flags = 0);

void remove_inside_triangles(TriangleMesh &mesh, const Interior &interior,
                             const std::vector<bool> &exclude_mask = {});

double get_distance(const Vec3f &p, const Interior &interior);

template<class T>
FloatingOnly<T> get_distance(const Vec<3, T> &p, const Interior &interior)
{
    return get_distance(Vec3f(p.template cast<float>()), interior);
}

void cut_drainholes(std::vector<ExPolygons> & obj_slices,
                    const std::vector<float> &slicegrid,
                    float                     closing_radius,
                    const sla::DrainHoles &   holes,
                    std::function<void(void)> thr);

}
}

#endif // HOLLOWINGFILTER_H
