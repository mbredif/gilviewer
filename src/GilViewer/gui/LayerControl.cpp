/***********************************************************************

This file is part of the GilViewer project source files.

GilViewer is an open source 2D viewer (raster and vector) based on Boost
GIL and wxWidgets.


Homepage:

	http://code.google.com/p/gilviewer

Copyright:

	Institut Geographique National (2009)

Authors:

	Olivier Tournaire, Adrien Chauve




    GilViewer is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    GilViewer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with GilViewer.  If not, see <http://www.gnu.org/licenses/>.

***********************************************************************/

#include <list>
#include <sstream>
#include <stdexcept>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>

#include <wx/scrolwin.h>
#include <wx/toolbar.h>
#include <wx/bmpbuttn.h>
#include <wx/checkbox.h>
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/tooltip.h>
#include <wx/sizer.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/log.h>

#include "../gui/LayerControl.hpp"

#include "../layers/ImageLayer.hpp"
#include "../layers/vector_layer.hpp"
#include "../layers/VectorLayerGhost.h"

#include "../gui/VectorLayerSettingsControl.hpp"
#include "../gui/ImageLayerSettingsControl.hpp"
#include "../gui/GlobalSettingsControl.hpp"
#include "../gui/LayerInfosControl.hpp"
#include "../gui/LayerControlUtils.hpp"
#include "../gui/PanelViewer.hpp"
#include "../gui/define_id.hpp"

#include "../io/XMLDisplayConfigurationIO.hpp"
#include "../io/gilviewer_file_io.hpp"
#include "../io/gilviewer_io_factory.hpp"
#include "../tools/Orientation2D.h"

#ifdef _WINDOWS
#	include <wx/msw/winundef.h>
#endif

using namespace std;

BEGIN_EVENT_TABLE(layer_control, wxFrame)
    EVT_CLOSE(layer_control::OnCloseWindow)
    EVT_BUTTON(ID_INFO,layer_control::OnInfoButton)
    EVT_BUTTON(ID_SAVE,layer_control::OnSaveButton)
    EVT_BUTTON(ID_DELETE,layer_control::OnDeleteButton)
    // Gestion des evenements de la toolbar
    EVT_TOOL(wxID_RESET,layer_control::OnReset)
    EVT_TOOL(wxID_OPEN,layer_control::OnOpenLayer)
    // Gestion des evenements "globaux" du layer_control
    EVT_BUTTON(wxID_UP,layer_control::OnLayerUp)
    EVT_BUTTON(wxID_DOWN,layer_control::OnLayerDown)
    EVT_BUTTON(ID_VISIBILITY_BUTTON,layer_control::OnVisibilityButton)
    EVT_BUTTON(ID_TRANSFORMATION_BUTTON,layer_control::OnTransformationButton)
    EVT_BUTTON(ID_GLOBAL_SETTINGS_BUTTON,layer_control::OnGlobalSettingsButton)
    EVT_BUTTON(wxID_SAVE,layer_control::OnSaveDisplayConfigButton)
    EVT_BUTTON(wxID_OPEN,layer_control::OnLoadDisplayConfigButton)
    EVT_BUTTON(ID_DELETE_ALL_ROWS,layer_control::OnDeleteAllRowsButton)
END_EVENT_TABLE()

