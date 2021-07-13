#include "GalleryDialog.hpp"

#include <cstddef>
#include <vector>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include <wx/wupdlock.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "format.hpp"
#include "wxExtensions.hpp"
#include "I18N.hpp"
#include "Notebook.hpp"
#include "3DScene.hpp"
#include "GLCanvas3D.hpp"
#include "Plater.hpp"
#include "3DBed.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

namespace Slic3r {
namespace GUI {

#define BORDER_W    10
#define IMG_PX_CNT  64

namespace fs = boost::filesystem;

// Gallery::DropTarget
class GalleryDropTarget : public wxFileDropTarget
{
public:
    GalleryDropTarget(GalleryDialog* gallery_dlg) : gallery_dlg(gallery_dlg) { this->SetDefaultAction(wxDragCopy); }

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override;

private:
    GalleryDialog* gallery_dlg {nullptr};
};

bool GalleryDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
{
#ifdef WIN32
    // hides the system icon
    this->MSWUpdateDragImageOnLeave();
#endif // WIN32
    return gallery_dlg ? gallery_dlg->load_files(filenames) : false;
}


GalleryDialog::GalleryDialog(wxWindow* parent) :
    DPIDialog(parent, wxID_ANY, _L("Shapes Gallery"), wxDefaultPosition, wxSize(45 * wxGetApp().em_unit(), -1), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
#ifndef _WIN32
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif
    SetFont(wxGetApp().normal_font());

    wxStaticText* label_top = new wxStaticText(this, wxID_ANY, _L("Select shape from the gallery") + ":");

    m_list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(55 * wxGetApp().em_unit(), 35 * wxGetApp().em_unit()),
                                wxLC_ICON | wxLC_NO_HEADER | wxLC_ALIGN_TOP | wxSIMPLE_BORDER);
    m_list_ctrl->Bind(wxEVT_LIST_ITEM_SELECTED, &GalleryDialog::select, this);
    m_list_ctrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &GalleryDialog::deselect, this);
    m_list_ctrl->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
        m_selected_items.clear();
        select(event);
        this->EndModal(wxID_OK);
    });
    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        event.Skip();
        m_list_ctrl->Arrange();
    });

    wxStdDialogButtonSizer* buttons = this->CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    wxButton* ok_btn = static_cast<wxButton*>(FindWindowById(wxID_OK, this));
    ok_btn->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& evt) { evt.Enable(!m_selected_items.empty()); });

    auto add_btn = [this, buttons]( size_t pos, int& ID, wxString title, wxString tooltip,
                                    void (GalleryDialog::* method)(wxEvent&), 
                                    std::function<bool()> enable_fn = []() {return true; }) {
        ID = NewControlId();
        wxButton* btn = new wxButton(this, ID, title);
        btn->SetToolTip(tooltip);
        btn->Bind(wxEVT_UPDATE_UI, [enable_fn](wxUpdateUIEvent& evt) { evt.Enable(enable_fn()); });
        buttons->Insert(pos, btn, 0, wxRIGHT, BORDER_W);
        this->Bind(wxEVT_BUTTON, method, this, ID);
    };

    auto enable_del_fn = [this]() {
        if (m_selected_items.empty())
            return false;
        for (const Item& item : m_selected_items)
            if (item.is_system)
                return false;
        return true;
    };

    add_btn(0, ID_BTN_ADD_CUSTOM_SHAPE,   _L("Add"),         _L("Add one or more custom shapes"),                                       &GalleryDialog::add_custom_shapes);
    add_btn(1, ID_BTN_DEL_CUSTOM_SHAPE,   _L("Delete"),      _L("Delete one or more custom shape. You can't delete system shapes"),     &GalleryDialog::del_custom_shapes,  enable_del_fn);
    add_btn(2, ID_BTN_REPLACE_CUSTOM_PNG, _L("Replace PNG"), _L("Replace PNG for custom shape. You can't raplace PNG for system shape"),&GalleryDialog::replace_custom_png, [this]() { return (m_selected_items.size() == 1 && !m_selected_items[0].is_system); });
    buttons->InsertStretchSpacer(3, 2* BORDER_W);

    load_label_icon_list();

    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

    topSizer->Add(label_top , 0, wxEXPAND | wxLEFT | wxTOP | wxRIGHT, BORDER_W);
    topSizer->Add(m_list_ctrl, 1, wxEXPAND | wxLEFT | wxTOP | wxRIGHT, BORDER_W);
    topSizer->Add(buttons   , 0, wxEXPAND | wxALL, BORDER_W);

    SetSizer(topSizer);
    topSizer->SetSizeHints(this);

    wxGetApp().UpdateDlgDarkUI(this);
    this->CenterOnScreen();

    this->SetDropTarget(new GalleryDropTarget(this));
}

