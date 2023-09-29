/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022-2023 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <footprint.h>
#include <pcb_edit_frame.h>
#include <pcb_marker.h>
#include <pcb_painter.h>
#include <pcb_textbox.h>
#include <pcb_text.h>
#include <connectivity/connectivity_data.h>
#include <ratsnest/ratsnest_data.h>
#include <string_utils.h>
#include <tool/tool_manager.h>
#include <tools/pcb_actions.h>
#include <zone.h>
#include "search_handlers.h"


void PCB_SEARCH_HANDLER::ActivateItem( long aItemRow )
{
    std::vector<long> item = { aItemRow };
    SelectItems( item );

    m_frame->GetToolManager()->RunAction( PCB_ACTIONS::properties );
}


void PCB_SEARCH_HANDLER::Sort( int aCol, bool aAscending )
{
    std::sort( m_hitlist.begin(), m_hitlist.end(),
            [&]( BOARD_ITEM* a, BOARD_ITEM* b ) -> bool
            {
                // N.B. To meet the iterator sort conditions, we cannot simply invert the truth
                // to get the opposite sort.  i.e. ~(a<b) != (a>b)
                if( aAscending )
                    return StrNumCmp( getResultCell( a, aCol ), getResultCell( b, aCol ), true ) < 0;
                else
                    return StrNumCmp( getResultCell( b, aCol ), getResultCell( a, aCol ), true ) < 0;
            } );
}


void PCB_SEARCH_HANDLER::SelectItems( std::vector<long>& aItemRows )
{
    std::vector<EDA_ITEM*> selectedItems;

    for( long row : aItemRows )
    {
        if( row >= 0 && row < (long) m_hitlist.size() )
            selectedItems.push_back( m_hitlist[row] );
    }

    m_frame->GetToolManager()->RunAction( PCB_ACTIONS::selectionClear );

    if( selectedItems.size() )
        m_frame->GetToolManager()->RunAction( PCB_ACTIONS::selectItems, &selectedItems );

    m_frame->GetCanvas()->Refresh( false );
}


FOOTPRINT_SEARCH_HANDLER::FOOTPRINT_SEARCH_HANDLER( PCB_EDIT_FRAME* aFrame ) :
        PCB_SEARCH_HANDLER( wxT( "Footprints" ), aFrame )
{
    m_columns.emplace_back( wxT( "Reference" ), 1 );
    m_columns.emplace_back( wxT( "Value" ), 2 );
    m_columns.emplace_back( wxT( "Layer" ), 1 );
    m_columns.emplace_back( wxT( "X" ), 1 );
    m_columns.emplace_back( wxT( "Y" ), 1 );
}


int FOOTPRINT_SEARCH_HANDLER::Search( const wxString& aQuery )
{
    m_hitlist.clear();
    BOARD* board = m_frame->GetBoard();

    if( board == nullptr )
        return 0;

    EDA_SEARCH_DATA frp;
    frp.findString = aQuery;

    // Try to handle whatever the user throws at us (substring, wildcards, regex, etc.)
    frp.matchMode = EDA_SEARCH_MATCH_MODE::PERMISSIVE;

    for( FOOTPRINT* fp : board->Footprints() )
    {
        if( aQuery.IsEmpty()
            || fp->Reference().Matches( frp, nullptr )
            || fp->Value().Matches( frp, nullptr ) )
        {
            m_hitlist.push_back( fp );
        }
    }

    return (int) m_hitlist.size();
}


wxString FOOTPRINT_SEARCH_HANDLER::getResultCell( BOARD_ITEM* aItem, int aCol )
{
    FOOTPRINT* fp = static_cast<FOOTPRINT*>( aItem );

    if( aCol == 0 )
        return fp->GetReference();
    else if( aCol == 1 )
        return UnescapeString( fp->GetValue() );
    else if( aCol == 2 )
        return fp->GetLayerName();
    else if( aCol == 3 )
        return m_frame->MessageTextFromValue( fp->GetX() );
    else if( aCol == 4 )
        return m_frame->MessageTextFromValue( fp->GetY() );

    return wxEmptyString;
}


ZONE_SEARCH_HANDLER::ZONE_SEARCH_HANDLER( PCB_EDIT_FRAME* aFrame ) :
        PCB_SEARCH_HANDLER( wxT( "Zones" ), aFrame )
{
    m_columns.emplace_back( wxT( "Name" ), 2 );
    m_columns.emplace_back( wxT( "Net" ), 1 );
    m_columns.emplace_back( wxT( "Layer" ), 1 );
    m_columns.emplace_back( wxT( "Priority" ), 1 );
    m_columns.emplace_back( wxT( "X" ), 1 );
    m_columns.emplace_back( wxT( "Y" ), 1 );
}