layer_control::layer_control(panel_viewer* DrawPane, wxFrame* parent, wxWindowID id, const wxString& title, long style, const wxPoint& pos, const wxSize& size) :
wxFrame(parent, id, title, pos, size, style), m_ghostLayer(new vector_layer_ghost), m_basicDrawPane(DrawPane), m_isOrientationSet(false)
{

    SetIcon(wxArtProvider::GetIcon(wxART_LIST_VIEW, wxART_TOOLBAR, wxSize(32,32)));
    // Couleur de fond grisee
    wxColour bgcolor(220, 220, 220);
    SetBackgroundColour(bgcolor);
    ClearBackground();

    // Ajout de la barre d'outils
    this->CreateToolBar(wxNO_BORDER | wxTB_HORIZONTAL, 500);
    InitToolbar(this->GetToolBar());

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // On initialise le sizer qui permet de gerer efficacement le positionnement des controles des differentes lignes
    m_sizer = new wxFlexGridSizer(1, 4, 5, 5);

    m_scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxT("scroll"));
    m_scroll->SetScrollRate(20,20);//,50,50);

    // On ajoute l'ensemble des boutons "globaux"
    const wxSize smallButtonSize(12,12);
    // Un sizer pour les fleches "up" et "down"
    wxBoxSizer* upDownButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    // Fleche "up"
    //wxImage im_up = wxXmlResource::Get()->LoadBitmap( wxT("GO-UP_16x16") ).ConvertToImage().Rescale(12,12,wxIMAGE_QUALITY_HIGH);
    wxBitmapButton* upButton = new wxBitmapButton(m_scroll, wxID_UP, wxXmlResource::Get()->LoadBitmap( wxT("GO-UP_16x16") ) );
    //wxBitmapButton* upButton = new wxBitmapButton(m_scroll, wxID_UP, wxArtProvider::GetBitmap(wxART_GO_UP, wxART_BUTTON, smallButtonSize) );
    //upButton->SetToolTip( _("
    upDownButtonSizer->Add(upButton, 0, wxALL | wxALIGN_CENTRE, 5);
    // Fleche "down"
    upDownButtonSizer->Add(new wxBitmapButton(m_scroll, wxID_DOWN, wxXmlResource::Get()->LoadBitmap( wxT("GO-DOWN_16x16") ) ) , 0, wxALL | wxALIGN_CENTRE, 5);
    m_sizer->Add(upDownButtonSizer, 0, wxALL | wxALIGN_CENTRE, 5);
    // Bonuton pour inverser la visibilite de tous les calques selectionnes
    wxBitmapButton* visibilityButton = new wxBitmapButton(m_scroll, ID_VISIBILITY_BUTTON, wxXmlResource::Get()->LoadBitmap( wxT("EYE_16x16") ));
    visibilityButton->SetToolTip(_("Change selected layers visibility"));
    m_sizer->Add(visibilityButton, 0, wxALL | wxALIGN_CENTRE, 5);
    // Bouton pour inverser la transformabilite (!!!) de tous les calques selectionnes
    wxBitmapButton* transformabilityButton = new wxBitmapButton(m_scroll, ID_TRANSFORMATION_BUTTON, wxXmlResource::Get()->LoadBitmap( wxT("MEASURE_16x16") ) );
    transformabilityButton->SetToolTip(_("Change selected layers transformability"));
    m_sizer->Add(transformabilityButton, 0, wxALL | wxALIGN_CENTRE, 5);

    wxBoxSizer *globalSettingsSizer = new wxBoxSizer(wxHORIZONTAL);

    // Bouton pour les modifications globales
    wxBitmapButton* globalSettingsButton = new wxBitmapButton(m_scroll, ID_GLOBAL_SETTINGS_BUTTON, wxXmlResource::Get()->LoadBitmap( wxT("PREFERENCES-SYSTEM_16x16") ) );
    globalSettingsButton->SetToolTip(_("Global settings control. Apply to all layers ..."));
    globalSettingsSizer->Add(globalSettingsButton, 0, wxALL | wxALIGN_CENTRE, 5);

    // Bouton pour la sauvegarde de la configuration d'affichage
    wxBitmapButton* saveDisplayConfig = new wxBitmapButton(m_scroll, wxID_SAVE, wxXmlResource::Get()->LoadBitmap( wxT("MEDIA-FLOPPY_16x16") ) );
    saveDisplayConfig->SetToolTip(_("Save display configuration"));
    globalSettingsSizer->Add(saveDisplayConfig, 0, wxALL | wxALIGN_CENTRE, 5);

    // Bouton pour l'ouverture d'une configuration d'affichage
    wxBitmapButton* loadDisplayConfig = new wxBitmapButton(m_scroll, wxID_OPEN, wxXmlResource::Get()->LoadBitmap( wxT("DOCUMENT-OPEN_16x16") ) );
    loadDisplayConfig->SetToolTip(_("Load display configuration"));
    globalSettingsSizer->Add(loadDisplayConfig, 0, wxALL | wxALIGN_CENTRE, 5);

    // Bouton pour la suppression de tous les layers
    wxBitmapButton* delete_all_rows = new wxBitmapButton(m_scroll, ID_DELETE_ALL_ROWS, wxXmlResource::Get()->LoadBitmap( wxT("USER-TRASH_16x16") ) );
    delete_all_rows->SetToolTip(_("Delete all rows"));
    globalSettingsSizer->Add(delete_all_rows, 0, wxALL | wxALIGN_CENTRE, 5);

    m_sizer->Add(globalSettingsSizer, 0, wxALL | wxALIGN_CENTRE, 5);

    // Quand on cree le layer_control, on en profite pour creer la fenetre de GlobalSettings.
    // On ne l'affiche que sur demande (clic bouton)
    m_globalSettingsControl = new global_settings_control(this);

    /*
	m_sizer->SetSizeHints(this);
	SetSizer(m_sizer);
	Layout();
	* */

    m_scroll->SetSizer(m_sizer);
    //m_scroll->SetAutoLayout(true);
    m_scroll->Layout();
    // add the scrolledwindow to the main sizer
    main_sizer->Add(m_scroll, 1, wxEXPAND | wxALL, 5);
    SetSizer(main_sizer);
}

void layer_control::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    // On ne fait que cacher la fenetre. Ca permet de garder les settings courants et de ne pas recalculer tout a chaque fois ...
    Hide();
}

