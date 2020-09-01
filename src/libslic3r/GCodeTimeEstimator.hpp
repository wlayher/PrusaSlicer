#ifndef slic3r_GCodeTimeEstimator_hpp_
#define slic3r_GCodeTimeEstimator_hpp_

#include "libslic3r.h"
#include "PrintConfig.hpp"
#include "GCodeReader.hpp"
#include "CustomGCode.hpp"

#if !ENABLE_GCODE_VIEWER

#define ENABLE_MOVE_STATS 0

namespace Slic3r {

    //
    // Some of the algorithms used by class GCodeTimeEstimator were inpired by
    // Cura Engine's class TimeEstimateCalculator
    // https://github.com/Ultimaker/CuraEngine/blob/master/src/timeEstimate.h
    //
    class GCodeTimeEstimator
    {
    public:
        static const std::string Normal_First_M73_Output_Placeholder_Tag;
        static const std::string Silent_First_M73_Output_Placeholder_Tag;
        static const std::string Normal_Last_M73_Output_Placeholder_Tag;
        static const std::string Silent_Last_M73_Output_Placeholder_Tag;

        static const std::string Color_Change_Tag;
        static const std::string Pause_Print_Tag;

        enum EMode : unsigned char
        {
            Normal,
            Silent
        };

        enum EUnits : unsigned char
        {
            Millimeters,
            Inches
        };

        enum EAxis : unsigned char
        {
            X,
            Y,
            Z,
            E,
            Num_Axis
        };

        enum EPositioningType : unsigned char
        {
            Absolute,
            Relative
        };

    private:
        struct Axis
        {
            float position;         // mm
            float origin;           // mm
            float max_feedrate;     // mm/s
            float max_acceleration; // mm/s^2
            float max_jerk;         // mm/s
        };

        struct Feedrates
        {
            float feedrate;                    // mm/s
            float axis_feedrate[Num_Axis];     // mm/s
            float abs_axis_feedrate[Num_Axis]; // mm/s
            float safe_feedrate;               // mm/s

            void reset();
        };

        struct State
        {
            GCodeFlavor dialect;
            EUnits units;
            EPositioningType global_positioning_type;
            EPositioningType e_local_positioning_type;
            Axis axis[Num_Axis];
            float feedrate;                     // mm/s
            float acceleration;                 // mm/s^2
            // hard limit for the acceleration, to which the firmware will clamp.
            float max_acceleration;             // mm/s^2
            float retract_acceleration;         // mm/s^2
            float minimum_feedrate;             // mm/s
            float minimum_travel_feedrate;      // mm/s
            float extrude_factor_override_percentage;
            // Additional load / unload times for a filament exchange sequence.
            std::vector<float> filament_load_times;
            std::vector<float> filament_unload_times;
            unsigned int g1_line_id;
            // extruder_id is currently used to correctly calculate filament load / unload times 
            // into the total print time. This is currently only really used by the MK3 MMU2:
            // Extruder id (-1) means no filament is loaded yet, all the filaments are parked in the MK3 MMU2 unit.
            static const unsigned int extruder_id_unloaded = (unsigned int)-1;
            unsigned int extruder_id;
        };

    public:
        struct Block
        {
#if ENABLE_MOVE_STATS
            enum EMoveType : unsigned char
            {
                Noop,
                Retract,
                Unretract,
                Tool_change,
                Move,
                Extrude,
                Num_Types
            };
#endif // ENABLE_MOVE_STATS

            struct FeedrateProfile
            {
                float entry;  // mm/s
                float cruise; // mm/s
                float exit;   // mm/s
            };

            struct Trapezoid
            {
                float accelerate_until; // mm
                float decelerate_after; // mm
                float cruise_feedrate; // mm/sec

                float acceleration_time(float entry_feedrate, float acceleration) const;
                float cruise_time() const;
                float deceleration_time(float distance, float acceleration) const;
                float cruise_distance() const;

                // This function gives the time needed to accelerate from an initial speed to reach a final distance.
                static float acceleration_time_from_distance(float initial_feedrate, float distance, float acceleration);

                // This function gives the final speed while accelerating at the given constant acceleration from the given initial speed along the given distance.
                static float speed_from_distance(float initial_feedrate, float distance, float acceleration);
            };

