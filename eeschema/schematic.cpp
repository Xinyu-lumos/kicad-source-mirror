/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020-2022 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <bus_alias.h>
#include <connection_graph.h>
#include <erc_settings.h>
#include <sch_marker.h>
#include <project.h>
#include <project/project_file.h>
#include <project/net_settings.h>
#include <schematic.h>
#include <sch_junction.h>
#include <sch_line.h>
#include <sch_screen.h>
#include <sim/spice_settings.h>
#include <sch_label.h>

SCHEMATIC::SCHEMATIC( PROJECT* aPrj ) :
          EDA_ITEM( nullptr, SCHEMATIC_T ),
          m_project( nullptr ),
          m_rootSheet( nullptr )
{
    m_currentSheet    = new SCH_SHEET_PATH();
    m_connectionGraph = new CONNECTION_GRAPH( this );

    SetProject( aPrj );
}


SCHEMATIC::~SCHEMATIC()
{
    delete m_currentSheet;
    delete m_connectionGraph;
}


void SCHEMATIC::Reset()
{
    // Assume project already saved
    if( m_project )
    {
        PROJECT_FILE& project = m_project->GetProjectFile();

        delete project.m_ErcSettings;
        delete project.m_SchematicSettings;

        project.m_ErcSettings       = nullptr;
        project.m_SchematicSettings = nullptr;

        m_project = nullptr; // clear the project, so we don't do this again when setting a new one
    }

    delete m_rootSheet;

    m_rootSheet = nullptr;

    m_connectionGraph->Reset();
    m_currentSheet->clear();
}


void SCHEMATIC::SetProject( PROJECT* aPrj )
{
    if( m_project )
    {
        PROJECT_FILE& project = m_project->GetProjectFile();

        delete project.m_ErcSettings;
        delete project.m_SchematicSettings;

        project.m_ErcSettings       = nullptr;
        project.m_SchematicSettings = nullptr;
    }

    m_project = aPrj;

    if( m_project )
    {
        PROJECT_FILE& project       = m_project->GetProjectFile();
        project.m_ErcSettings       = new ERC_SETTINGS( &project, "erc" );
        project.m_SchematicSettings = new SCHEMATIC_SETTINGS( &project, "schematic" );

        project.m_SchematicSettings->LoadFromFile();
        project.m_SchematicSettings->m_NgspiceSimulatorSettings->LoadFromFile();
        project.m_ErcSettings->LoadFromFile();
    }
}


void SCHEMATIC::SetRoot( SCH_SHEET* aRootSheet )
{
    wxCHECK_RET( aRootSheet, wxS( "Call to SetRoot with null SCH_SHEET!" ) );

    m_rootSheet = aRootSheet;

    m_currentSheet->clear();
    m_currentSheet->push_back( m_rootSheet );

    m_connectionGraph->Reset();
}


SCH_SCREEN* SCHEMATIC::RootScreen() const
{
    return IsValid() ? m_rootSheet->GetScreen() : nullptr;
}


bool SCHEMATIC::ResolveTextVar( wxString* token, int aDepth ) const
{
    if( !CurrentSheet().empty() )
    {
        if( token->IsSameAs( wxT( "#" ) ) )
        {
            *token = CurrentSheet().GetPageNumber();
            return true;
        }
        else if( token->IsSameAs( wxT( "##" ) ) )
        {
            *token = wxString::Format( "%i", Root().CountSheets() );
            return true;
        }
        else if( token->IsSameAs( wxT( "SHEETPATH" ) ) )
        {
            *token = CurrentSheet().PathHumanReadable();
            return true;
        }
        else if( token->IsSameAs( wxT( "SHEETNAME" ) ) )
        {
            *token = CurrentSheet().Last()->GetName();
            return true;
        }
        else if( token->IsSameAs( wxT( "FILENAME" ) ) )
        {
            wxFileName fn( GetFileName() );
            *token = fn.GetFullName();
            return true;
        }
        else if( token->IsSameAs( wxT( "PROJECTNAME" ) ) )
        {
            *token = Prj().GetProjectName();
            return true;
        }

        return CurrentSheet().LastScreen()->GetTitleBlock().TextVarResolver( token, m_project );
    }

    if( Prj().TextVarResolver( token ) )
        return true;

    return false;
}


wxString SCHEMATIC::GetFileName() const
{
    return IsValid() ? m_rootSheet->GetScreen()->GetFileName() : wxString( wxEmptyString );
}


