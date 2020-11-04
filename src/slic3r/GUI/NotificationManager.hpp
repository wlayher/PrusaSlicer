#ifndef slic3r_GUI_NotificationManager_hpp_
#define slic3r_GUI_NotificationManager_hpp_

#include "Event.hpp"
#include "I18N.hpp"

#include <libslic3r/ObjectID.hpp>

#include <string>
#include <vector>
#include <deque>
#include <unordered_set>

namespace Slic3r {
namespace GUI {

using EjectDriveNotificationClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_EJECT_DRIVE_NOTIFICAION_CLICKED, EjectDriveNotificationClickedEvent);
using ExportGcodeNotificationClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_EXPORT_GCODE_NOTIFICAION_CLICKED, ExportGcodeNotificationClickedEvent);
using PresetUpdateAvailableClickedEvent = SimpleEvent;
wxDECLARE_EVENT(EVT_PRESET_UPDATE_AVAILABLE_CLICKED, PresetUpdateAvailableClickedEvent);

class GLCanvas3D;
class ImGuiWrapper;

enum class NotificationType
{
	CustomNotification,
	// Notification on end of slicing and G-code processing (the full G-code preview is available).
	// Contains a hyperlink to export the G-code to a removable media.
	SlicingComplete,
//	SlicingNotPossible,
	// Notification on end of export to a removable media, with hyperling to eject the external media.
	// Obsolete by ExportFinished
//	ExportToRemovableFinished,
	// Notification on end of export, with hyperling to see folder and eject if export was to external media.
	// Own subclass.
	ExportFinished,
	// Works on OSX only.
	//FIXME Do we want to have it on Linux and Windows? Is it possible to get the Disconnect event on Windows?
	Mouse3dDisconnected,
//	Mouse3dConnected,
//	NewPresetsAviable,
	// Notification on the start of PrusaSlicer, when a new PrusaSlicer version is published.
	// Contains a hyperlink to open a web browser pointing to the PrusaSlicer download location.
	NewAppAvailable,
	// Notification on the start of PrusaSlicer, when updates of system profiles are detected.
	// Contains a hyperlink to execute installation of the new system profiles.
	PresetUpdateAvailable,
//	LoadingFailed,
	// Not used - instead Slicing error is used for both slicing and validate errors.
//	ValidateError,
	// Slicing error produced by BackgroundSlicingProcess::validate() or by the BackgroundSlicingProcess background
	// thread thowing a SlicingError exception.
	SlicingError,
	// Slicing warnings, issued by the slicing process.
	// Slicing warnings are registered for a particular Print milestone or a PrintObject and its milestone.
	SlicingWarning,
	// Object partially outside the print volume. Cannot slice.
	PlaterError,
	// Object fully outside the print volume, or extrusion outside the print volume. Slicing is not disabled.
	PlaterWarning
};

class NotificationManager
{
public:
	enum class NotificationLevel : int
	{
		// The notifications will be presented in the order of importance, thus these enum values
		// are sorted by the importance.
		// "Good to know" notification, usually but not always with a quick fade-out.
		RegularNotification = 1,
		// Information notification without a fade-out or with a longer fade-out.
		ImportantNotification,
		// Warning, no fade-out.
		WarningNotification,
		// Error, no fade-out.
		ErrorNotification,
	};

	NotificationManager(wxEvtHandler* evt_handler);
	