            struct Flags
            {
                bool recalculate;
                bool nominal_length;
            };

#if ENABLE_MOVE_STATS
            EMoveType move_type;
#endif // ENABLE_MOVE_STATS
            Flags flags;

            float distance; // mm
            float acceleration;        // mm/s^2
            float max_entry_speed;     // mm/s
            float safe_feedrate;       // mm/s

            FeedrateProfile feedrate;
            Trapezoid trapezoid;

            // Ordnary index of this G1 line in the file.
            int   g1_line_id { -1 };

            // Returns the time spent accelerating toward cruise speed, in seconds
            float acceleration_time() const;

            // Returns the time spent at cruise speed, in seconds
            float cruise_time() const;

            // Returns the time spent decelerating from cruise speed, in seconds
            float deceleration_time() const;

            // Returns the distance covered at cruise speed, in mm
            float cruise_distance() const;

            // Calculates this block's trapezoid
            void calculate_trapezoid();

            // Calculates the maximum allowable speed at this point when you must be able to reach target_velocity using the 
            // acceleration within the allotted distance.
            static float max_allowable_speed(float acceleration, float target_velocity, float distance);

            // Calculates the distance (not time) it takes to accelerate from initial_rate to target_rate using the given acceleration:
            static float estimate_acceleration_distance(float initial_rate, float target_rate, float acceleration);

            // This function gives you the point at which you must start braking (at the rate of -acceleration) if 
            // you started at speed initial_rate and accelerated until this point and want to end at the final_rate after
            // a total travel of distance. This can be used to compute the intersection point between acceleration and
            // deceleration in the cases where the trapezoid has no plateau (i.e. never reaches maximum speed)
            static float intersection_distance(float initial_rate, float final_rate, float acceleration, float distance);
        };

        typedef std::vector<Block> BlocksList;

#if ENABLE_MOVE_STATS
        struct MoveStats
        {
            unsigned int count;
            float time;

            MoveStats();
        };

        typedef std::map<Block::EMoveType, MoveStats> MovesStatsMap;
#endif // ENABLE_MOVE_STATS

    public:
        typedef std::pair<int, float> G1LineIdTime;
        typedef std::vector<G1LineIdTime> G1LineIdsTimes;

        struct PostProcessData
        {
            const G1LineIdsTimes& g1_times;
            float time;
        };

    private:
        EMode m_mode;
        GCodeReader m_parser;
        State m_state;
        Feedrates m_curr;
        Feedrates m_prev;
        BlocksList m_blocks;
        // Size of the firmware planner queue. The old 8-bit Marlins usually just managed 16 trapezoidal blocks.
        // Let's be conservative and plan for newer boards with more memory.
        static constexpr size_t planner_queue_size = 64;
        // The firmware recalculates last planner_queue_size trapezoidal blocks each time a new block is added.
        // We are not simulating the firmware exactly, we calculate a sequence of blocks once a reasonable number of blocks accumulate.
        static constexpr size_t planner_refresh_if_larger = planner_queue_size * 4;
        // Map from g1 line id to its elapsed time from the start of the print.
        G1LineIdsTimes m_g1_times;
        float m_time; // s

        // data to calculate custom code times
        bool m_needs_custom_gcode_times;
        std::vector<std::pair<CustomGCode::Type, float>> m_custom_gcode_times;
        float m_custom_gcode_time_cache;

#if ENABLE_MOVE_STATS
        MovesStatsMap _moves_stats;
#endif // ENABLE_MOVE_STATS

    public:
        explicit GCodeTimeEstimator(EMode mode);

        // Adds the given gcode line
        void add_gcode_line(const std::string& gcode_line);

        void add_gcode_block(const char *ptr);
        void add_gcode_block(const std::string &str) { this->add_gcode_block(str.c_str()); }

        // Calculates the time estimate from the gcode lines added using add_gcode_line() or add_gcode_block()
        // start_from_beginning:
        // if set to true all blocks will be used to calculate the time estimate,
        // if set to false only the blocks not yet processed will be used and the calculated time will be added to the current calculated time
        void calculate_time(bool start_from_beginning);