SCHEMATIC_SETTINGS& SCHEMATIC::Settings() const
{
    wxASSERT( m_project );
    return *m_project->GetProjectFile().m_SchematicSettings;
}


ERC_SETTINGS& SCHEMATIC::ErcSettings() const
{
    wxASSERT( m_project );
    return *m_project->GetProjectFile().m_ErcSettings;
}


std::vector<SCH_MARKER*> SCHEMATIC::ResolveERCExclusions()
{
    SCH_SHEET_LIST sheetList = GetSheets();
    ERC_SETTINGS&  settings  = ErcSettings();

    for( const SCH_SHEET_PATH& sheet : sheetList )
    {
        for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_MARKER_T ) )
        {
            SCH_MARKER* marker = static_cast<SCH_MARKER*>( item );
            auto        it = settings.m_ErcExclusions.find( marker->Serialize() );

            if( it != settings.m_ErcExclusions.end() )
            {
                marker->SetExcluded( true );
                settings.m_ErcExclusions.erase( it );
            }
        }
    }

    std::vector<SCH_MARKER*> newMarkers;

    for( const wxString& exclusionData : settings.m_ErcExclusions )
    {
        SCH_MARKER* marker = SCH_MARKER::Deserialize( exclusionData );

        if( marker )
        {
            marker->SetExcluded( true );
            newMarkers.push_back( marker );
        }
    }

    settings.m_ErcExclusions.clear();

    return newMarkers;
}


std::shared_ptr<BUS_ALIAS> SCHEMATIC::GetBusAlias( const wxString& aLabel ) const
{
    for( const SCH_SHEET_PATH& sheet : GetSheets() )
    {
        for( const std::shared_ptr<BUS_ALIAS>& alias : sheet.LastScreen()->GetBusAliases() )
        {
            if( alias->GetName() == aLabel )
                return alias;
        }
    }

    return nullptr;
}


std::set<wxString> SCHEMATIC::GetNetClassAssignmentCandidates()
{
    std::set<wxString> names;

    for( const auto& [ key, subgraphList ] : m_connectionGraph->GetNetMap() )
    {
        CONNECTION_SUBGRAPH* firstSubgraph = subgraphList[0];

        if( !firstSubgraph->m_driver_connection->IsBus()
                && firstSubgraph->GetDriverPriority() >= CONNECTION_SUBGRAPH::PRIORITY::PIN )
        {
            names.insert( key.Name );
        }
    }

    return names;
}


bool SCHEMATIC::ResolveCrossReference( wxString* token, int aDepth ) const
{
    SCH_SHEET_LIST sheetList = GetSheets();
    wxString       remainder;
    wxString       ref = token->BeforeFirst( ':', &remainder );
    SCH_SHEET_PATH sheetPath;
    SCH_ITEM*      refItem = sheetList.GetItem( KIID( ref ), &sheetPath );

    if( refItem && refItem->Type() == SCH_SYMBOL_T )
    {
        SCH_SYMBOL* refSymbol = static_cast<SCH_SYMBOL*>( refItem );

        if( refSymbol->ResolveTextVar( &remainder, aDepth + 1 ) )
            *token = remainder;
        else
            *token = refSymbol->GetRef( &sheetPath, true ) + wxS( ":" ) + remainder;

        return true;    // Cross-reference is resolved whether or not the actual textvar was
    }
    else if( refItem && refItem->Type() == SCH_SHEET_T )
    {
        SCH_SHEET* refSheet = static_cast<SCH_SHEET*>( refItem );

        if( refSheet->ResolveTextVar( &remainder, aDepth + 1 ) )
            *token = remainder;

        return true;    // Cross-reference is resolved whether or not the actual textvar was
    }

    return false;
}


std::map<int, wxString> SCHEMATIC::GetVirtualPageToSheetNamesMap() const
{
    std::map<int, wxString> namesMap;

    for( const SCH_SHEET_PATH& sheet : GetSheets() )
    {
        if( sheet.size() == 1 )
            namesMap[sheet.GetVirtualPageNumber()] = _( "<root sheet>" );
        else
            namesMap[sheet.GetVirtualPageNumber()] = sheet.Last()->GetName();
    }

    return namesMap;
}


