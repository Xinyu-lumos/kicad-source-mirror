/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dialogs/dialog_export_2581.h>

#include <board.h>
#include <footprint.h>
#include <kiway_holder.h>
#include <pcb_edit_frame.h>
#include <pcbnew_settings.h>
#include <pgm_base.h>
#include <project.h>
#include <project/board_project_settings.h>
#include <project/project_file.h>
#include <settings/settings_manager.h>
#include <widgets/std_bitmap_button.h>

#include <set>
#include <vector>
#include <wx/filedlg.h>

static wxString s_oemColumn = wxEmptyString;

DIALOG_EXPORT_2581::DIALOG_EXPORT_2581( PCB_EDIT_FRAME* aParent ) :
        DIALOG_EXPORT_2581_BASE( aParent ), m_parent( aParent )
{
    m_browseButton->SetBitmap( KiBitmapBundle( BITMAPS::small_folder ) );

    SetupStandardButtons( { { wxID_OK,     _( "Export" ) },
                            { wxID_CANCEL, _( "Close" )  } } );

    wxString path = m_parent->GetLastPath( LAST_PATH_2581 );

    if( path.IsEmpty() )
    {
        wxFileName brdFile( m_parent->GetBoard()->GetFileName() );
        brdFile.SetExt( wxT( "xml" ) );
        path = brdFile.GetFullPath();
    }

    m_outputFileName->SetValue( path );

    m_textDistributor->SetSize( m_choiceDistPN->GetSize() );

    // Fill wxChoice (and others) items with data before calling finishDialogSettings()
    // to calculate suitable widgets sizes
    Init();

    // Now all widgets have the size fixed, call FinishDialogSettings
    finishDialogSettings();
}


void DIALOG_EXPORT_2581::onBrowseClicked( wxCommandEvent& event )
{
    // Build the absolute path of current output directory to preselect it in the file browser.
    wxString     path = ExpandEnvVarSubstitutions( m_outputFileName->GetValue(), &Prj() );
    wxFileName   fn( Prj().AbsolutePath( path ) );
    wxString     ipc_files = _( "IPC-2581 Files (*.xml)|*.xml" );
    wxString     compressed_files = _( "IPC-2581 Compressed Files (*.zip)|*.zip" );

    wxFileDialog dlg( this, _( "Export IPC-2581 File" ), fn.GetPath(), fn.GetFullName(),
                      m_cbCompress->IsChecked() ? compressed_files : ipc_files,
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() == wxID_CANCEL )
        return;

    m_outputFileName->SetValue( dlg.GetPath() );

}

void DIALOG_EXPORT_2581::onOKClick( wxCommandEvent& event )
{
    m_parent->SetLastPath( LAST_PATH_2581, m_outputFileName->GetValue() );

    event.Skip();
}


void DIALOG_EXPORT_2581::onCompressCheck( wxCommandEvent& event )
{
    if( m_cbCompress->GetValue() )
    {
        wxFileName fn = m_outputFileName->GetValue();

        fn.SetExt( "zip" );
        m_outputFileName->SetValue( fn.GetFullPath() );
    }
    else
    {
        wxFileName fn = m_outputFileName->GetValue();

        fn.SetExt( "xml" );
        m_outputFileName->SetValue( fn.GetFullPath() );
    }
}


void DIALOG_EXPORT_2581::onMfgPNChange( wxCommandEvent& event )
{
    if( event.GetSelection() == 0 )
    {
        m_choiceMfg->Enable( false );
    }
    else
    {
        m_choiceMfg->Enable( true );

        // Don't try to guess the manufacturer if the user has already selected one
        if( m_choiceMfg->GetSelection() > 0 )
            return;

        int it = 0;

        if( it = m_choiceMfg->FindString( wxT( "manufacturer" ) ); it != wxNOT_FOUND )
        {
            m_choiceMfg->Select( it );
        }
        else if( it = m_choiceMfg->FindString( _( "manufacturer" ) ); it != wxNOT_FOUND )
        {
            m_choiceMfg->Select( it );
        }
        else if( it = m_choiceMfg->FindString( wxT( "mfg" ) ); it != wxNOT_FOUND )
        {
            m_choiceMfg->Select( it );
        }
        else if( it = m_choiceMfg->FindString( _( "mfg" ) ); it != wxNOT_FOUND )
        {
            m_choiceMfg->Select( it );
        }
    }
}