int ZONE_SEARCH_HANDLER::Search( const wxString& aQuery )
{
    m_hitlist.clear();
    BOARD* board = m_frame->GetBoard();

    EDA_SEARCH_DATA frp;
    frp.findString = aQuery;

    // Try to handle whatever the user throws at us (substring, wildcards, regex, etc.)
    frp.matchMode = EDA_SEARCH_MATCH_MODE::PERMISSIVE;

    for( BOARD_ITEM* item : board->Zones() )
    {
        ZONE* zoneItem = dynamic_cast<ZONE*>( item );

        if( zoneItem && ( aQuery.IsEmpty() || zoneItem->Matches( frp, nullptr ) ) )
            m_hitlist.push_back( zoneItem );
    }

    return (int) m_hitlist.size();
}


wxString ZONE_SEARCH_HANDLER::getResultCell( BOARD_ITEM* aItem, int aCol )
{
    ZONE* zone = static_cast<ZONE*>( aItem );

    if( aCol == 0 )
        return zone->GetZoneName();
    if( aCol == 1 )
        return UnescapeString( zone->GetNetname() );
    else if( aCol == 2 )
    {
        wxArrayString layers;
        BOARD*        board = m_frame->GetBoard();

        for( PCB_LAYER_ID layer : zone->GetLayerSet().Seq() )
            layers.Add( board->GetLayerName( layer ) );

        return wxJoin( layers, ',' );
    }
    else if( aCol == 3 )
        return wxString::Format( "%d", zone->GetAssignedPriority() );
    else if( aCol == 4 )
        return m_frame->MessageTextFromValue( zone->GetX() );
    else if( aCol == 5 )
        return m_frame->MessageTextFromValue( zone->GetY() );

    return wxEmptyString;
}


TEXT_SEARCH_HANDLER::TEXT_SEARCH_HANDLER( PCB_EDIT_FRAME* aFrame ) :
        PCB_SEARCH_HANDLER( wxT( "Text" ), aFrame )
{
    m_columns.emplace_back( wxT( "Type" ), 1 );
    m_columns.emplace_back( wxT( "Text" ), 3 );
    m_columns.emplace_back( wxT( "Layer" ), 1 );
    m_columns.emplace_back( wxT( "X" ), 1 );
    m_columns.emplace_back( wxT( "Y" ), 1 );
}


int TEXT_SEARCH_HANDLER::Search( const wxString& aQuery )
{
    m_hitlist.clear();
    BOARD* board = m_frame->GetBoard();

    EDA_SEARCH_DATA frp;
    frp.findString = aQuery;

    // Try to handle whatever the user throws at us (substring, wildcards, regex, etc.)
    frp.matchMode = EDA_SEARCH_MATCH_MODE::PERMISSIVE;

    for( BOARD_ITEM* item : board->Drawings() )
    {
        PCB_TEXT* textItem = dynamic_cast<PCB_TEXT*>( item );
        PCB_TEXTBOX* textBoxItem = dynamic_cast<PCB_TEXTBOX*>( item );

        if( textItem && ( aQuery.IsEmpty() || textItem->Matches( frp, nullptr ) ) )
            m_hitlist.push_back( textItem );
        else if( textBoxItem && ( aQuery.IsEmpty() || textBoxItem->Matches( frp, nullptr ) ) )
            m_hitlist.push_back( textBoxItem );
    }

    return (int) m_hitlist.size();
}


wxString TEXT_SEARCH_HANDLER::getResultCell( BOARD_ITEM* aItem, int aCol )
{
    if( aCol == 0 )
    {
        if( PCB_TEXT::ClassOf( aItem ) )
            return _( "Text" );
        else if( PCB_TEXTBOX::ClassOf( aItem ) )
            return _( "Textbox" );
    }
    else if( aCol == 1 )
    {
        if( PCB_TEXT::ClassOf( aItem ) )
            return UnescapeString( static_cast<PCB_TEXT*>( aItem )->GetText() );
        else if( PCB_TEXTBOX::ClassOf( aItem ) )
            return UnescapeString( static_cast<PCB_TEXTBOX*>( aItem )->GetText() );
    }
    if( aCol == 2 )
        return aItem->GetLayerName();
    else if( aCol == 3 )
        return m_frame->MessageTextFromValue( aItem->GetX() );
    else if( aCol == 4 )
        return m_frame->MessageTextFromValue( aItem->GetY() );

    return wxEmptyString;
}


NETS_SEARCH_HANDLER::NETS_SEARCH_HANDLER( PCB_EDIT_FRAME* aFrame ) :
        PCB_SEARCH_HANDLER( wxT( "Nets" ), aFrame )
{
    m_columns.emplace_back( wxT( "Name" ), 2 );
    m_columns.emplace_back( wxT( "Class" ), 2 );
}