void layer_control::AddRow(const std::string &name, layer_settings_control *layersettings, const std::string &tooltip)
{
    // Le nom du layer a une longueur maximale de 20 caracteres
    wxString layerName(name.substr(0, 50).c_str(), *wxConvCurrent);
    std::string ln(layerName.fn_str());

    // on ajoute la ligne dans le conteneur
    unsigned int nb = m_rows.size();
    m_rows.push_back(boost::shared_ptr<layer_control_row>(new layer_control_row(this, ln, nb, layersettings, tooltip)));
    // On ajoute a proprement parler les controles de la ligne dans le layer_control
    m_rows.back()->m_nameStaticText->IsSelected(true);
    m_sizer->Add(m_rows.back()->m_nameStaticText, 0, wxTOP | wxALIGN_LEFT, 5);

    m_sizer->Add(m_rows.back()->m_visibilityCheckBox, 0, wxALL | wxALIGN_CENTRE, 5);
    m_rows.back()->m_visibilityCheckBox->SetValue(m_layers.back()->IsVisible());


    m_sizer->Add(m_rows.back()->m_transformationCheckBox, 0, wxALL | wxALIGN_CENTRE, 5);
    m_rows.back()->m_transformationCheckBox->SetValue(m_layers.back()->IsTransformable());

    m_rows.back()->m_saveButton->Enable( m_layers.back()->is_saveable() );

    m_sizer->Add(m_rows.back()->m_boxSizer, wxGROW);
    m_sizer->Fit(m_scroll);
    m_scroll->Layout();
}

void layer_control::InitToolbar(wxToolBar* toolBar)
{
    if ( !toolBar )
        toolBar = new wxToolBar(m_parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER | wxTB_HORIZONTAL);

    toolBar->AddTool(wxID_OPEN, wxT("O"), wxXmlResource::Get()->LoadBitmap( wxT("DOCUMENT-OPEN_22x22") ) , wxNullBitmap, wxITEM_NORMAL, _("Open file"));
    toolBar->AddTool(wxID_RESET, wxT("R"), wxXmlResource::Get()->LoadBitmap( wxT("EDIT-CLEAR_22x22") ) , wxNullBitmap, wxITEM_NORMAL, _("Reset"));

    toolBar->Realize();
}

void layer_control::update()
{
    for (unsigned int id = 0;id<m_layers.size();++id)
    {
        m_rows[id]->m_transformationCheckBox->SetValue( m_layers[id]->IsTransformable() );
        m_rows[id]->m_visibilityCheckBox->SetValue( m_layers[id]->IsVisible() );
    }
}

void layer_control::OnInfoButton(wxCommandEvent& event)
{
    // Get layer index
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_INFO);

    wxString title(_("Infos: "));
    title << wxString(m_layers[id]->Name().c_str(), *wxConvCurrent);
    layer_infos_control *lic = new layer_infos_control(m_layers[id]->GetInfos(), this, wxID_ANY, title, wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL | wxCLOSE_BOX);
    lic->Show();
}

void layer_control::OnSaveButton(wxCommandEvent& event)
{
    // On commence par recupere l'indice du calque
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_SAVE);
    wxString wildcard(m_layers[id]->get_available_formats_wildcard().c_str(), *wxConvCurrent);
    std::string file = m_layers[id]->Filename();
    wxFileDialog *fileDialog = new wxFileDialog(NULL, _("Save layer"), wxT(""), wxString(file.c_str(), *wxConvCurrent), wildcard, wxFD_SAVE | wxFD_CHANGE_DIR | wxOVERWRITE_PROMPT);
    if (fileDialog->ShowModal() == wxID_OK)
    {
        try
        {
            string filename(fileDialog->GetFilename().To8BitData());
            string extension(boost::filesystem::extension(filename));
            boost::to_lower(extension);
            boost::shared_ptr<gilviewer_file_io> file_out = gilviewer_io_factory::Instance()->createObject(extension.substr(1,3));
            file_out->save(Layers()[id],filename);
        }
        catch( std::exception &err )
        {
            std::ostringstream oss;
            oss << "File : " << __FILE__ << "\n";
            oss << "Function : " << __FUNCTION__ << "\n";
            oss << "Line : " << __LINE__ << "\n";
            oss << err.what();
            wxMessageBox(wxString(oss.str().c_str(), *wxConvCurrent));
        }
    }
}

void layer_control::OnDeleteButton(wxCommandEvent& event)
{
    // Get layer index
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_DELETE);

    //Swap
    for (unsigned int i = id; i < m_rows.size() - 1; ++i)
    {
        SwapRows(i, i + 1);
    }

    //Destroy de la row
    m_rows.back()->m_nameStaticText->Destroy();
    m_rows.back()->m_visibilityCheckBox->Destroy();
    m_rows.back()->m_transformationCheckBox->Destroy();
    m_rows.back()->m_infoButton->Destroy();
    m_rows.back()->m_saveButton->Destroy();
    m_rows.back()->m_deleteButton->Destroy();
    m_rows.back()->m_settingsButton->Destroy();
    m_rows.back()->m_center_button->Destroy();
    m_rows.back()->m_layerSettingsControl->Destroy();
    m_sizer->Remove(m_rows.back()->m_boxSizer);

    m_rows.pop_back();

    //Delete de la layer
    m_layers.pop_back();

    //Refresh de la vue
    m_basicDrawPane->Refresh();

    //
    m_sizer->Fit(m_scroll);
    m_scroll->Layout();

    //Orientation : supprimee si tous les calques ont ete supprimes
    if (m_layers.empty())
        m_isOrientationSet = false;

}