	// Push a prefabricated notification from basic_notifications (see the table at the end of this file).
	void push_notification(const NotificationType type, GLCanvas3D& canvas, int timestamp = 0);
	// Push a NotificationType::CustomNotification with NotificationLevel::RegularNotification and 10s fade out interval.
	void push_notification(const std::string& text, GLCanvas3D& canvas, int timestamp = 0);
	// Push a NotificationType::CustomNotification with provided notification level and 10s for RegularNotification.
	// ErrorNotification and ImportantNotification are never faded out.
	void push_notification(const std::string& text, NotificationLevel level, GLCanvas3D& canvas, int timestamp = 0);
	// Creates Slicing Error notification with a custom text and no fade out.
	void push_slicing_error_notification(const std::string& text, GLCanvas3D& canvas);
	// Creates Slicing Warning notification with a custom text and no fade out.
	void push_slicing_warning_notification(const std::string& text, bool gray, GLCanvas3D& canvas, ObjectID oid, int warning_step);
	// marks slicing errors as gray
	void set_all_slicing_errors_gray(bool g);
	// marks slicing warings as gray
	void set_all_slicing_warnings_gray(bool g);
//	void set_slicing_warning_gray(const std::string& text, bool g);
	// immediately stops showing slicing errors
	void close_slicing_errors_and_warnings();
	// Release those slicing warnings, which refer to an ObjectID, which is not in the list.
	// living_oids is expected to be sorted.
	void remove_slicing_warnings_of_released_objects(const std::vector<ObjectID>& living_oids);
	// Object partially outside of the printer working space, cannot print. No fade out.
	void push_plater_error_notification(const std::string& text, GLCanvas3D& canvas);
	// Object fully out of the printer working space and such. No fade out.
	void push_plater_warning_notification(const std::string& text, GLCanvas3D& canvas);
	// Closes error or warning of the same text
	void close_plater_error_notification(const std::string& text);
	void close_plater_warning_notification(const std::string& text);
	// Creates special notification slicing complete.
	// If large = true (Plater side bar is closed), then printing time and export button is shown
	// at the notification and fade-out is disabled. Otherwise the fade out time is set to 10s.
	void push_slicing_complete_notification(GLCanvas3D& canvas, int timestamp, bool large);
	// Add a print time estimate to an existing SlicingComplete notification.
	void set_slicing_complete_print_time(const std::string &info);
	// Called when the side bar changes its visibility, as the "slicing complete" notification supplements
	// the "slicing info" normally shown at the side bar.
	void set_slicing_complete_large(bool large);
	// Exporting finished, show this information with path, button to open containing folder and if ejectable - eject button
	void push_exporting_finished_notification(GLCanvas3D& canvas, std::string path, std::string dir_path, bool on_removable);
    // Close old notification ExportFinished.
	void new_export_began(bool on_removable);
	// finds ExportFinished notification and closes it if it was to removable device
	void device_ejected();
	// renders notifications in queue and deletes expired ones
	void render_notifications(GLCanvas3D& canvas, float overlay_width);
	// finds and closes all notifications of given type
	void close_notification_of_type(const NotificationType type);
	// Which view is active? Plater or G-code preview? Hide warnings in G-code preview.
    void set_in_preview(bool preview);
	// Move to left to avoid colision with variable layer height gizmo.
	void set_move_from_overlay(bool move) { m_move_from_overlay = move; }
	
private:
	// duration 0 means not disapearing
	struct NotificationData {
		NotificationType    type;
		NotificationLevel   level;
		// Fade out time
		const int           duration;
		const std::string   text1;
		const std::string   hypertext;
		const std::string   text2;
	};

	// Cache of IDs to identify and reuse ImGUI windows.
	class NotificationIDProvider
	{
	public:
		int 		allocate_id();
		void 		release_id(int id);

	private:
		// Next ID used for naming the ImGUI windows.
		int       			m_next_id{ 1 };
		// IDs of ImGUI windows, which were released and they are ready for reuse.
		std::vector<int>	m_released_ids;
	};

	//Pop notification - shows only once to user.
	class PopNotification
	{
	public:
		enum class RenderResult
		{
			Finished,
			ClosePending,
			Static,
			Countdown,
			Hovered
		};
		PopNotification(const NotificationData &n, NotificationIDProvider &id_provider, wxEvtHandler* evt_handler);
		virtual ~PopNotification() { if (m_id) m_id_provider.release_id(m_id); }
		RenderResult           render(GLCanvas3D& canvas, const float& initial_y, bool move_from_overlay, float overlay_width);
		// close will dissapear notification on next render
		void                   close() { m_close_pending = true; }
		// data from newer notification of same type
		void                   update(const NotificationData& n);
		bool                   get_finished() const { return m_finished || m_close_pending; }
		// returns top after movement
		float                  get_top() const { return m_top_y; }
		//returns top in actual frame
		float                  get_current_top() const { return m_top_y; }
		const NotificationType get_type() const { return m_data.type; }
		const NotificationData get_data() const { return m_data;  }
		const bool             get_is_gray() const { return m_is_gray; }
		// Call equals one second down
		void                   substract_remaining_time() { m_remaining_time--; }
		void                   set_gray(bool g) { m_is_gray = g; }
		void                   set_paused(bool p) { m_paused = p; }
		bool                   compare_text(const std::string& text);
        void                   hide(bool h) { m_hidden = h; }