        // Calculates the time estimate from the given gcode in string format
        //void calculate_time_from_text(const std::string& gcode);

        // Calculates the time estimate from the gcode contained in the file with the given filename
        //void calculate_time_from_file(const std::string& file);

        // Calculates the time estimate from the gcode contained in given list of gcode lines
        //void calculate_time_from_lines(const std::vector<std::string>& gcode_lines);

        // Process the gcode contained in the file with the given filename, 
        // replacing placeholders with correspondent new lines M73
        // placing new lines M73 (containing the remaining time) where needed (in dependence of the given interval in seconds)
        // and removing working tags (as those used for color changes)
        // if normal_mode == nullptr no M73 line will be added for normal mode
        // if silent_mode == nullptr no M73 line will be added for silent mode
        static bool post_process(const std::string& filename, float interval_sec, const PostProcessData* const normal_mode, const PostProcessData* const silent_mode);

        // Set current position on the given axis with the given value
        void set_axis_position(EAxis axis, float position);
        // Set current origin on the given axis with the given value
        void set_axis_origin(EAxis axis, float position);

        void set_axis_max_feedrate(EAxis axis, float feedrate_mm_sec);
        void set_axis_max_acceleration(EAxis axis, float acceleration);
        void set_axis_max_jerk(EAxis axis, float jerk);

        // Returns current position on the given axis
        float get_axis_position(EAxis axis) const;
        // Returns current origin on the given axis
        float get_axis_origin(EAxis axis) const;

        float get_axis_max_feedrate(EAxis axis) const;
        float get_axis_max_acceleration(EAxis axis) const;
        float get_axis_max_jerk(EAxis axis) const;

        void set_feedrate(float feedrate_mm_sec);
        float get_feedrate() const;

        void set_acceleration(float acceleration_mm_sec2);
        float get_acceleration() const;

        // Maximum acceleration for the machine. The firmware simulator will clamp the M204 Sxxx to this maximum.
        void set_max_acceleration(float acceleration_mm_sec2);
        float get_max_acceleration() const;

        void set_retract_acceleration(float acceleration_mm_sec2);
        float get_retract_acceleration() const;

        void set_minimum_feedrate(float feedrate_mm_sec);
        float get_minimum_feedrate() const;

        void set_minimum_travel_feedrate(float feedrate_mm_sec);
        float get_minimum_travel_feedrate() const;

        void set_filament_load_times(const std::vector<double> &filament_load_times);
        void set_filament_unload_times(const std::vector<double> &filament_unload_times);
        float get_filament_load_time(unsigned int id_extruder);
        float get_filament_unload_time(unsigned int id_extruder);

        void set_extrude_factor_override_percentage(float percentage);
        float get_extrude_factor_override_percentage() const;

        void set_dialect(GCodeFlavor dialect);
        GCodeFlavor get_dialect() const;

        void set_units(EUnits units);
        EUnits get_units() const;

        void set_global_positioning_type(EPositioningType type);
        EPositioningType get_global_positioning_type() const;

        void set_e_local_positioning_type(EPositioningType type);
        EPositioningType get_e_local_positioning_type() const;

        int get_g1_line_id() const;
        void increment_g1_line_id();
        void reset_g1_line_id();

        void set_extrusion_axis(char axis) { m_parser.set_extrusion_axis(axis); }

        void set_extruder_id(unsigned int id);
        unsigned int get_extruder_id() const;
        void reset_extruder_id();

        void set_default();

        // Call this method before to start adding lines using add_gcode_line() when reusing an instance of GCodeTimeEstimator
        void reset();

        // Returns the estimated time, in seconds
        float get_time() const;

        // Returns the estimated time, in format DDd HHh MMm SSs
        std::string get_time_dhms() const;

        // Returns the estimated time, in format DDd HHh MMm
        std::string get_time_dhm() const;

        // Returns the estimated time, in minutes (integer)
        std::string get_time_minutes() const;

        // Returns the estimated time, in seconds, for each custom gcode
        std::vector<std::pair<CustomGCode::Type, float>> get_custom_gcode_times() const;

        // Returns the estimated time, in format DDd HHh MMm SSs, for each color
        // If include_remaining==true the strings will be formatted as: "time for color (remaining time at color start)"
        std::vector<std::string> get_color_times_dhms(bool include_remaining) const;