void layer_control::OnSettingsButton(wxCommandEvent& event)
{
    // Get layer index
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_SETTINGS);
    m_rows[id]->m_layerSettingsControl->Show(!m_rows[id]->m_layerSettingsControl->IsVisible());
}

void layer_control::OnCenterButton(wxCommandEvent& event)
{
    /*
    // Get layer index
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_CENTER);
    for (layer_control::iterator it = begin(); it != end(); ++it)
    {
        // TODO: handle cartographic or image coordinates
        (*it)->TranslationX((*it)->TranslationX()-m_layers[id]->get_center_x());
        (*it)->TranslationY((*it)->TranslationY()+m_layers[id]->get_center_y());
        (*it)->HasToBeUpdated(true);
    }
    m_basicDrawPane->Refresh();
    * */
}

void layer_control::OnCheckVisibility(wxCommandEvent& event)
{
    // On commence par recupere l'indice du calque
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_VISIBILITY);
    // On inverse ...
    m_layers[id]->IsVisible(m_rows[id]->m_visibilityCheckBox->GetValue());
    // ... et on refresh
    m_layers[id]->HasToBeUpdated(true);
    m_basicDrawPane->Refresh();
}

void layer_control::OnCheckTransformable(wxCommandEvent& event)
{
    // On commence par recupere l'indice du calque
    unsigned int id = static_cast<unsigned int> (event.GetId()) - static_cast<unsigned int> (ID_TRANSFORMATION);
    // On inverse ...
    m_layers[id]->IsTransformable(m_rows[id]->m_transformationCheckBox->GetValue());
    // ... et on refresh
    m_basicDrawPane->Refresh();
}

void layer_control::OnReset(wxCommandEvent& event)
{
    // Pour chaque calque, on reinitialise
    for (layer_control::iterator it = begin(); it != end(); ++it)
    {
        (*it)->ZoomFactor(1.);
        (*it)->TranslationX(0.);
        (*it)->TranslationY(0.);
        (*it)->HasToBeUpdated(true);
        (*it)->IsTransformable(true);
        (*it)->IsVisible(true);
    }

    // Et on remet les checkbox en place (true)
    for (unsigned int i = 0; i < m_rows.size(); ++i)
    {
        m_rows[i]->m_visibilityCheckBox->SetValue(true);
        m_rows[i]->m_transformationCheckBox->SetValue(true);
    }

    // On refresh le tout ...
    m_basicDrawPane->Refresh();
}

void layer_control::OnOpenLayer(wxCommandEvent& event)
{
    wxString wildcard;
    wildcard << _("All supported files ");
    wildcard << wxT("(*.tif;*.tiff;*.png;*.jpg;*.jpeg;*.bmp;*.txt;*.xml;*.bin;*.shp;*.kml)|*.tif;*.tiff;*.TIF;*.TIFF;*.png;*.PNG;*.jpg;*.jpeg;*.JPG;*.JPEG;*.bmp;*.BMP;*.txt;*.TXT;*.xml;*.XML;*.bin;*.BIN;*.shp;*.SHP;*.kml;*.KML|");
    wildcard << _("Image files ");
    wildcard << wxT("(*.tif;*.tiff;*.png;*.jpg;*.jpeg)|*.tif;*.tiff;*.png;*.jpg;*.jpeg;*.bmp|");
    wildcard << wxT("TIFF (*.tif;*.tiff;*.TIF;*.TIFF)|*.tif;*.tiff;*.TIF;*.TIFF|");
    wildcard << wxT("PNG (*.png;*.PNG)|*.png;*.PNG|");
    wildcard << wxT("JPEG (*.jpg;*.jpeg;*.JPG;*.JPEG)|*.jpg;*.jpeg;*.JPG;*.JPEG|");
    wildcard << wxT("BMP (*.bmp)|*.bmp;*.BMP|");
    wildcard << wxT("Serialization text (*.txt)|*.txt;*.TXT|");
    wildcard << wxT("Serialization xml (*.xml)|*.xml;*.XML|");
    wildcard << wxT("Serialization binary (*.bin)|*.bin;*.BIN|");
    wildcard << wxT("Shapefile (*.shp)|*.shp;*.SHP|");
    wildcard << wxT("KML (*.kml)|*.kml;*.KML|");
    wildcard << _("Custom format ");
    wildcard << wxT("(*)|*");
    wxString str;
    //wxConfigBase::Get()->Read(_T("/Paths/WorkingDirectory"), &str, ::wxGetCwd());
    wxFileDialog *fileDialog = new wxFileDialog(this, _("Open image or shapefile"), wxT(""), wxT(""), wildcard, wxFD_OPEN|wxFD_CHANGE_DIR|wxFD_MULTIPLE );

    if (fileDialog->ShowModal() == wxID_OK)
    {
        wxArrayString names;
        fileDialog->GetPaths(names);
        AddLayersFromFiles(names);
    }
    //fileDialog->Destroy();
    m_basicDrawPane->SetCursor(wxCursor(wxCURSOR_ARROW));

    Layout();
    Refresh();
}

