#ifndef _prusaslicer_technologies_h_
#define _prusaslicer_technologies_h_

//=============
// debug techs
//=============
// Shows camera target in the 3D scene
#define ENABLE_SHOW_CAMERA_TARGET 0
// Log debug messages to console when changing selection
#define ENABLE_SELECTION_DEBUG_OUTPUT 0
// Renders a small sphere in the center of the bounding box of the current selection when no gizmo is active
#define ENABLE_RENDER_SELECTION_CENTER 0
// Shows an imgui dialog with render related data
#define ENABLE_RENDER_STATISTICS 0
// Shows an imgui dialog with camera related data
#define ENABLE_CAMERA_STATISTICS 0
// Render the picking pass instead of the main scene (use [T] key to toggle between regular rendering and picking pass only rendering)
#define ENABLE_RENDER_PICKING_PASS 0
// Enable extracting thumbnails from selected gcode and save them as png files
#define ENABLE_THUMBNAIL_GENERATOR_DEBUG 0
// Disable synchronization of unselected instances
#define DISABLE_INSTANCES_SYNCH 0
// Use wxDataViewRender instead of wxDataViewCustomRenderer
#define ENABLE_NONCUSTOM_DATA_VIEW_RENDERING 0
// Enable G-Code viewer statistics imgui dialog
#define ENABLE_GCODE_VIEWER_STATISTICS 0
// Enable G-Code viewer comparison between toolpaths height and width detected from gcode and calculated at gcode generation 
#define ENABLE_GCODE_VIEWER_DATA_CHECKING 0


// Enable rendering of objects using environment map
#define ENABLE_ENVIRONMENT_MAP 0
// Enable smoothing of objects normals
#define ENABLE_SMOOTH_NORMALS 0
// Enable rendering markers for options in preview as fixed screen size points
#define ENABLE_FIXED_SCREEN_SIZE_POINT_MARKERS 1


//====================
// 2.3.1.alpha1 techs
//====================
#define ENABLE_2_3_1_ALPHA1 1

// Enable splitting of vertex buffers used to render toolpaths
#define ENABLE_SPLITTED_VERTEX_BUFFER (1 && ENABLE_2_3_1_ALPHA1)
// Enable rendering only starting and final caps for toolpaths
#define ENABLE_REDUCED_TOOLPATHS_SEGMENT_CAPS (1 && ENABLE_SPLITTED_VERTEX_BUFFER)
// Enable reload from disk command for 3mf files
#define ENABLE_RELOAD_FROM_DISK_FOR_3MF (1 && ENABLE_2_3_1_ALPHA1)
// Removes obsolete warning texture code
#define ENABLE_WARNING_TEXTURE_REMOVAL (1 && ENABLE_2_3_1_ALPHA1)
// Enable showing gcode line numbers in previeww horizontal slider
#define ENABLE_GCODE_LINES_ID_IN_H_SLIDER (1 && ENABLE_2_3_1_ALPHA1)
// Enable validation of custom gcode against gcode processor reserved keywords
#define ENABLE_VALIDATE_CUSTOM_GCODE (1 && ENABLE_2_3_1_ALPHA1)


#endif // _prusaslicer_technologies_h_