        // Returns the estimated time, in minutes (integer), for each color
        // If include_remaining==true the strings will be formatted as: "time for color (remaining time at color start)"
        std::vector<std::string> get_color_times_minutes(bool include_remaining) const;

        // Returns the estimated time, in format DDd HHh MMm, for each custom_gcode
        // If include_remaining==true the strings will be formatted as: "time for custom_gcode (remaining time at color start)"
        std::vector<std::pair<CustomGCode::Type, std::string>> get_custom_gcode_times_dhm(bool include_remaining) const;

        // Return an estimate of the memory consumed by the time estimator.
        size_t memory_used() const;

        PostProcessData get_post_process_data() const { return PostProcessData{ m_g1_times, m_time }; }

    private:
        void _reset();
        void _reset_time();
        void _reset_blocks();

        // Calculates the time estimate
        void _calculate_time(size_t keep_last_n_blocks);

        // Processes the given gcode line
        void _process_gcode_line(GCodeReader&, const GCodeReader::GCodeLine& line);

        // Move
        void _processG1(const GCodeReader::GCodeLine& line);

        // Dwell
        void _processG4(const GCodeReader::GCodeLine& line);

        // Set Units to Inches
        void _processG20(const GCodeReader::GCodeLine& line);

        // Set Units to Millimeters
        void _processG21(const GCodeReader::GCodeLine& line);

        // Move to Origin (Home)
        void _processG28(const GCodeReader::GCodeLine& line);

        // Set to Absolute Positioning
        void _processG90(const GCodeReader::GCodeLine& line);

        // Set to Relative Positioning
        void _processG91(const GCodeReader::GCodeLine& line);

        // Set Position
        void _processG92(const GCodeReader::GCodeLine& line);

        // Sleep or Conditional stop
        void _processM1(const GCodeReader::GCodeLine& line);

        // Set extruder to absolute mode
        void _processM82(const GCodeReader::GCodeLine& line);

        // Set extruder to relative mode
        void _processM83(const GCodeReader::GCodeLine& line);

        // Set Extruder Temperature and Wait
        void _processM109(const GCodeReader::GCodeLine& line);

        // Set max printing acceleration
        void _processM201(const GCodeReader::GCodeLine& line);

        // Set maximum feedrate
        void _processM203(const GCodeReader::GCodeLine& line);

        // Set default acceleration
        void _processM204(const GCodeReader::GCodeLine& line);

        // Advanced settings
        void _processM205(const GCodeReader::GCodeLine& line);

        // Set extrude factor override percentage
        void _processM221(const GCodeReader::GCodeLine& line);

        // Set allowable instantaneous speed change
        void _processM566(const GCodeReader::GCodeLine& line);

        // Unload the current filament into the MK3 MMU2 unit at the end of print.
        void _processM702(const GCodeReader::GCodeLine& line);

        // Processes T line (Select Tool)
        void _processT(const GCodeReader::GCodeLine& line);

        // Processes the tags
        // Returns true if any tag has been processed
        bool _process_tags(const GCodeReader::GCodeLine& line);

        // Processes ColorChangeTag and PausePrintTag
        void _process_custom_gcode_tag(CustomGCode::Type code);

        // Simulates firmware st_synchronize() call
        void _simulate_st_synchronize(float additional_time);

        void _forward_pass();
        void _reverse_pass();

        void _planner_forward_pass_kernel(Block& prev, Block& curr);
        void _planner_reverse_pass_kernel(Block& curr, Block& next);

        void _recalculate_trapezoids();

        // Returns the given time is seconds in format DDd HHh MMm SSs
        static std::string _get_time_dhms(float time_in_secs);
        // Returns the given time is minutes in format DDd HHh MMm
        static std::string _get_time_dhm(float time_in_secs);

        // Returns the given, in minutes (integer)
        static std::string _get_time_minutes(float time_in_secs);

#if ENABLE_MOVE_STATS
        void _log_moves_stats() const;
#endif // ENABLE_MOVE_STATS
    };

} /* namespace Slic3r */

#endif // !ENABLE_GCODE_VIEWER

#endif /* slic3r_GCodeTimeEstimator_hpp_ */