void layer_control::AddLayersFromFiles(const wxArrayString &names)
{
    unsigned int i;
    m_basicDrawPane->SetCursor(wxCursor(wxCURSOR_WAIT));
    wxProgressDialog *progress = NULL;
    //wxProgressDialog *progressLargeFile = NULL;

    //Liste des formats gérés par le viewer
    std::list<std::string> image_formats;
    image_formats.push_back(".tif");
    image_formats.push_back(".tiff");
    image_formats.push_back(".jpg");
    image_formats.push_back(".jpeg");
    image_formats.push_back(".png");
    image_formats.push_back(".bmp");

    if (names.GetCount() >= 2)
        progress = new wxProgressDialog(_("Opening files ..."), _("Reading ..."), names.GetCount(), NULL, wxPD_AUTO_HIDE | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);

    for (i = 0; i < names.GetCount(); ++i)
    {
        if (progress)
        {
            wxString m;
            m << _("Reading file ") << i << wxT("/") << names.GetCount() << wxT("\n");
            m << names[i] << wxT("\n");
            progress->Update(i, m);
        }

        string filename(names[i].fn_str());
        string extension(boost::filesystem::extension(filename));
        boost::to_lower(extension);
        try
        {
            boost::shared_ptr<gilviewer_file_io> file = gilviewer_io_factory::Instance()->createObject(extension.substr(1,3));
            AddLayer( file->load(filename) );
        }
        catch (exception &err)
        {
            if (progress)
                progress->Destroy();
            ostringstream oss;
            oss << "File : " << __FILE__ << "\n";
            oss << "Function : " << __FUNCTION__ << "\n";
            oss << "Line : " << __LINE__ << "\n";
            oss << err.what();
            wxMessageBox(wxString(oss.str().c_str(), *wxConvCurrent));
        }
        //else
        //{
        //c'est un format supporté par une appli dérivée du viewer -> on redirige vers PanelViewer::OpenCustomFormat
        // m_basicDrawPane->OpenCustomFormat(string(names[i].fn_str()));
        // !!!!!!!!!!! You now have to register the format in the factory !!!!!!!!!!!
        //}
    }

    m_basicDrawPane->Refresh();
    if (progress) progress->Destroy();
    m_basicDrawPane->SetCursor(wxCursor(wxCURSOR_ARROW));
}

void layer_control::AddLayer(const layer::ptrLayerType &layer)
{
    if (!layer) return;

    // On ajoute le calque dans le conteneur
    layer->SetNotifyLayerControl( boost::bind( &layer_control::update, this ) );
    m_layers.push_back(layer);

    // On construit le SettingsControl en fonction du type de calque ajoute
    layer_settings_control *settingscontrol = layer->build_layer_settings_control(m_layers.size()-1, this);
    layer->SetNotifyLayerSettingsControl( boost::bind( &layer_settings_control::update, settingscontrol ) );
    // On ajoute la ligne correspondante
    AddRow(layer->Name(), settingscontrol, layer->Filename());

    //Si c'est un calque image avec ori et que c'est le premier on met en place l'orientation generale du viewer
    if (!m_isOrientationSet && m_layers.size() == 1 && layer->HasOri())
    {
        m_ori = layer->Orientation();
        m_isOrientationSet = true;
        ::wxLogMessage(_("Viewer orientation has been set!"));
    }
    else if (!m_isOrientationSet && m_layers.size() > 1 && layer->HasOri())
    {
        ::wxLogMessage(_("Warning! Image orientation will not be used, because there is no orientation defined for the first displayed image!"));
    }
    else if (!m_isOrientationSet && m_layers.size() > 1 && !layer->HasOri())
    {
        ::wxLogMessage(_("Image layer position initialised with respect to first image!"));
        layer->ZoomFactor(m_ghostLayer->ZoomFactor());
        layer->TranslationX(m_ghostLayer->TranslationX());
        layer->TranslationY(m_ghostLayer->TranslationY());

    }

    //Si il y a une orientation definie pour le viewer et pour le nouveau calque image on initialise correctement
    //sa position initiale et son zoom
    if (m_isOrientationSet && layer->HasOri())
    {
        ::wxLogMessage(_("Image layer position initialised with respect to global orientation!"));

        const orientation_2d &oriLayer = layer->Orientation();

        double newZoomFactor = m_ori.Step() / oriLayer.Step();
        double translationInitX = (oriLayer.OriginX() - m_ori.OriginX()) / oriLayer.Step();//+ m_layers[0]->TranslationX()/m_layers[0]->ZoomFactor();
        double translationInitY = -(oriLayer.OriginY() - m_ori.OriginY()) / oriLayer.Step();//+ m_layers[0]->TranslationY()/m_layers[0]->ZoomFactor();

        layer->ZoomFactor(newZoomFactor * m_layers[0]->ZoomFactor());
        layer->TranslationX(translationInitX + m_layers[0]->TranslationX() * newZoomFactor);//* layer->ZoomFactor());
        layer->TranslationY(translationInitY + m_layers[0]->TranslationY() * newZoomFactor);//* layer->ZoomFactor());
    }

    //Si il y a une orientation definie pour le viewer et et qu'on a affaire a une couche vecteur :
    if (m_isOrientationSet && layer->get_layer_type_as_string() == "Vector")
    {
        ::wxLogMessage(_("Vector layer position initialised with respect to global orientation!"));

        double translationInitX =-m_ori.OriginX();
        double translationInitY = m_ori.OriginY();

        double newZoomFactor = m_ori.Step();
        //layer->ZoomFactor(newZoomFactor * m_layers[0]->ZoomFactor());
        layer->ZoomFactor(m_layers[0]->ZoomFactor());
        layer->TranslationX(translationInitX + m_layers[0]->TranslationX() * newZoomFactor);
        layer->TranslationY(translationInitY + m_layers[0]->TranslationY() * newZoomFactor);
    }
    layer->SetDefaultDisplayParameters();
    layer->notifyLayerSettingsControl_();


    if(m_isOrientationSet)
        layer->Resolution(m_ori.Step());
    else
    {
        layer->Resolution(1.);
    }

    Refresh();
    m_parent->Refresh();
    m_basicDrawPane->Refresh();
}

