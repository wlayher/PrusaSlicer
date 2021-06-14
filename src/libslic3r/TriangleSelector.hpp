#ifndef libslic3r_TriangleSelector_hpp_
#define libslic3r_TriangleSelector_hpp_

// #define PRUSASLICER_TRIANGLE_SELECTOR_DEBUG


#include "Point.hpp"
#include "TriangleMesh.hpp"

namespace Slic3r {

enum class EnforcerBlockerType : int8_t;



// Following class holds information about selected triangles. It also has power
// to recursively subdivide the triangles and make the selection finer.
class TriangleSelector {
public:
    enum CursorType {
        CIRCLE,
        SPHERE
    };

    // Set a limit to the edge length, below which the edge will not be split by select_patch().
    // Called by select_patch() internally. Made public for debugging purposes, see TriangleSelectorGUI::render_debug().
    void set_edge_limit(float edge_limit);

    // Create new object on a TriangleMesh. The referenced mesh must
    // stay valid, a ptr to it is saved and used.
    explicit TriangleSelector(const TriangleMesh& mesh);

    // Select all triangles fully inside the circle, subdivide where needed.
    void select_patch(const Vec3f        &hit,         // point where to start
                      int                 facet_start, // facet of the original mesh (unsplit) that the hit point belongs to
                      const Vec3f        &source,      // camera position (mesh coords)
                      float               radius,      // radius of the cursor
                      CursorType          type,        // current type of cursor
                      EnforcerBlockerType new_state,   // enforcer or blocker?
                      const Transform3d  &trafo,       // matrix to get from mesh to world
                      bool                triangle_splitting); // If triangles will be split base on the cursor or not

    void seed_fill_select_triangles(const Vec3f &hit,               // point where to start
                                    int          facet_start,       // facet of the original mesh (unsplit) that the hit point belongs to
                                    float        seed_fill_angle);  // the maximal angle between two facets to be painted by the same color

    // Get facets currently in the given state.
    indexed_triangle_set get_facets(EnforcerBlockerType state) const;

    // Set facet of the mesh to a given state. Only works for original triangles.
    void set_facet(int facet_idx, EnforcerBlockerType state);

    // Clear everything and make the tree empty.
    void reset(const EnforcerBlockerType reset_state = EnforcerBlockerType{0});

    // Remove all unnecessary data.
    void garbage_collect();

    // Store the division trees in compact form (a long stream of bits for each triangle of the original mesh).
    // First vector contains pairs of (triangle index, first bit in the second vector).
    std::pair<std::vector<std::pair<int, int>>, std::vector<bool>> serialize() const;

    // Load serialized data. Assumes that correct mesh is loaded.
    void deserialize(const std::pair<std::vector<std::pair<int, int>>, std::vector<bool>> &data, const EnforcerBlockerType init_state = EnforcerBlockerType{0});

    // For all triangles, remove the flag indicating that the triangle was selected by seed fill.
    void seed_fill_unselect_all_triangles();

    // For all triangles selected by seed fill, set new EnforcerBlockerType and remove flag indicating that triangle was selected by seed fill.
    // The operation may merge split triangles if they are being assigned the same color.
    void seed_fill_apply_on_triangles(EnforcerBlockerType new_state);

protected:
    // Triangle and info about how it's split.
    class Triangle {
    public:
        // Use TriangleSelector::push_triangle to create a new triangle.
        // It increments/decrements reference counter on vertices.
        Triangle(int a, int b, int c, int source_triangle, const EnforcerBlockerType init_state)
            : verts_idxs{a, b, c},
              source_triangle{source_triangle},
              state{init_state}
        {
            // Initialize bit fields. Default member initializers are not supported by C++17.
            m_selected_by_seed_fill = false;
            m_valid = true;
            old_number_of_splits = 0;
        }
        // Indices into m_vertices.
        std::array<int, 3> verts_idxs;

        // Index of the source triangle at the initial (unsplit) mesh.
        int source_triangle;

        // Children triangles.
        std::array<int, 4> children;

        // Set the division type.
        void set_division(int sides_to_split, int special_side_idx = -1);