std::map<int, wxString> SCHEMATIC::GetVirtualPageToSheetPagesMap() const
{
    std::map<int, wxString> pagesMap;

    for( const SCH_SHEET_PATH& sheet : GetSheets() )
        pagesMap[sheet.GetVirtualPageNumber()] = sheet.GetPageNumber();

    return pagesMap;
}


wxString SCHEMATIC::ConvertRefsToKIIDs( const wxString& aSource ) const
{
    wxString newbuf;
    size_t   sourceLen = aSource.length();

    for( size_t i = 0; i < sourceLen; ++i )
    {
        if( aSource[i] == '$' && i + 1 < sourceLen && aSource[i+1] == '{' )
        {
            wxString token;
            bool     isCrossRef = false;
            int      nesting = 0;

            for( i = i + 2; i < sourceLen; ++i )
            {
                if( aSource[i] == '{'
                        && ( aSource[i-1] == '_' || aSource[i-1] == '^' || aSource[i-1] == '~' ) )
                {
                    nesting++;
                }

                if( aSource[i] == '}' )
                {
                    nesting--;

                    if( nesting < 0 )
                        break;
                }

                if( aSource[i] == ':' )
                    isCrossRef = true;

                token.append( aSource[i] );
            }

            if( isCrossRef )
            {
                SCH_SHEET_LIST     sheetList = GetSheets();
                wxString           remainder;
                wxString           ref = token.BeforeFirst( ':', &remainder );
                SCH_REFERENCE_LIST references;

                sheetList.GetSymbols( references );

                for( size_t jj = 0; jj < references.GetCount(); jj++ )
                {
                    SCH_SYMBOL* refSymbol = references[ jj ].GetSymbol();

                    if( ref == refSymbol->GetRef( &references[ jj ].GetSheetPath(), true ) )
                    {
                        token = refSymbol->m_Uuid.AsString() + wxS( ":" ) + remainder;
                        break;
                    }
                }
            }

            newbuf.append( wxS( "${" ) + token + wxS( "}" ) );
        }
        else
        {
            newbuf.append( aSource[i] );
        }
    }

    return newbuf;
}


wxString SCHEMATIC::ConvertKIIDsToRefs( const wxString& aSource ) const
{
    wxString newbuf;
    size_t   sourceLen = aSource.length();

    for( size_t i = 0; i < sourceLen; ++i )
    {
        if( aSource[i] == '$' && i + 1 < sourceLen && aSource[i+1] == '{' )
        {
            wxString token;
            bool     isCrossRef = false;

            for( i = i + 2; i < sourceLen; ++i )
            {
                if( aSource[i] == '}' )
                    break;

                if( aSource[i] == ':' )
                    isCrossRef = true;

                token.append( aSource[i] );
            }

            if( isCrossRef )
            {
                SCH_SHEET_LIST sheetList = GetSheets();
                wxString       remainder;
                wxString       ref = token.BeforeFirst( ':', &remainder );

                SCH_SHEET_PATH refSheetPath;
                SCH_ITEM*      refItem = sheetList.GetItem( KIID( ref ), &refSheetPath );

                if( refItem && refItem->Type() == SCH_SYMBOL_T )
                {
                    SCH_SYMBOL* refSymbol = static_cast<SCH_SYMBOL*>( refItem );
                    token = refSymbol->GetRef( &refSheetPath, true ) + wxS( ":" ) + remainder;
                }
            }

            newbuf.append( wxS( "${" ) + token + wxS( "}" ) );
        }
        else
        {
            newbuf.append( aSource[i] );
        }
    }

    return newbuf;
}


SCH_SHEET_LIST& SCHEMATIC::GetFullHierarchy() const
{
    static SCH_SHEET_LIST hierarchy;

    hierarchy.clear();
    hierarchy.BuildSheetList( m_rootSheet, false );

    return hierarchy;
}


void SCHEMATIC::SetLegacySymbolInstanceData()
{
    SCH_SCREENS screens( m_rootSheet );

    screens.SetLegacySymbolInstanceData();
}


wxString SCHEMATIC::GetUniqueFilenameForCurrentSheet()
{
    // Filename is rootSheetName-sheetName-...-sheetName
    // Note that we need to fetch the rootSheetName out of its filename, as the root SCH_SHEET's
    // name is just a timestamp.

    wxFileName rootFn( CurrentSheet().at( 0 )->GetFileName() );
    wxString   filename = rootFn.GetName();

    for( unsigned i = 1; i < CurrentSheet().size(); i++ )
        filename += wxT( "-" ) + CurrentSheet().at( i )->GetName();

    return filename;
}