GalleryDialog::~GalleryDialog()
{   
}

void GalleryDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    const int& em = em_unit();

    msw_buttons_rescale(this, em, { ID_BTN_ADD_CUSTOM_SHAPE, ID_BTN_DEL_CUSTOM_SHAPE, ID_BTN_REPLACE_CUSTOM_PNG, wxID_OK, wxID_CANCEL });

    wxSize size = wxSize(55 * em, 35 * em);
    m_list_ctrl->SetMinSize(size);
    m_list_ctrl->SetSize(size);

    Fit();
    Refresh();
}

//static void add_border(wxImage& image)
//{
//    const wxColour& clr = wxGetApp().get_color_hovered_btn_label();

//    auto px_data = (uint8_t*)image.GetData();
//    auto a_data = (uint8_t*)image.GetAlpha();

//    int width = image.GetWidth();
//    int height = image.GetHeight();
//    int border_width = 1;

//    for (size_t x = 0; x < width; ++x) {
//        for (size_t y = 0; y < height; ++y) {
//            if (x < border_width || y < border_width ||
//                x >= (width - border_width) || y >= (height - border_width)) {
//                const size_t idx = (x + y * width);
//                const size_t idx_rgb = (x + y * width) * 3;
//                px_data[idx_rgb] = clr.Red();
//                px_data[idx_rgb + 1] = clr.Green();
//                px_data[idx_rgb + 2] = clr.Blue();
//                if (a_data)
//                    a_data[idx] = 255u;
//            }
//        }
//    }
//}

static void add_lock(wxImage& image) 
{
    wxBitmap bmp = create_scaled_bitmap("lock", nullptr, 22);

    wxImage lock_image = bmp.ConvertToImage();
    if (!lock_image.IsOk() || lock_image.GetWidth() == 0 || lock_image.GetHeight() == 0)
        return;

    int icon_sz = 16;
    auto lock_px_data = (uint8_t*)lock_image.GetData();
    auto lock_a_data = (uint8_t*)lock_image.GetAlpha();
    int lock_width  = lock_image.GetWidth();
    int lock_height = lock_image.GetHeight();
    
    auto px_data = (uint8_t*)image.GetData();
    auto a_data = (uint8_t*)image.GetAlpha();

    int width = image.GetWidth();
    int height = image.GetHeight();

    size_t beg_x = width - lock_width;
    size_t beg_y = height - lock_height;
    for (size_t x = 0; x < lock_width; ++x) {
        for (size_t y = 0; y < lock_height; ++y) {
            const size_t lock_idx = (x + y * lock_width);
            if (lock_a_data && lock_a_data[lock_idx] == 0)
                continue;

            const size_t idx = (beg_x + x + (beg_y + y) * width);
            if (a_data)
                a_data[idx] = lock_a_data[lock_idx];

            const size_t idx_rgb = (beg_x + x + (beg_y + y) * width) * 3;
            const size_t lock_idx_rgb = (x + y * lock_width) * 3;
            px_data[idx_rgb] = lock_px_data[lock_idx_rgb];
            px_data[idx_rgb + 1] = lock_px_data[lock_idx_rgb + 1];
            px_data[idx_rgb + 2] = lock_px_data[lock_idx_rgb + 2];
        }
    }
//    add_border(image);
}

static void add_default_image(wxImageList* img_list, bool is_system, std::string stl_path)
{
    wxBitmap bmp = create_scaled_bitmap("cog", nullptr, IMG_PX_CNT, true);    

    if (is_system) {
        wxImage image = bmp.ConvertToImage();
        if (image.IsOk() && image.GetWidth() != 0 && image.GetHeight() != 0) {
            add_lock(image);
            bmp = wxBitmap(std::move(image));
        }
    }
    img_list->Add(bmp);
};

static fs::path get_dir(bool sys_dir)
{
    return fs::absolute(fs::path(gallery_dir()) / (sys_dir ? "system" : "custom")).make_preferred();
}

static bool custom_exists() 
{
    return fs::exists(fs::absolute(fs::path(gallery_dir()) / "custom").make_preferred());
}

static std::string get_dir_path(bool sys_dir) 
{
    fs::path dir = get_dir(sys_dir);
#ifdef __WXMSW__
    return dir.string() + "\\";
#else
    return dir.string() + "/";
#endif
}