	protected:
		// Call after every size change
		void         init();
		// Part of init() 
		virtual void count_spaces();
		// Calculetes correct size but not se it in imgui!
		virtual void set_next_window_size(ImGuiWrapper& imgui);
		virtual void render_text(ImGuiWrapper& imgui,
			                     const float win_size_x, const float win_size_y,
			                     const float win_pos_x , const float win_pos_y);
		virtual void render_close_button(ImGuiWrapper& imgui,
			                             const float win_size_x, const float win_size_y,
			                             const float win_pos_x , const float win_pos_y);
		void         render_countdown(ImGuiWrapper& imgui,
			                          const float win_size_x, const float win_size_y,
			                          const float win_pos_x , const float win_pos_y);
		virtual void render_hypertext(ImGuiWrapper& imgui,
			                          const float text_x, const float text_y,
		                              const std::string text,
		                              bool more = false);
		// Left sign could be error or warning sign
		void         render_left_sign(ImGuiWrapper& imgui);
		virtual void render_minimize_button(ImGuiWrapper& imgui,
			                                const float win_pos_x, const float win_pos_y);
		// Hypertext action, returns if close notification
		virtual bool on_text_click();

		const NotificationData m_data;

		// For reusing ImGUI windows.
		NotificationIDProvider &m_id_provider;
		int              m_id { 0 };
		bool			 m_initialized          { false };
		// Main text
		std::string      m_text1;
		// Clickable text
		std::string      m_hypertext;
		// Aditional text after hypertext - currently not used
		std::string      m_text2;
		// Countdown variables
		long             m_remaining_time;
		bool             m_counting_down;
		long             m_last_remaining_time;
		bool             m_paused               { false };
		int              m_countdown_frame      { 0 };
		bool             m_fading_out           { false };
		// total time left when fading beggins
		float            m_fading_time          { 0.0f }; 
		float            m_current_fade_opacity { 1.f };
		// If hidden the notif is alive but not visible to user
		bool             m_hidden               { false };
		//  m_finished = true - does not render, marked to delete
		bool             m_finished             { false }; 
		// Will go to m_finished next render
		bool             m_close_pending        { false }; 
		// variables to count positions correctly
		// all space without text
		float            m_window_width_offset;
		// Space on left side without text
		float            m_left_indentation;
		// Total size of notification window - varies based on monitor
		float            m_window_height        { 56.0f };  
		float            m_window_width         { 450.0f };
		//Distance from bottom of notifications to top of this notification
		float            m_top_y                { 0.0f };  
		
		// Height of text
		// Used as basic scaling unit!
		float            m_line_height;
		std::vector<int> m_endlines;
		// Gray are f.e. eorrors when its uknown if they are still valid
		bool             m_is_gray              { false };
		//if multiline = true, notification is showing all lines(>2)
		bool             m_multiline            { false };
		// True if minimized button is rendered, helps to decide where is area for invisible close button
		bool             m_minimize_b_visible   { false };
		int              m_lines_count{ 1 };
	    // Target for wxWidgets events sent by clicking on the hyperlink available at some notifications.
		wxEvtHandler*    m_evt_handler;
	};

	class SlicingCompleteLargeNotification : public PopNotification
	{
	public:
		SlicingCompleteLargeNotification(const NotificationData& n, NotificationIDProvider& id_provider, wxEvtHandler* evt_handler, bool largeds);
		void set_large(bool l);
		bool get_large() { return m_is_large; }

		void set_print_info(const std::string &info);
	protected:
		virtual void render_text(ImGuiWrapper& imgui,
			                     const float win_size_x, const float win_size_y,
			                     const float win_pos_x, const float win_pos_y) 
			                     override;
		bool        m_is_large;
		bool        m_has_print_info { false };
		std::string m_print_info { std::string() };
	};

	class SlicingWarningNotification : public PopNotification
	{
	public:
		SlicingWarningNotification(const NotificationData& n, NotificationIDProvider& id_provider, wxEvtHandler* evt_handler) : PopNotification(n, id_provider, evt_handler) {}
		ObjectID 	object_id;
		int    		warning_step;
	};