void layer_control::SwapRows(const unsigned int firstRow, const unsigned int secondRow)
{
    if (firstRow < 0 || secondRow < 0)
    {
        std::ostringstream oss;
        oss << "You passed a negative index for a row !" << std::endl;
        oss << "firstRow = " << firstRow << "  --  secondRow = " << secondRow << std::endl;
        oss << "File : " << __FILE__ << std::endl;
        oss << "Line : " << __LINE__ << std::endl;
        oss << "Function : " << __FUNCTION__ << std::endl;
        std::string mess(oss.str());
        wxString mes(mess.c_str(), *wxConvCurrent);
        ::wxLogMessage(mes);
        return;
    }
    else if (firstRow >= m_rows.size() || secondRow >= m_rows.size())
    {
        std::ostringstream oss;
        oss << "You passed an invalid index for a row !" << std::endl;
        oss << "firstRow = " << firstRow << "  --  secondRow = " << secondRow << std::endl;
        oss << "m_numberOfLayers = " << m_rows.size() << std::endl;
        oss << "File : " << __FILE__ << std::endl;
        oss << "Line : " << __LINE__ << std::endl;
        oss << "Function : " << __FUNCTION__ << std::endl;
        std::string mess(oss.str());
        wxString mes(mess.c_str(), *wxConvCurrent);
        ::wxLogMessage(mes);
        return;
    }

    //statictext
    {
        // On fait aussi la mise a jour du tooltip ...
        wxString temp(m_rows[firstRow]->m_nameStaticText->GetLabel());
        wxString strTooltip(m_rows[firstRow]->m_nameStaticText->GetToolTip()->GetTip());

        m_rows[firstRow]->m_nameStaticText->SetLabel(m_rows[secondRow]->m_nameStaticText->GetLabel());
        m_rows[firstRow]->m_nameStaticText->GetToolTip()->SetTip(m_rows[secondRow]->m_nameStaticText->GetToolTip()->GetTip());
        m_rows[secondRow]->m_nameStaticText->SetLabel(temp);
        m_rows[secondRow]->m_nameStaticText->GetToolTip()->SetTip(strTooltip);
        m_rows[firstRow]->m_nameStaticText->IsSelected(false);
        m_rows[secondRow]->m_nameStaticText->IsSelected(true);
    }

    //visibility

    {
        bool temp(m_rows[firstRow]->m_visibilityCheckBox->GetValue());
        m_rows[firstRow]->m_visibilityCheckBox->SetValue(m_rows[secondRow]->m_visibilityCheckBox->GetValue());
        m_rows[secondRow]->m_visibilityCheckBox->SetValue(temp);
    }

    //transform
    {
        bool temp(m_rows[firstRow]->m_transformationCheckBox->GetValue());
        m_rows[firstRow]->m_transformationCheckBox->SetValue(m_rows[secondRow]->m_transformationCheckBox->GetValue());
        m_rows[secondRow]->m_transformationCheckBox->SetValue(temp);
    }

    //layer
    {
        layer::ptrLayerType temp = m_layers[firstRow];
        m_layers[firstRow] = m_layers[secondRow];
        m_layers[secondRow] = temp;
    }

    //layersettingscontrol
    {
        layer_settings_control *temp = m_rows[firstRow]->m_layerSettingsControl;
        m_rows[firstRow]->m_layerSettingsControl = m_rows[secondRow]->m_layerSettingsControl;
        m_rows[secondRow]->m_layerSettingsControl = temp;

        m_rows[firstRow]->m_layerSettingsControl->Index(firstRow);
        m_rows[secondRow]->m_layerSettingsControl->Index(secondRow);
    }

    // Finalement, si necessaire, il faut changer le bitmap associe au bouton d'infos
    {
        m_rows[firstRow]->m_infoButton->SetBitmapLabel(wxBitmap(m_rows[firstRow]->m_layerSettingsControl->get_icon_xpm()));
        m_rows[secondRow]->m_infoButton->SetBitmapLabel(wxBitmap(m_rows[secondRow]->m_layerSettingsControl->get_icon_xpm()));
    }

    // Ticket #9 : bouton sauvegarde ne bouge pas ...
    {
        m_rows[firstRow]->m_saveButton->Enable( m_layers[firstRow]->is_saveable() );
        m_rows[secondRow]->m_saveButton->Enable( m_layers[secondRow]->is_saveable() );
    }
}