static void generate_thumbnail_from_stl(const std::string& filename)
{
    if (!boost::algorithm::iends_with(filename, ".stl")) {
        BOOST_LOG_TRIVIAL(error) << "Found invalid file type in generate_thumbnail_from_stl() [" << filename << "]";
        return;
    }

    Model model;
    try {
        model = Model::read_from_file(filename);
    }
    catch (std::exception&) {
        BOOST_LOG_TRIVIAL(error) << "Error loading model from " << filename << " in generate_thumbnail_from_stl()";
        return;
    }

    assert(model.objects.size() == 1);
    assert(model.objects[0]->volumes.size() == 1);
    assert(model.objects[0]->instances.size() == 1);

    model.objects[0]->center_around_origin(false);
    model.objects[0]->ensure_on_bed(false);

    const Vec3d bed_center_3d = wxGetApp().plater()->get_bed().get_bounding_box(false).center();
    const Vec2d bed_center_2d = { bed_center_3d.x(), bed_center_3d.y()};
    model.center_instances_around_point(bed_center_2d);

    GLVolumeCollection volumes;
    volumes.volumes.push_back(new GLVolume());
    GLVolume* volume = volumes.volumes[0];
    volume->indexed_vertex_array.load_mesh(model.mesh());
    volume->indexed_vertex_array.finalize_geometry(true);
    volume->set_instance_transformation(model.objects[0]->instances[0]->get_transformation());
    volume->set_volume_transformation(model.objects[0]->volumes[0]->get_transformation());

    ThumbnailData thumbnail_data;
    const ThumbnailsParams thumbnail_params = { {}, false, false, false, true };
    wxGetApp().plater()->canvas3D()->render_thumbnail(thumbnail_data, 256, 256, thumbnail_params, volumes, Camera::EType::Perspective);

    if (thumbnail_data.width == 0 || thumbnail_data.height == 0)
        return;

    wxImage image(thumbnail_data.width, thumbnail_data.height);
    image.InitAlpha();

    for (unsigned int r = 0; r < thumbnail_data.height; ++r) {
        unsigned int rr = (thumbnail_data.height - 1 - r) * thumbnail_data.width;
        for (unsigned int c = 0; c < thumbnail_data.width; ++c) {
            unsigned char* px = (unsigned char*)thumbnail_data.pixels.data() + 4 * (rr + c);
            image.SetRGB((int)c, (int)r, px[0], px[1], px[2]);
            image.SetAlpha((int)c, (int)r, px[3]);
        }
    }

    fs::path out_path = fs::path(filename);
    out_path.replace_extension("png");
    image.SaveFile(out_path.string(), wxBITMAP_TYPE_PNG);
}

void GalleryDialog::load_label_icon_list()
{
    // load names from files
    auto add_files_from_gallery = [](std::vector<Item>& items, bool sys_dir, std::string& dir_path)
    {
        fs::path dir = get_dir(sys_dir);
        dir_path = get_dir_path(sys_dir);

        for (auto& dir_entry : fs::directory_iterator(dir))
            if (is_stl_file(dir_entry)) {
                std::string name = dir_entry.path().stem().string();
                Item item = Item{ name, sys_dir };
                items.push_back(item);
            }
    };

    wxBusyCursor busy;

    std::string m_sys_dir_path, m_cust_dir_path;
    std::vector<Item> list_items;
    add_files_from_gallery(list_items, true, m_sys_dir_path);
    if (custom_exists())
        add_files_from_gallery(list_items, false, m_cust_dir_path);

    // Make an image list containing large icons

    int px_cnt = (int)(em_unit() * IMG_PX_CNT * 0.1f + 0.5f);
    m_image_list = new wxImageList(px_cnt, px_cnt);

    std::string ext = ".png";

    for (const auto& item : list_items) {
        std::string img_name = (item.is_system ? m_sys_dir_path : m_cust_dir_path) + item.name + ext;
        std::string stl_name = (item.is_system ? m_sys_dir_path : m_cust_dir_path) + item.name + ".stl";
        if (!fs::exists(img_name))
            generate_thumbnail_from_stl(stl_name);

        wxImage image;
        if (!image.LoadFile(from_u8(img_name), wxBITMAP_TYPE_PNG) ||
            image.GetWidth() == 0 || image.GetHeight() == 0) {
            add_default_image(m_image_list, item.is_system, stl_name);
            continue;
        }
        image.Rescale(px_cnt, px_cnt, wxIMAGE_QUALITY_BILINEAR);

        if (item.is_system)
            add_lock(image);
        wxBitmap bmp = wxBitmap(std::move(image));
        m_image_list->Add(bmp);
    }

    m_list_ctrl->SetImageList(m_image_list, wxIMAGE_LIST_NORMAL);

    int img_cnt = m_image_list->GetImageCount();
    for (int i = 0; i < img_cnt; i++) {
        m_list_ctrl->InsertItem(i, from_u8(list_items[i].name), i);
        if (list_items[i].is_system)
            m_list_ctrl->SetItemFont(i, m_list_ctrl->GetItemFont(i).Bold());
    }
}