void DIALOG_EXPORT_2581::onDistPNChange( wxCommandEvent& event )
{
    if( event.GetSelection() == 0 )
    {
        m_textDistributor->Enable( false );
        m_textDistributor->SetValue( _( "N/A" ) );
    }
    else
    {
        m_textDistributor->Enable( true );

        // Don't try to guess the distributor if the user has already selected one
        if( m_textDistributor->GetValue() != _( "N/A" ) )
            return;

        wxString dist = m_choiceDistPN->GetStringSelection();
        dist.MakeUpper();

        // Try to guess the distributor from the part number column

        if( dist.Contains( wxT( "DIGIKEY" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Digi-Key" ) );
        }
        else if( dist.Contains( wxT( "DIGI-KEY" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Digi-Key" ) );
        }
        else if( dist.Contains( wxT( "MOUSER" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Mouser" ) );
        }
        else if( dist.Contains( wxT( "NEWARK" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Newark" ) );
        }
        else if( dist.Contains( wxT( "RS COMPONENTS" ) ) )
        {
            m_textDistributor->SetValue( wxT( "RS Components" ) );
        }
        else if( dist.Contains( wxT( "FARNELL" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Farnell" ) );
        }
        else if( dist.Contains( wxT( "ARROW" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Arrow" ) );
        }
        else if( dist.Contains( wxT( "AVNET" ) ) )
        {
            m_textDistributor->SetValue( wxT( "Avnet" ) );
        }
        else if( dist.Contains( wxT( "TME" ) ) )
        {
            m_textDistributor->SetValue( wxT( "TME" ) );
        }
        else if( dist.Contains( wxT( "LCSC" ) ) )
        {
            m_textDistributor->SetValue( wxT( "LCSC" ) );
        }
    }
}


bool DIALOG_EXPORT_2581::Init()
{
    PCBNEW_SETTINGS* cfg = Pgm().GetSettingsManager().GetAppSettings<PCBNEW_SETTINGS>();

    std::set<wxString> options;
    BOARD* board = m_parent->GetBoard();

    for( FOOTPRINT* fp : board->Footprints() )
    {
        for( PCB_FIELD* field : fp->GetFields() )
            options.insert( field->GetName() );
    }

    m_choiceUnits->SetSelection( cfg->m_Export2581.units );
    m_precision->SetValue( cfg->m_Export2581.precision );
    m_versionChoice->SetSelection( cfg->m_Export2581.version );
    m_cbCompress->SetValue( cfg->m_Export2581.compress );

    wxCommandEvent dummy;
    onCompressCheck( dummy );

    std::vector<wxString> items( options.begin(), options.end() );
    m_oemRef->Append( items );
    m_choiceMPN->Append( items );
    m_choiceMfg->Append( items );
    m_choiceDistPN->Append( items );

    m_oemRef->SetStringSelection( s_oemColumn );

    PROJECT_FILE& prj = Prj().GetProjectFile();

    if( !m_choiceMPN->SetStringSelection( prj.m_IP2581Bom.id ) )
        m_choiceMPN->SetSelection( 0 );

    if( m_choiceMPN->SetStringSelection( prj.m_IP2581Bom.MPN ) )
    {
        m_choiceMfg->Enable( true );

        if( !m_choiceMfg->SetStringSelection( prj.m_IP2581Bom.mfg ) )
            m_choiceMfg->SetSelection( 0 );
    }
    else
    {
        m_choiceMPN->SetSelection( 0 );
        m_choiceMfg->SetSelection( 0 );
        m_choiceMfg->Enable( false );
    }

    if( m_choiceDistPN->SetStringSelection( prj.m_IP2581Bom.distPN ) )
    {
        m_textDistributor->Enable( true );

        // The combo box selection can be fixed, so any value can be entered
        if( !prj.m_IP2581Bom.distPN.empty() )
        {
            m_textDistributor->SetValue( prj.m_IP2581Bom.dist );
        }
        else
        {
            wxCommandEvent evt;
            onDistPNChange( evt );
        }
    }
    else
    {
        m_choiceDistPN->SetSelection( 0 );
        m_textDistributor->SetValue( _( "N/A" ) );
        m_textDistributor->Enable( false );
    }

    return true;
}

bool DIALOG_EXPORT_2581::TransferDataFromWindow()
{
    PCBNEW_SETTINGS* cfg = Pgm().GetSettingsManager().GetAppSettings<PCBNEW_SETTINGS>();

    cfg->m_Export2581.units = m_choiceUnits->GetSelection();
    cfg->m_Export2581.precision = m_precision->GetValue();
    cfg->m_Export2581.version = m_versionChoice->GetSelection();
    cfg->m_Export2581.compress = m_cbCompress->GetValue();

    PROJECT_FILE& prj = Prj().GetProjectFile();
    wxString empty;

    prj.m_IP2581Bom.id = GetOEM();
    prj.m_IP2581Bom.mfg = GetMfg();
    prj.m_IP2581Bom.MPN = GetMPN();
    prj.m_IP2581Bom.distPN = GetDistPN();
    prj.m_IP2581Bom.dist = GetDist();

    s_oemColumn = m_oemRef->GetStringSelection();
    return true;
}