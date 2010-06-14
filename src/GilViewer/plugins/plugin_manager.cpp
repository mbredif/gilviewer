#include "plugin_manager.hpp"
#include "../gui/PanelManager.h"
#include "../gui/define_id.hpp"

#include "../convenient/utils.hpp"

#include <boost/filesystem.hpp>

using namespace std;

plugin_base* plugin_manager_model::create_object(const string& id)
{
    plugin_base* plugin = PatternFactory<plugin_base>::create_object(id);
    if(plugin)
        m_plugins.push_back(plugin);
    return plugin;
}

void plugin_manager_model::register_plugins(const std::string &path)
{
    vector<string> all_so_files = filesystem_utils::all_files_from_path(path, ".so");

    // Add a menu plugin if it does not exist
    panel_viewer* first_panel = panel_manager::instance()->panels_list()[0];
    wxMenuBar* menus = first_panel->menubar();
    int index_plugins_menu = menus->FindMenu(wxString("Plugins", *wxConvCurrent));
    wxMenu* plugins_menu;
    if(index_plugins_menu==wxNOT_FOUND)
    {
        plugins_menu = new wxMenu;
        menus->Insert(menus->GetMenuCount(), plugins_menu, _("Plugins"));
        //test->Append(wxID_ANY, _("Plugins"));
    }
    else
    {
        plugins_menu = menus->GetMenu(index_plugins_menu);
    }

    unsigned int nb_of_successfully_loaded_plugins = 0;
    for(unsigned int i=0;i<all_so_files.size();++i)
    {
        boost::filesystem::path file_path(boost::filesystem::system_complete(all_so_files[i]));
        if(file_path.filename()!="libGilViewer.so")
        {
            plugin_base::Register(file_path.filename(), file_path.parent_path().string());
            plugin_base *p = plugin_manager::instance()->create_object(file_path.filename());
            //p->parent(first_panel);
            if(p)
            {
                p->process();
                cout << "dflkjdflkgjfdlgkjfd" << endl;
                plugins_menu->Append(FIRST_GILVIEWER_PLUGIN+nb_of_successfully_loaded_plugins, wxString(p->menuentry_name().c_str(), *wxConvCurrent));
                menus->GetParent()->Connect(FIRST_GILVIEWER_PLUGIN+nb_of_successfully_loaded_plugins, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&plugin_base::gui, NULL, p );
                ++nb_of_successfully_loaded_plugins;
            }
            else
                cout << "null plugin !!!" << endl;
        }
    }
}