void layer_control::OnLayerUp(wxCommandEvent& WXUNUSED(event))
{
    for (unsigned int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i]->m_nameStaticText->IsSelected())
        {
            SwapRows(i, i - 1);
            break;
        }
    }

    Refresh();
    m_basicDrawPane->Refresh();

}

void layer_control::OnLayerDown(wxCommandEvent& WXUNUSED(event))
{
    for (unsigned int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i]->m_nameStaticText->IsSelected())
        {
            SwapRows(i, i + 1);
            break;
        }
    }

    Refresh();
    m_basicDrawPane->Refresh();
}

void layer_control::OnVisibilityButton(wxCommandEvent& event)
{
    // On parcourt tous les calques et on inverse la visibilite si la selection est a true
    for (unsigned int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i]->m_nameStaticText->IsSelected() == true)
        {
            m_rows[i]->m_visibilityCheckBox->SetValue(!m_rows[i]->m_visibilityCheckBox->GetValue());
            m_layers[i]->IsVisible(m_rows[i]->m_visibilityCheckBox->GetValue());
        }
    }

    // On refresh le tout ...
    m_basicDrawPane->Refresh();
}

void layer_control::OnTransformationButton(wxCommandEvent& event)
{
    // On parcourt tous les calques et on inverse la transformabilite (!!!) si la section est a true
    for (unsigned int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i]->m_nameStaticText->IsSelected() == true)
        {
            m_rows[i]->m_transformationCheckBox->SetValue(!m_rows[i]->m_transformationCheckBox->GetValue());
            m_layers[i]->IsTransformable(m_rows[i]->m_transformationCheckBox->GetValue());
        }
    }

    // On refresh le tout ...
    m_basicDrawPane->Refresh();
}

void layer_control::OnGlobalSettingsButton(wxCommandEvent& event)
{
    m_globalSettingsControl->Show();
}