int NETS_SEARCH_HANDLER::Search( const wxString& aQuery )
{
    m_hitlist.clear();

    EDA_SEARCH_DATA frp;
    frp.findString = aQuery;

    // Try to handle whatever the user throws at us (substring, wildcards, regex, etc.)
    frp.matchMode = EDA_SEARCH_MATCH_MODE::PERMISSIVE;

    BOARD* board = m_frame->GetBoard();

    for( NETINFO_ITEM* net : board->GetNetInfo() )
    {
        if( net && ( aQuery.IsEmpty() || net->Matches( frp, nullptr ) ) )
            m_hitlist.push_back( net );
    }

    return (int) m_hitlist.size();
}


wxString NETS_SEARCH_HANDLER::getResultCell( BOARD_ITEM* aItem, int aCol )
{
    NETINFO_ITEM* net = static_cast<NETINFO_ITEM*>( aItem );

    if( net->GetNetCode() == 0 )
    {
        if( aCol == 0 )
            return _( "No Net" );
        else if( aCol == 1 )
            return wxT( "" );
    }

    if( aCol == 0 )
        return UnescapeString( net->GetNetname() );
    else if( aCol == 1 )
        return net->GetNetClass()->GetName();

    return wxEmptyString;
}


void NETS_SEARCH_HANDLER::SelectItems( std::vector<long>& aItemRows )
{
    RENDER_SETTINGS* ps = m_frame->GetCanvas()->GetView()->GetPainter()->GetSettings();
    ps->SetHighlight( false );

    std::vector<NETINFO_ITEM*> selectedItems;

    for( long row : aItemRows )
    {
        if( row >= 0 && row < (long) m_hitlist.size() )
        {
            NETINFO_ITEM* net = static_cast<NETINFO_ITEM*>( m_hitlist[row] );

            ps->SetHighlight( true, net->GetNetCode(), true );
        }
    }

    m_frame->GetCanvas()->GetView()->UpdateAllLayersColor();
    m_frame->GetCanvas()->Refresh();
}


void NETS_SEARCH_HANDLER::ActivateItem( long aItemRow )
{
    m_frame->ShowBoardSetupDialog( _( "Net Classes" ) );
}


RATSNEST_SEARCH_HANDLER::RATSNEST_SEARCH_HANDLER( PCB_EDIT_FRAME* aFrame ) :
        PCB_SEARCH_HANDLER( wxT( "Ratsnest" ), aFrame )
{
    m_columns.emplace_back( wxT( "Name" ), 2 );
    m_columns.emplace_back( wxT( "Class" ), 2 );
}


int RATSNEST_SEARCH_HANDLER::Search( const wxString& aQuery )
{
    m_hitlist.clear();

    EDA_SEARCH_DATA frp;
    frp.findString = aQuery;

    // Try to handle whatever the user throws at us (substring, wildcards, regex, etc.)
    frp.matchMode = EDA_SEARCH_MATCH_MODE::PERMISSIVE;

    BOARD* board = m_frame->GetBoard();

    for( NETINFO_ITEM* net : board->GetNetInfo() )
    {
        if( net == nullptr || !net->Matches( frp, nullptr ) )
            continue;

        RN_NET* rn = board->GetConnectivity()->GetRatsnestForNet( net->GetNetCode() );

        if( rn && !rn->GetEdges().empty() )
            m_hitlist.push_back( net );
    }

    return (int) m_hitlist.size();
}


wxString RATSNEST_SEARCH_HANDLER::getResultCell( BOARD_ITEM* aItem, int aCol )
{
    NETINFO_ITEM* net = static_cast<NETINFO_ITEM*>( aItem );

    if( net->GetNetCode() == 0 )
    {
        if( aCol == 0 )
            return _( "No Net" );
        else if( aCol == 1 )
            return wxT( "" );
    }

    if( aCol == 0 )
        return UnescapeString( net->GetNetname() );
    else if( aCol == 1 )
        return net->GetNetClass()->GetName();

    return wxEmptyString;
}


void RATSNEST_SEARCH_HANDLER::SelectItems( std::vector<long>& aItemRows )
{
    RENDER_SETTINGS* ps = m_frame->GetCanvas()->GetView()->GetPainter()->GetSettings();
    ps->SetHighlight( false );

    std::vector<NETINFO_ITEM*> selectedItems;

    for( long row : aItemRows )
    {
        if( row >= 0 && row < (long) m_hitlist.size() )
        {
            NETINFO_ITEM* net = static_cast<NETINFO_ITEM*>( m_hitlist[row] );

            ps->SetHighlight( true, net->GetNetCode(), true );
        }
    }

    m_frame->GetCanvas()->GetView()->UpdateAllLayersColor();
    m_frame->GetCanvas()->Refresh();
}


void RATSNEST_SEARCH_HANDLER::ActivateItem( long aItemRow )
{
    m_frame->ShowBoardSetupDialog( _( "Net Classes" ) );
}