void GalleryDialog::get_input_files(wxArrayString& input_files)
{
    for (const Item& item : m_selected_items)
        input_files.Add(from_u8(get_dir_path(item.is_system) + item.name + ".stl"));
}

void GalleryDialog::add_custom_shapes(wxEvent& event)
{
    wxArrayString input_files;
    wxFileDialog dialog(this, _L("Choose one or more files (STL):"),
        from_u8(wxGetApp().app_config->get_last_dir()), "",
        file_wildcards(FT_STL), wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK)
        dialog.GetPaths(input_files);

    if (input_files.IsEmpty())
        return;

    load_files(input_files);
}

void GalleryDialog::del_custom_shapes(wxEvent& event)
{
    auto dest_dir = get_dir(false);

    for (const Item& item : m_selected_items) {
        std::string filename = item.name + ".stl";

        if (!fs::exists(dest_dir / filename))
            continue;
        try {
            fs::remove(dest_dir / filename);
        }
        catch (fs::filesystem_error const& e) {
            std::cerr << e.what() << '\n';
            return;
        }
    }

    update();
}

void GalleryDialog::replace_custom_png(wxEvent& event)
{
    if (m_selected_items.size() != 1 || m_selected_items[0].is_system)
        return;

    wxFileDialog dialog(this, _L("Choose one PNG file:"),
                        from_u8(wxGetApp().app_config->get_last_dir()), "",
                        "PNG files (*.png)|*.png;*.PNG", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    wxArrayString input_files;
    dialog.GetPaths(input_files);

    if (input_files.IsEmpty())
        return;

    try {
        fs::path current = fs::path(into_u8(input_files.Item(0)));
        fs::copy_file(current, get_dir(false) / (m_selected_items[0].name + ".png"));
    }
    catch (fs::filesystem_error const& e) {
        std::cerr << e.what() << '\n';
        return;
    }

    update();
}

void GalleryDialog::select(wxListEvent& event)
{
    int idx = event.GetIndex();
    Item item { into_u8(m_list_ctrl->GetItemText(idx)), m_list_ctrl->GetItemFont(idx).GetWeight() == wxFONTWEIGHT_BOLD };

    m_selected_items.push_back(item);
}

void GalleryDialog::deselect(wxListEvent& event)
{
    if (m_list_ctrl->GetSelectedItemCount() == 0) {
        m_selected_items.clear();
        return;
    }

    std::string name = into_u8(m_list_ctrl->GetItemText(event.GetIndex()));
    m_selected_items.erase(std::remove_if(m_selected_items.begin(), m_selected_items.end(), [name](Item item) { return item.name == name; }));
}

void GalleryDialog::update()
{
    m_selected_items.clear();
    m_image_list->RemoveAll();
    m_list_ctrl->ClearAll();
    load_label_icon_list();
}

bool GalleryDialog::load_files(const wxArrayString& input_files)
{
    auto dest_dir = get_dir(false);

    try {
        if (!fs::exists(dest_dir))
            if (!fs::create_directory(dest_dir)) {
                std::cerr << "Unable to create destination directory" << dest_dir.string() << '\n' ;
                return false;
            }
    }
    catch (fs::filesystem_error const& e) {
        std::cerr << e.what() << '\n';
        return false;
    }

    // Iterate through the source directory
    for (size_t i = 0; i < input_files.size(); ++i) {
        std::string input_file = into_u8(input_files.Item(i));

        try {
            fs::path current = fs::path(input_file);
            if (!fs::exists(dest_dir / current.filename()))
                fs::copy_file(current, dest_dir / current.filename());
            else {
                std::string filename = current.stem().string();

                int file_idx = 0;
                for (auto& dir_entry : fs::directory_iterator(dest_dir))
                    if (is_stl_file(dir_entry)) {
                        std::string name = dir_entry.path().stem().string();
                        if (filename == name) {
                            if (file_idx == 0)
                                file_idx++;
                            continue;
                        }
                        
                        if (name.find(filename) != 0 ||
                            name[filename.size()] != ' ' || name[filename.size()+1] != '(' || name[name.size()-1] != ')')
                            continue;
                        std::string idx_str = name.substr(filename.size() + 2, name.size() - filename.size() - 3);                        
                        if (int cur_idx = atoi(idx_str.c_str()); file_idx <= cur_idx)
                            file_idx = cur_idx+1;
                    }
                if (file_idx > 0) {
                    filename += " (" + std::to_string(file_idx) + ").stl";
                    fs::copy_file(current, dest_dir / filename);
                }
            }
        }
        catch (fs::filesystem_error const& e) {
            std::cerr << e.what() << '\n';
            return false;
        }
    }

    update();
    return true;
}

}}    // namespace Slic3r::GUI