	class ExportFinishedNotification : public PopNotification
	{
	public:
		ExportFinishedNotification(const NotificationData& n, NotificationIDProvider& id_provider, wxEvtHandler* evt_handler, bool to_removable,const std::string& export_path,const std::string& export_dir_path)
			: PopNotification(n, id_provider, evt_handler)
			, m_to_removable(to_removable)
			, m_export_path(export_path)
			, m_export_dir_path(export_dir_path)
		    {
				m_multiline = true;
			}
		bool        m_to_removable;
		std::string m_export_path;
		std::string m_export_dir_path;
	protected:
		// Reserves space on right for more buttons
		virtual void count_spaces() override;
		virtual void render_text(ImGuiWrapper& imgui,
			                     const float win_size_x, const float win_size_y,
			                     const float win_pos_x, const float win_pos_y) override;
		// Renders also button to open directory with exported path and eject removable media
		virtual void render_close_button(ImGuiWrapper& imgui,
			                             const float win_size_x, const float win_size_y,
			                             const float win_pos_x, const float win_pos_y) override;
		void         render_eject_button(ImGuiWrapper& imgui,
			                             const float win_size_x, const float win_size_y,
			                             const float win_pos_x, const float win_pos_y);
		virtual void render_minimize_button(ImGuiWrapper& imgui, const float win_pos_x, const float win_pos_y) override 
			{ m_minimize_b_visible = false; }
		virtual bool on_text_click() override; 
		// local time of last hover for showing tooltip
		long      m_hover_time { 0 };
	};

	//pushes notification into the queue of notifications that are rendered
	//can be used to create custom notification
	bool push_notification_data(const NotificationData& notification_data, GLCanvas3D& canvas, int timestamp);
	bool push_notification_data(std::unique_ptr<NotificationManager::PopNotification> notification, GLCanvas3D& canvas, int timestamp);
	//finds older notification of same type and moves it to the end of queue. returns true if found
	bool activate_existing(const NotificationManager::PopNotification* notification);
	// Put the more important notifications to the bottom of the list.
	void sort_notifications();
	// If there is some error notification active, then the "Export G-code" notification after the slicing is finished is suppressed.
    bool has_slicing_error_notification();

    // Target for wxWidgets events sent by clicking on the hyperlink available at some notifications.
	wxEvtHandler*                m_evt_handler;
	// Cache of IDs to identify and reuse ImGUI windows.
	NotificationIDProvider 		 m_id_provider;
	std::deque<std::unique_ptr<PopNotification>> m_pop_notifications;
	// Last render time in seconds for fade out control.
	long                         m_last_time { 0 };
	// When mouse hovers over some notification, the fade-out of all notifications is suppressed.
	bool                         m_hovered { false };
	//timestamps used for slicing finished - notification could be gone so it needs to be stored here
	std::unordered_set<int>      m_used_timestamps;
	// True if G-code preview is active. False if the Plater is active.
	bool                         m_in_preview { false };
	// True if the layer editing is enabled in Plater, so that the notifications are shifted left of it.
	bool                         m_move_from_overlay { false };

	//prepared (basic) notifications
	const std::vector<NotificationData> basic_notifications = {
//		{NotificationType::SlicingNotPossible, NotificationLevel::RegularNotification, 10,  _u8L("Slicing is not possible.")},
//		{NotificationType::ExportToRemovableFinished, NotificationLevel::ImportantNotification, 0,  _u8L("Exporting finished."),  _u8L("Eject drive.") },
		{NotificationType::Mouse3dDisconnected, NotificationLevel::RegularNotification, 10,  _u8L("3D Mouse disconnected.") },
//		{NotificationType::Mouse3dConnected, NotificationLevel::RegularNotification, 5,  _u8L("3D Mouse connected.") },
//		{NotificationType::NewPresetsAviable, NotificationLevel::ImportantNotification, 20,  _u8L("New Presets are available."),  _u8L("See here.") },
		{NotificationType::PresetUpdateAvailable, NotificationLevel::ImportantNotification, 20,  _u8L("Configuration update is available."),  _u8L("See more.")},
		{NotificationType::NewAppAvailable, NotificationLevel::ImportantNotification, 20,  _u8L("New version is available."),  _u8L("See Releases page.")},
		//{NotificationType::NewAppAvailable, NotificationLevel::ImportantNotification, 20,  _u8L("New vesion of PrusaSlicer is available.",  _u8L("Download page.") },
		//{NotificationType::LoadingFailed, NotificationLevel::RegularNotification, 20,  _u8L("Loading of model has Failed") },
		//{NotificationType::DeviceEjected, NotificationLevel::RegularNotification, 10,  _u8L("Removable device has been safely ejected")} // if we want changeble text (like here name of device), we need to do it as CustomNotification
	};
};

}//namespace GUI
}//namespace Slic3r

#endif //slic3r_GUI_NotificationManager_hpp_