void SCHEMATIC::SetSheetNumberAndCount()
{
    SCH_SCREEN* screen;
    SCH_SCREENS s_list( Root() );

    // Set the sheet count, and the sheet number (1 for root sheet)
    int              sheet_count = Root().CountSheets();
    int              sheet_number = 1;
    const KIID_PATH& current_sheetpath = CurrentSheet().Path();

    // @todo Remove all pseudo page number system is left over from prior to real page number
    //       implementation.
    for( const SCH_SHEET_PATH& sheet : GetSheets() )
    {
        if( sheet.Path() == current_sheetpath ) // Current sheet path found
            break;

        sheet_number++; // Not found, increment before this current path
    }

    for( screen = s_list.GetFirst(); screen != nullptr; screen = s_list.GetNext() )
        screen->SetPageCount( sheet_count );

    CurrentSheet().SetVirtualPageNumber( sheet_number );
    CurrentSheet().LastScreen()->SetVirtualPageNumber( sheet_number );
    CurrentSheet().LastScreen()->SetPageNumber( CurrentSheet().GetPageNumber() );
}


void SCHEMATIC::RecomputeIntersheetRefs( const std::function<void( SCH_GLOBALLABEL* )>& aItemCallback )
{
    std::map<wxString, std::set<int>>& pageRefsMap = GetPageRefsMap();

    pageRefsMap.clear();

    SCH_SCREENS      screens( Root() );
    std::vector<int> virtualPageNumbers;

    /* Iterate over screens */
    for( SCH_SCREEN* screen = screens.GetFirst(); screen != nullptr; screen = screens.GetNext() )
    {
        virtualPageNumbers.clear();

        /* Find in which sheets this screen is used */
        for( const SCH_SHEET_PATH& sheet : GetSheets() )
        {
            if( sheet.LastScreen() == screen )
                virtualPageNumbers.push_back( sheet.GetVirtualPageNumber() );
        }

        for( SCH_ITEM* item : screen->Items() )
        {
            if( item->Type() == SCH_GLOBAL_LABEL_T )
            {
                SCH_GLOBALLABEL* globalLabel = static_cast<SCH_GLOBALLABEL*>( item );
                std::set<int>&   virtualpageList = pageRefsMap[globalLabel->GetText()];

                for( const int& pageNo : virtualPageNumbers )
                    virtualpageList.insert( pageNo );
            }
        }
    }

    bool show = Settings().m_IntersheetRefsShow;

    // Refresh all global labels.  Note that we have to collect them first as the
    // SCH_SCREEN::Update() call is going to invalidate the RTree iterator.

    std::vector<SCH_GLOBALLABEL*> currentSheetGlobalLabels;

    for( EDA_ITEM* item : CurrentSheet().LastScreen()->Items().OfType( SCH_GLOBAL_LABEL_T ) )
        currentSheetGlobalLabels.push_back( static_cast<SCH_GLOBALLABEL*>( item ) );

    for( SCH_GLOBALLABEL* globalLabel : currentSheetGlobalLabels )
    {
        std::vector<SCH_FIELD>& fields = globalLabel->GetFields();

        fields[0].SetVisible( show );

        if( show )
        {
            if( fields.size() == 1 && fields[0].GetTextPos() == globalLabel->GetPosition() )
                globalLabel->AutoplaceFields( CurrentSheet().LastScreen(), false );

            CurrentSheet().LastScreen()->Update( globalLabel );
            aItemCallback( globalLabel );
        }
    }
}


void SCHEMATIC::FixupJunctions()
{
    for( const SCH_SHEET_PATH& sheet : GetSheets() )
    {
        SCH_SCREEN* screen = sheet.LastScreen();

        std::deque<EDA_ITEM*> allItems;

        for( auto item : screen->Items() )
            allItems.push_back( item );

        // Add missing junctions and breakup wires as needed
        for( const VECTOR2I& point : screen->GetNeededJunctions( allItems ) )
        {
            SCH_JUNCTION* junction = new SCH_JUNCTION( point );
            screen->Append( junction );

            // Breakup wires
            for( SCH_LINE* wire : screen->GetBusesAndWires( point, true ) )
            {
                SCH_LINE* newSegment = wire->BreakAt( point );
                screen->Append( newSegment );
            }
        }
    }
}