        // Get/set current state.
        void set_state(EnforcerBlockerType type) { assert(! is_split()); state = type; }
        EnforcerBlockerType get_state() const { assert(! is_split()); return state; }

        // Set if the triangle has been selected or unselected by seed fill.
        void select_by_seed_fill() { assert(! is_split()); m_selected_by_seed_fill = true; }
        void unselect_by_seed_fill() { assert(! is_split()); m_selected_by_seed_fill = false; }
        // Get if the triangle has been selected or not by seed fill.
        bool is_selected_by_seed_fill() const { assert(! is_split()); return m_selected_by_seed_fill; }

        // Is this triangle valid or marked to be removed?
        bool valid() const throw() { return m_valid; }
        // Get info on how it's split.
        bool is_split() const throw() { return number_of_split_sides() != 0; }
        int number_of_split_sides() const throw() { return number_of_splits; }
        int special_side() const throw() { assert(is_split()); return special_side_idx; }
        bool was_split_before() const throw() { return old_number_of_splits != 0; }
        void forget_history() { old_number_of_splits = 0; }

    private:
        friend TriangleSelector;

        // Packing the rest of member variables into 4 bytes, aligned to 4 bytes boundary.
        char number_of_splits { 0 };
        // Index of a vertex opposite to the split edge (for number_of_splits == 1)
        // or index of a vertex shared by the two split edges (for number_of_splits == 2).
        // For number_of_splits == 3, special_side_idx is always zero.
        char special_side_idx { 0 };
        EnforcerBlockerType state;
        bool m_selected_by_seed_fill : 1;
        // Is this triangle valid or marked to be removed?
        bool m_valid : 1;

        // How many children were spawned during last split?
        // Is not reset on remerging the triangle.
        int old_number_of_splits : 3;

        // there are still 3 bits available at the last byte :-)
    };

    struct Vertex {
        explicit Vertex(const stl_vertex& vert)
            : v{vert},
              ref_cnt{0}
        {}
        stl_vertex v;
        int ref_cnt;
    };

    // Lists of vertices and triangles, both original and new
    std::vector<Vertex> m_vertices;
    std::vector<Triangle> m_triangles;
    const TriangleMesh* m_mesh;

    // Number of invalid triangles (to trigger garbage collection).
    int m_invalid_triangles;

    // Limiting length of triangle side (squared).
    float m_edge_limit_sqr = 1.f;

    // Number of original vertices and triangles.
    int m_orig_size_vertices = 0;
    int m_orig_size_indices = 0;

    // Cache for cursor position, radius and direction.
    struct Cursor {
        Cursor() = default;
        Cursor(const Vec3f& center_, const Vec3f& source_, float radius_world,
               CursorType type_, const Transform3d& trafo_);
        bool is_mesh_point_inside(Vec3f pt) const;
        bool is_pointer_in_triangle(const Vec3f& p1, const Vec3f& p2, const Vec3f& p3) const;

        Vec3f center;
        Vec3f source;
        Vec3f dir;
        float radius_sqr;
        CursorType type;
        Transform3f trafo;
        Transform3f trafo_normal;
        bool uniform_scaling;
    };

    Cursor m_cursor;
    float m_old_cursor_radius_sqr;

    // Private functions:
private:
    bool select_triangle(int facet_idx, EnforcerBlockerType type, bool recursive_call = false, bool triangle_splitting = true);
    int  vertices_inside(int facet_idx) const;
    bool faces_camera(int facet) const;
    void undivide_triangle(int facet_idx);
    void split_triangle(int facet_idx);
    void remove_useless_children(int facet_idx); // No hidden meaning. Triangles are meant.
    bool is_pointer_in_triangle(int facet_idx) const;
    bool is_edge_inside_cursor(int facet_idx) const;
    void push_triangle(int a, int b, int c, int source_triangle, const EnforcerBlockerType state = EnforcerBlockerType{0});
    void perform_split(int facet_idx, EnforcerBlockerType old_state);
};




} // namespace Slic3r

#endif // libslic3r_TriangleSelector_hpp_