void layer_control::OnSaveDisplayConfigButton(wxCommandEvent& event)
{
    // Choix du fichier de sauvegarde
    wxFileDialog* fd = new wxFileDialog(this, _("Save display configuration"), wxT(""), wxT(""), wxT("*.xml"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxFD_CHANGE_DIR);
    if (fd->ShowModal() == wxID_OK)
    {
        std::string savename(fd->GetPath().fn_str());
        if (boost::filesystem::extension(savename) != ".xml")
            boost::filesystem::change_extension(savename, ".xml");

        xml_display_configuration_io::write(this, savename.c_str());
    }
    delete fd;
}

void layer_control::OnLoadDisplayConfigButton(wxCommandEvent& event)
{
    // Choix du fichier de sauvegarde
    wxFileDialog* fd = new wxFileDialog(this, _("Load display configuration"), wxT(""), wxT(""), wxT("*.xml"), wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR);
    if (fd->ShowModal() == wxID_OK)
    {
        wxString message;
        message << _("Reading a display configuration file: ") << fd->GetPath();
        ::wxLogMessage(message);

        std::string loadname(fd->GetPath().fn_str());
        if (boost::filesystem::extension(loadname) != ".xml")
        {
            wxLogMessage(_("Display configuration file must have the extension .xml !"), _("Error!"));
            return;
        }

        xml_display_configuration_io::read(this, loadname);
        m_basicDrawPane->Refresh();
    }

    delete fd;
}

void layer_control::OnDeleteAllRowsButton(wxCommandEvent& event)
{
    while( m_rows.size() > 0 )
    {
        wxCommandEvent event;
        event.SetInt(m_rows.size());
        OnDeleteButton(event);
    }
}

void layer_control::CreateNewImageLayerWithParameters(const ImageLayerParameters &parameters)
{
    try
    {
        // On cree cette fameuse image ...
        std::string filename(parameters.path.c_str());

        layer::ptrLayerType ptr = image_layer::CreateImageLayer(filename);
        if (!ptr) return;
        AddLayer(ptr);

        // Et on sette l'ensemble des parametres qu'on a pu lire ...
        this->m_layers.back()->IsVisible(parameters.isVisible);
        this->m_layers.back()->IsTransformable(parameters.isTransformable);
        this->m_layers.back()->Alpha(parameters.alpha);
        this->m_layers.back()->Gamma(parameters.gamma);
        this->m_layers.back()->IntensityMin(parameters.intensityMin);
        this->m_layers.back()->IntensityMax(parameters.intensityMax);
        this->m_layers.back()->IsTransparent(parameters.isTransparent);
        this->m_layers.back()->TransparencyMin(parameters.transparencyMin);
        this->m_layers.back()->TransparencyMax(parameters.transparencyMax);
        this->m_layers.back()->ZoomFactor(parameters.zoomFactor);
        this->m_layers.back()->TranslationX(parameters.translationX);
        this->m_layers.back()->TranslationY(parameters.translationY);
        this->m_layers.back()->SetAlphaChannel(parameters.useAlphaChannel,parameters.alphaChannel);

        // MAJ de l'interface
        this->m_layers.back()->notifyLayerControl_();
        this->m_layers.back()->notifyLayerSettingsControl_();
    }
    catch (std::exception &err)
    {
        std::ostringstream oss;
        oss << "File : " << __FILE__ << "\n";
        oss << "Function : " << __FUNCTION__ << "\n";
        oss << "Line : " << __LINE__ << "\n";
        oss << err.what();
        ::wxMessageBox(wxString(oss.str().c_str(), *wxConvCurrent));
    }
}

void layer_control::CreateNewVectorLayerWithParameters(const VectorLayerParameters &parameters)
{
    try
    {
        std::string filename(parameters.path.c_str());
        string extension(boost::filesystem::extension(filename));
        boost::to_lower(extension);
        boost::shared_ptr<gilviewer_file_io> file = gilviewer_io_factory::Instance()->createObject(extension.substr(1,3));
        AddLayer(file->load(filename) );

        // Et on sette l'ensemble des parametres qu'on a pu lire ...
        this->m_layers.back()->IsVisible(parameters.isVisible);
        this->m_layers.back()->IsTransformable(parameters.isTransformable);
        /*
        this->m_layers.back()->SetPointsStyle(parameters.pointsColor, parameters.pointsWidth);
        this->m_layers.back()->SetLinesStyle(parameters.linesColor, parameters.linesWidth, parameters.linesStyle);
        this->m_layers.back()->PolygonsRingsColour(parameters.polygonsRingsColor);
        this->m_layers.back()->PolygonsRingsWidth(parameters.polygonsRingsWidth);
        this->m_layers.back()->PolygonsInsideColour(parameters.polygonsInsideColor);
        this->m_layers.back()->PolygonsRingsStyle(parameters.polygonsRingsStyle);
        this->m_layers.back()->PolygonsInsideStyle(parameters.polygonsInsideStyle);
        */
        //this->m_layers.back()->set_style(parameters.polygonsInsideColor,parameters.polygonsRingsColor,parameters.polygonsInsideStyle,parameters.polygonsRingsStyle,parameters.polygonsRingsWidth);

        this->m_layers.back()->set_point_color(parameters.pointsColor);
        this->m_layers.back()->set_point_width(parameters.pointsWidth);

        this->m_layers.back()->set_line_color(parameters.linesColor);
        this->m_layers.back()->set_line_width(parameters.linesWidth);
        this->m_layers.back()->set_line_style(parameters.linesStyle);

        this->m_layers.back()->set_polygon_border_color(parameters.polygonsRingsColor);
        this->m_layers.back()->set_polygon_inner_color(parameters.polygonsInsideColor);
        this->m_layers.back()->set_polygon_border_width(parameters.polygonsRingsWidth);
        this->m_layers.back()->set_polygon_border_style(parameters.polygonsRingsStyle);
        this->m_layers.back()->set_polygon_inner_style(parameters.polygonsInsideStyle);

        this->m_layers.back()->ZoomFactor(parameters.zoomFactor);
        this->m_layers.back()->TranslationX(parameters.translationX);
        this->m_layers.back()->TranslationY(parameters.translationY);

        // MAJ de l'interface
        this->m_layers.back()->notifyLayerControl_();
        this->m_layers.back()->notifyLayerSettingsControl_();
    }
    catch (std::exception &err)
    {
        std::ostringstream oss;
        oss << "File : " << __FILE__ << "\n";
        oss << "Function : " << __FUNCTION__ << "\n";
        oss << "Line : " << __LINE__ << "\n";
        oss << err.what();
        ::wxMessageBox(wxString(oss.str().c_str(), *wxConvCurrent));
    }
}
