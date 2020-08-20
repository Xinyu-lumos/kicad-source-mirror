/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 CERN
 * Copyright (C) 2019-2020 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#include <class_libentry.h>
#include <core/typeinfo.h>
#include <ee_actions.h>
#include <ee_collectors.h>
#include <ee_selection_tool.h>
#include <eeschema_id.h> // For MAX_SELECT_ITEM_IDS
#include <lib_edit_frame.h>
#include <lib_item.h>
#include <lib_view_frame.h>
#include <math/util.h>
#include <menus_helpers.h>
#include <painter.h>
#include <preview_items/selection_area.h>
#include <sch_base_frame.h>
#include <sch_component.h>
#include <sch_edit_frame.h>
#include <sch_field.h>
#include <sch_item.h>
#include <sch_line.h>
#include <sch_sheet.h>
#include <schematic.h>
#include <tool/tool_event.h>
#include <tool/tool_manager.h>
#include <tools/sch_line_wire_bus_tool.h>
#include <view/view.h>
#include <view/view_controls.h>
#include <view/view_group.h>


SELECTION_CONDITION EE_CONDITIONS::SingleSymbol = [] (const SELECTION& aSel )
{
    if( aSel.GetSize() == 1 )
    {
        SCH_COMPONENT* comp = dynamic_cast<SCH_COMPONENT*>( aSel.Front() );

        if( comp )
            return !comp->GetPartRef() || !comp->GetPartRef()->IsPower();
    }

    return false;
};


SELECTION_CONDITION EE_CONDITIONS::SingleDeMorganSymbol = [] ( const SELECTION& aSel )
{
    if( aSel.GetSize() == 1 )
    {
        SCH_COMPONENT* comp = dynamic_cast<SCH_COMPONENT*>( aSel.Front() );

        if( comp )
            return comp->GetPartRef() && comp->GetPartRef()->HasConversion();
    }

    return false;
};


SELECTION_CONDITION EE_CONDITIONS::SingleMultiUnitSymbol = [] ( const SELECTION& aSel )
{
    if( aSel.GetSize() == 1 )
    {
        SCH_COMPONENT* comp = dynamic_cast<SCH_COMPONENT*>( aSel.Front() );

        if( comp )
            return comp->GetPartRef() && comp->GetPartRef()->GetUnitCount() >= 2;
    }

    return false;
};


#define HITTEST_THRESHOLD_PIXELS 5


EE_SELECTION_TOOL::EE_SELECTION_TOOL() :
        TOOL_INTERACTIVE( "eeschema.InteractiveSelection" ),
        m_frame( nullptr ),
        m_additive( false ),
        m_subtractive( false ),
        m_exclusive_or( false ),
        m_multiple( false ),
        m_skip_heuristics( false ),
        m_isLibEdit( false ),
        m_isLibView( false ),
        m_unit( 0 ),
        m_convert( 0 )
{
    m_selection.Clear();
}


EE_SELECTION_TOOL::~EE_SELECTION_TOOL()
{
    getView()->Remove( &m_selection );
}

using E_C = EE_CONDITIONS;

bool EE_SELECTION_TOOL::Init()
{
    m_frame = getEditFrame<SCH_BASE_FRAME>();

    LIB_VIEW_FRAME* libViewFrame = dynamic_cast<LIB_VIEW_FRAME*>( m_frame );
    LIB_EDIT_FRAME* libEditFrame = dynamic_cast<LIB_EDIT_FRAME*>( m_frame );

    if( libEditFrame )
    {
        m_isLibEdit = true;
        m_unit = libEditFrame->GetUnit();
        m_convert = libEditFrame->GetConvert();
    }
    else
        m_isLibView = libViewFrame != nullptr;


    static KICAD_T wireOrBusTypes[] = { SCH_LINE_LOCATE_WIRE_T, SCH_LINE_LOCATE_BUS_T, EOT };
    static KICAD_T connectedTypes[] = { SCH_LINE_LOCATE_WIRE_T, SCH_LINE_LOCATE_BUS_T,
                                        SCH_GLOBAL_LABEL_T, SCH_HIER_LABEL_T, SCH_LABEL_T,
                                        SCH_SHEET_PIN_T, SCH_PIN_T, EOT };

    auto wireSelection =      E_C::MoreThan( 0 ) && E_C::OnlyType( SCH_LINE_LOCATE_WIRE_T );
    auto busSelection =       E_C::MoreThan( 0 ) && E_C::OnlyType( SCH_LINE_LOCATE_BUS_T );
    auto wireOrBusSelection = E_C::MoreThan( 0 ) && E_C::OnlyTypes( wireOrBusTypes );
    auto connectedSelection = E_C::MoreThan( 0 ) && E_C::OnlyTypes( connectedTypes );
    auto sheetSelection =     E_C::Count( 1 )    && E_C::OnlyType( SCH_SHEET_T );

    auto schEditCondition =
            [this] ( const SELECTION& aSel )
            {

                return !m_isLibEdit && !m_isLibView;
            };

    auto belowRootSheetCondition =
            [&]( const SELECTION& aSel )
            {
                SCH_EDIT_FRAME* schEditFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );

                return ( schEditFrame&&
                         schEditFrame->GetCurrentSheet().Last() !=
                         &schEditFrame->Schematic().Root() );
            };

    auto havePartCondition =
            [&]( const SELECTION& sel )
            {
                return m_isLibEdit && static_cast<LIB_EDIT_FRAME*>( m_frame )->GetCurPart();
            };

    auto& menu = m_menu.GetMenu();

    menu.AddItem( EE_ACTIONS::enterSheet,         sheetSelection && EE_CONDITIONS::Idle, 1 );
    menu.AddItem( EE_ACTIONS::explicitCrossProbe, sheetSelection && EE_CONDITIONS::Idle, 1 );
    menu.AddItem( EE_ACTIONS::leaveSheet,         belowRootSheetCondition, 1 );

    menu.AddSeparator( 100 );
    menu.AddItem( EE_ACTIONS::drawWire,           schEditCondition && EE_CONDITIONS::Empty, 100 );
    menu.AddItem( EE_ACTIONS::drawBus,            schEditCondition && EE_CONDITIONS::Empty, 100 );

    menu.AddSeparator( 100 );
    menu.AddItem( EE_ACTIONS::finishWire,         SCH_LINE_WIRE_BUS_TOOL::IsDrawingWire, 100 );

    menu.AddSeparator( 100 );
    menu.AddItem( EE_ACTIONS::finishBus,          SCH_LINE_WIRE_BUS_TOOL::IsDrawingBus, 100 );

    menu.AddSeparator( 200 );
    menu.AddItem( EE_ACTIONS::selectConnection,   wireOrBusSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::placeJunction,      wireOrBusSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::placeLabel,         wireOrBusSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::placeGlobalLabel,   wireOrBusSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::placeHierLabel,     wireOrBusSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::breakWire,          wireSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::breakBus,           busSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::importSheetPin,     sheetSelection && EE_CONDITIONS::Idle, 250 );
    menu.AddItem( EE_ACTIONS::assignNetclass,     connectedSelection && EE_CONDITIONS::Idle, 250 );

    menu.AddSeparator( 400 );
    menu.AddItem( EE_ACTIONS::symbolProperties,   havePartCondition && EE_CONDITIONS::Empty, 400 );
    menu.AddItem( EE_ACTIONS::pinTable,           havePartCondition && EE_CONDITIONS::Empty, 400 );

    menu.AddSeparator( 1000 );
    m_frame->AddStandardSubMenus( m_menu );

    return true;
}


void EE_SELECTION_TOOL::Reset( RESET_REASON aReason )
{
    m_frame = getEditFrame<SCH_BASE_FRAME>();

    if( aReason == TOOL_BASE::MODEL_RELOAD )
    {
        // Remove pointers to the selected items from containers without changing their
        // properties (as they are already deleted while a new sheet is loaded)
        m_selection.Clear();
        getView()->GetPainter()->GetSettings()->SetHighlight( false );

        LIB_EDIT_FRAME* libEditFrame = dynamic_cast<LIB_EDIT_FRAME*>( m_frame );
        LIB_VIEW_FRAME* libViewFrame = dynamic_cast<LIB_VIEW_FRAME*>( m_frame );

        if( libEditFrame )
        {
            m_isLibEdit = true;
            m_unit = libEditFrame->GetUnit();
            m_convert = libEditFrame->GetConvert();
        }
        else
            m_isLibView = libViewFrame != nullptr;
    }
    else
        // Restore previous properties of selected items and remove them from containers
        ClearSelection();

    // Reinsert the VIEW_GROUP, in case it was removed from the VIEW
    getView()->Remove( &m_selection );
    getView()->Add( &m_selection );
}


int EE_SELECTION_TOOL::UpdateMenu( const TOOL_EVENT& aEvent )
{
    ACTION_MENU*      actionMenu = aEvent.Parameter<ACTION_MENU*>();
    CONDITIONAL_MENU* conditionalMenu = dynamic_cast<CONDITIONAL_MENU*>( actionMenu );

    if( conditionalMenu )
        conditionalMenu->Evaluate( m_selection );

    if( actionMenu )
        actionMenu->UpdateAll();

    return 0;
}


const KICAD_T movableSchematicItems[] =
{
    SCH_MARKER_T,
    SCH_JUNCTION_T,
    SCH_NO_CONNECT_T,
    SCH_BUS_BUS_ENTRY_T,
    SCH_BUS_WIRE_ENTRY_T,
    SCH_LINE_T,
    SCH_BITMAP_T,
    SCH_TEXT_T,
    SCH_LABEL_T,
    SCH_GLOBAL_LABEL_T,
    SCH_HIER_LABEL_T,
    SCH_FIELD_T,
    SCH_COMPONENT_T,
    SCH_SHEET_PIN_T,
    SCH_SHEET_T,
    EOT
};


const KICAD_T movableSymbolItems[] =
{
    LIB_ARC_T,
    LIB_CIRCLE_T,
    LIB_TEXT_T,
    LIB_RECTANGLE_T,
    LIB_POLYLINE_T,
    LIB_BEZIER_T,
    LIB_PIN_T,
    LIB_FIELD_T,
    EOT
};


int EE_SELECTION_TOOL::Main( const TOOL_EVENT& aEvent )
{

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        if( m_frame->ToolStackIsEmpty() )
            m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_ARROW );

        m_additive = m_subtractive = m_exclusive_or = false;

        if( evt->Modifier( MD_SHIFT ) && evt->Modifier( MD_CTRL ) )
            m_subtractive = true;
        else if( evt->Modifier( MD_SHIFT ) )
            m_additive = true;
        else if( evt->Modifier( MD_CTRL ) )
            m_exclusive_or = true;

        // Is the user requesting that the selection list include all possible
        // items without removing less likely selection candidates
        m_skip_heuristics = !!evt->Modifier( MD_ALT );

        // Single click? Select single object
        if( evt->IsClick( BUT_LEFT ) )
        {
            if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
                schframe->FocusOnItem( nullptr );

            SelectPoint( evt->Position(), EE_COLLECTOR::AllItems, nullptr, nullptr, false,
                         m_additive, m_subtractive, m_exclusive_or );
        }

        // right click? if there is any object - show the context menu
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            bool selectionCancelled = false;

            if( m_selection.Empty() ||
                    !m_selection.GetBoundingBox().Contains( wxPoint( evt->Position() ) ) )
            {
                ClearSelection();
                SelectPoint( evt->Position(), EE_COLLECTOR::AllItems, nullptr,
                             &selectionCancelled );
                m_selection.SetIsHover( true );
            }

            if( !selectionCancelled )
                m_menu.ShowContextMenu( m_selection );
        }

        // double click? Display the properties window
        else if( evt->IsDblClick( BUT_LEFT ) )
        {
            if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
                schframe->FocusOnItem( nullptr );

            if( m_selection.Empty() )
                SelectPoint( evt->Position());

            EDA_ITEM* item = m_selection.Front();

            if( item && item->Type() == SCH_SHEET_T )
                m_toolMgr->RunAction( EE_ACTIONS::enterSheet );
            else
                m_toolMgr->RunAction( EE_ACTIONS::properties );
        }

        // drag with LMB? Select multiple objects (or at least draw a selection box) or drag them
        else if( evt->IsDrag( BUT_LEFT ) )
        {
            if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
                schframe->FocusOnItem( nullptr );

            if( m_additive || m_subtractive || m_exclusive_or || m_frame->GetDragSelects() )
            {
                selectMultiple();
            }
            else
            {
                // selection is empty? try to start dragging the item under the point where drag
                // started
                if( m_isLibEdit && m_selection.Empty() )
                    m_selection = RequestSelection( movableSymbolItems );
                else if( m_selection.Empty() )
                    m_selection = RequestSelection( movableSchematicItems );

                // Check if dragging has started within any of selected items bounding box
                if( selectionContains( evt->Position() ) )
                {
                    // Yes -> run the move tool and wait till it finishes
                    if( m_isLibEdit )
                        m_toolMgr->InvokeTool( "eeschema.SymbolMoveTool" );
                    else
                        m_toolMgr->InvokeTool( "eeschema.InteractiveMove" );
                }
                else
                {
                    // No -> drag a selection box
                    selectMultiple();
                }
            }
        }

        // context sub-menu selection?  Handle unit selection or bus unfolding
        else if( evt->Category() == TC_COMMAND && evt->Action() == TA_CHOICE_MENU_CHOICE )
        {
            if( evt->GetCommandId().get() >= ID_POPUP_SCH_SELECT_UNIT_CMP
                && evt->GetCommandId().get() <= ID_POPUP_SCH_SELECT_UNIT_CMP_MAX )
            {
                SCH_COMPONENT* component = dynamic_cast<SCH_COMPONENT*>( m_selection.Front() );
                int unit = evt->GetCommandId().get() - ID_POPUP_SCH_SELECT_UNIT_CMP;

                if( component )
                    static_cast<SCH_EDIT_FRAME*>( m_frame )->SelectUnit( component, unit );
            }
            else if( evt->GetCommandId().get() >= ID_POPUP_SCH_UNFOLD_BUS
                     && evt->GetCommandId().get() <= ID_POPUP_SCH_UNFOLD_BUS_END )
            {
                wxString* net = new wxString( *evt->Parameter<wxString*>() );
                m_toolMgr->RunAction( EE_ACTIONS::unfoldBus, true, net );
            }

        }

        else if( evt->IsCancelInteractive() )
        {
            if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
                schframe->FocusOnItem( nullptr );

            ClearSelection();
        }

        else if( evt->Action() == TA_UNDO_REDO_PRE )
        {
            if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
                schframe->FocusOnItem( nullptr );

            ClearSelection();
        }

        else
            evt->SetPassEvent();
    }

    return 0;
}


EE_SELECTION& EE_SELECTION_TOOL::GetSelection()
{
    return m_selection;
}


bool EE_SELECTION_TOOL::SelectPoint( const VECTOR2I& aWhere, const KICAD_T* aFilterList,
        EDA_ITEM** aItem, bool* aSelectionCancelledFlag, bool aCheckLocked, bool aAdd,
        bool aSubtract, bool aExclusiveOr )
{
    EE_COLLECTOR collector;

    collector.m_Threshold = KiROUND( getView()->ToWorld( HITTEST_THRESHOLD_PIXELS ) );

    if( m_isLibEdit )
    {
        auto part = static_cast<LIB_EDIT_FRAME*>( m_frame )->GetCurPart();

        if( !part )
            return false;

        collector.Collect( part->GetDrawItems(), aFilterList, (wxPoint) aWhere, m_unit, m_convert );
    }
    else
        collector.Collect( m_frame->GetScreen(), aFilterList, (wxPoint) aWhere, m_unit, m_convert );

    // Post-process collected items
    for( int i = collector.GetCount() - 1; i >= 0; --i )
    {
        if( !Selectable( collector[ i ] ) )
        {
            collector.Remove( i );
            continue;
        }

        if( aCheckLocked && collector[ i ]->IsLocked() )
        {
            collector.Remove( i );
            continue;
        }

        // SelectPoint, unlike other selection routines, can select line ends
        if( collector[ i ]->Type() == SCH_LINE_T )
        {
            SCH_LINE* line = (SCH_LINE*) collector[ i ];
            line->ClearFlags( STARTPOINT | ENDPOINT );

            if( HitTestPoints( line->GetStartPoint(), (wxPoint) aWhere, collector.m_Threshold ) )
                line->SetFlags( STARTPOINT );
            else if (HitTestPoints( line->GetEndPoint(), (wxPoint) aWhere, collector.m_Threshold ) )
                line->SetFlags( ENDPOINT );
            else
                line->SetFlags( STARTPOINT | ENDPOINT );
        }
    }

    m_selection.ClearReferencePoint();

    // Apply some ugly heuristics to avoid disambiguation menus whenever possible
    if( collector.GetCount() > 1 && !m_skip_heuristics )
    {
        GuessSelectionCandidates( collector, aWhere );
    }

    // If still more than one item we're going to have to ask the user.
    if( collector.GetCount() > 1 )
    {
        collector.m_MenuTitle = wxEmptyString;
        // Must call selectionMenu via RunAction() to avoid event-loop contention
        m_toolMgr->RunAction( EE_ACTIONS::selectionMenu, true, &collector );

        if( collector.m_MenuCancelled )
        {
            if( aSelectionCancelledFlag )
                *aSelectionCancelledFlag = true;

            return false;
        }
    }

    if( !aAdd && !aSubtract && !aExclusiveOr )
        ClearSelection();

    bool anyAdded      = false;
    bool anySubtracted = false;

    if( collector.GetCount() > 0 )
    {
        for( int i = 0; i < collector.GetCount(); ++i )
        {
            if( aSubtract || ( aExclusiveOr && collector[i]->IsSelected() ) )
            {
                unselect( collector[i] );
                anySubtracted = true;
            }
            else
            {
                select( collector[i] );
                anyAdded = true;
            }
        }
    }

    if( anyAdded )
    {
        m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );

        if( aItem && collector.GetCount() == 1 )
            *aItem = collector[0];

        return true;
    }
    else if( anySubtracted )
    {
        m_toolMgr->ProcessEvent( EVENTS::UnselectedEvent );
        return true;
    }

    return false;
}


int EE_SELECTION_TOOL::SelectAll( const TOOL_EVENT& aEvent )
{
    m_multiple = true;          // Multiple selection mode is active
    KIGFX::VIEW* view = getView();

    // hold all visible items
    std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> selectedItems;
    std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> sheetPins;

    // Filter the view items based on the selection box
    BOX2I selectionBox;

    selectionBox.SetMaximum();
    view->Query( selectionBox, selectedItems );         // Get the list of selected items

    // Sheet pins aren't in the view; add them by hand
    for( KIGFX::VIEW::LAYER_ITEM_PAIR& pair : selectedItems )
    {
        SCH_SHEET* sheet = dynamic_cast<SCH_SHEET*>( pair.first );

        if( sheet )
        {
            int layer = pair.second;

            for( SCH_SHEET_PIN* pin : sheet->GetPins() )
                sheetPins.emplace_back( KIGFX::VIEW::LAYER_ITEM_PAIR( pin, layer ) );
        }
    }

    selectedItems.insert( selectedItems.end(), sheetPins.begin(), sheetPins.end() );

    for( auto& item_pair : selectedItems )
    {
        if( EDA_ITEM* item = dynamic_cast<EDA_ITEM*>( item_pair.first ) )
        {
            if( Selectable( item ) )
                select( item );
        }
    }

    m_multiple = false;

    return 0;
}


void EE_SELECTION_TOOL::GuessSelectionCandidates( EE_COLLECTOR& collector, const VECTOR2I& aPos )
{
    // There are certain parent/child and enclosure combinations that can be handled
    // automatically.

    // Prefer exact hits to sloppy ones
    int exactHits = 0;

    for( int i = collector.GetCount() - 1; i >= 0; --i )
    {
        EDA_ITEM* item = collector[ i ];

        if( item->HitTest( (wxPoint) aPos, 0 ) )
            exactHits++;
    }

    if( exactHits > 0 && exactHits < collector.GetCount() )
    {
        for( int i = collector.GetCount() - 1; i >= 0; --i )
        {
            EDA_ITEM* item = collector[ i ];

            if( !item->HitTest( (wxPoint) aPos, 0 ) )
                collector.Transfer( item );
        }
    }

    // Prefer a non-sheet to a sheet
    for( int i = 0; collector.GetCount() == 2 && i < 2; ++i )
    {
        EDA_ITEM* item = collector[ i ];
        EDA_ITEM* other = collector[ ( i + 1 ) % 2 ];

        if( item->Type() != SCH_SHEET_T && other->Type() == SCH_SHEET_T )
            collector.Transfer( other );
    }

    // Prefer a symbol to a pin or the opposite, when both a symbol and a pin are selected
    // We need to be able to select only a pin:
    // - to display its characteristics (especially if an ERC is attached to the pin)
    // - for cross probing, to select the corresponding pad.
    // Note also the case happens only in schematic editor. In symbol editor, the symbol
    // itself is never selected
    for( int i = 0; collector.GetCount() == 2 && i < 2; ++i )
    {
        EDA_ITEM* item = collector[ i ];
        EDA_ITEM* other = collector[ ( i + 1 ) % 2 ];

        if( item->Type() == SCH_COMPONENT_T && other->Type() == SCH_PIN_T )
        {
            if( !m_isLibEdit && m_frame->eeconfig()->m_Selection.select_pin_selects_symbol )
                collector.Transfer( other );
            else
                collector.Transfer( item );
        }
    }

    // Prefer a field to a symbol
    for( int i = 0; collector.GetCount() == 2 && i < 2; ++i )
    {
        EDA_ITEM* item = collector[ i ];
        EDA_ITEM* other = collector[ ( i + 1 ) % 2 ];

        if( item->Type() == SCH_FIELD_T && other->Type() == SCH_COMPONENT_T )
            collector.Transfer( other );
    }

    // No need for multiple wires at a single point; if there's a junction select that;
    // otherwise any of the wires will do
    bool junction = false;
    bool wiresOnly = true;

    for( EDA_ITEM* item : collector )
    {
        if( item->Type() == SCH_JUNCTION_T )
            junction = true;
        else if( item->Type() != SCH_LINE_T )
            wiresOnly = false;
    }

    if( wiresOnly )
    {
        for( int j = collector.GetCount() - 1; j >= 0; --j )
        {
            if( junction && collector[ j ]->Type() != SCH_JUNCTION_T )
                collector.Transfer( j );
            else if( !junction && j > 0 )
                collector.Transfer( j );
        }
    }
}


EE_SELECTION& EE_SELECTION_TOOL::RequestSelection( const KICAD_T aFilterList[] )
{
    if( m_selection.Empty() )
    {
        VECTOR2D cursorPos = getViewControls()->GetCursorPosition( true );

        ClearSelection();
        SelectPoint( cursorPos, aFilterList );
        m_selection.SetIsHover( true );
        m_selection.ClearReferencePoint();
    }
    else        // Trim an existing selection by aFilterList
    {
        for( int i = (int) m_selection.GetSize() - 1; i >= 0; --i )
        {
            EDA_ITEM* item = (EDA_ITEM*) m_selection.GetItem( i );

            if( !item->IsType( aFilterList ) )
            {
                unselect( item );
                m_toolMgr->ProcessEvent( EVENTS::UnselectedEvent );
            }
        }
    }

    updateReferencePoint();

    return m_selection;
}


void EE_SELECTION_TOOL::updateReferencePoint()
{
    VECTOR2I refP( 0, 0 );

    if( m_selection.Size() > 0 )
    {
        if( m_isLibEdit )
            refP = static_cast<LIB_ITEM*>( m_selection.GetTopLeftItem() )->GetPosition();
        else
            refP = static_cast<SCH_ITEM*>( m_selection.GetTopLeftItem() )->GetPosition();
    }

    m_selection.SetReferencePoint( refP );
}


bool EE_SELECTION_TOOL::selectMultiple()
{
    bool cancelled = false;     // Was the tool cancelled while it was running?
    m_multiple = true;          // Multiple selection mode is active
    KIGFX::VIEW* view = getView();

    KIGFX::PREVIEW::SELECTION_AREA area;
    view->Add( &area );

    while( TOOL_EVENT* evt = Wait() )
    {
        if( evt->IsCancelInteractive() || evt->IsActivate() )
        {
            cancelled = true;
            break;
        }

        if( evt->IsDrag( BUT_LEFT ) )
        {
            if( !m_additive && !m_subtractive && !m_exclusive_or )
                ClearSelection();

            // Start drawing a selection box
            area.SetOrigin( evt->DragOrigin() );
            area.SetEnd( evt->Position() );
            area.SetAdditive( m_additive );
            area.SetSubtractive( m_subtractive );
            area.SetExclusiveOr( m_exclusive_or );

            view->SetVisible( &area, true );
            view->Update( &area );
            getViewControls()->SetAutoPan( true );
        }

        if( evt->IsMouseUp( BUT_LEFT ) )
        {
            getViewControls()->SetAutoPan( false );

            // End drawing the selection box
            view->SetVisible( &area, false );

            // Mark items within the selection box as selected
            std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> selectedItems;
            std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> sheetPins;

            // Filter the view items based on the selection box
            BOX2I selectionBox = area.ViewBBox();
            view->Query( selectionBox, selectedItems );         // Get the list of selected items

            // Sheet pins aren't in the view; add them by hand
            for( KIGFX::VIEW::LAYER_ITEM_PAIR& pair : selectedItems )
            {
                SCH_SHEET* sheet = dynamic_cast<SCH_SHEET*>( pair.first );

                if( sheet )
                {
                    int layer = pair.second;

                    for( SCH_SHEET_PIN* pin : sheet->GetPins() )
                        sheetPins.emplace_back( KIGFX::VIEW::LAYER_ITEM_PAIR( pin, layer ) );
                }
            }

            selectedItems.insert( selectedItems.end(), sheetPins.begin(), sheetPins.end() );

            int width = area.GetEnd().x - area.GetOrigin().x;
            int height = area.GetEnd().y - area.GetOrigin().y;

            /* Selection mode depends on direction of drag-selection:
             * Left > Right : Select objects that are fully enclosed by selection
             * Right > Left : Select objects that are crossed by selection
             */
            bool windowSelection = width >= 0;
            bool anyAdded = false;
            bool anySubtracted = false;

            if( view->IsMirroredX() )
                windowSelection = !windowSelection;

            // Construct an EDA_RECT to determine EDA_ITEM selection
            EDA_RECT selectionRect( (wxPoint) area.GetOrigin(), wxSize( width, height ) );

            selectionRect.Normalize();

            for( KIGFX::VIEW::LAYER_ITEM_PAIR& pair : selectedItems )
            {
                EDA_ITEM* item = dynamic_cast<EDA_ITEM*>( pair.first );

                if( item && Selectable( item ) && item->HitTest( selectionRect, windowSelection ) )
                {
                    if( m_subtractive || ( m_exclusive_or && item->IsSelected() ) )
                    {
                        unselect( item );
                        anySubtracted = true;
                    }
                    else
                    {
                        select( item );
                        item->SetFlags( STARTPOINT | ENDPOINT );
                        anyAdded = true;
                    }
                }
            }

            m_selection.SetIsHover( false );

            // Inform other potentially interested tools
            if( anyAdded )
                m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );

            if( anySubtracted )
                m_toolMgr->ProcessEvent( EVENTS::UnselectedEvent );

            break;  // Stop waiting for events
        }
    }

    getViewControls()->SetAutoPan( false );

    // Stop drawing the selection box
    view->Remove( &area );
    m_multiple = false;         // Multiple selection mode is inactive

    if( !cancelled )
        m_selection.ClearReferencePoint();

    return cancelled;
}


static KICAD_T nodeTypes[] =
{
    SCH_COMPONENT_LOCATE_POWER_T,
    SCH_PIN_T,
    SCH_LINE_LOCATE_WIRE_T,
    SCH_LINE_LOCATE_BUS_T,
    SCH_BUS_WIRE_ENTRY_T,
    SCH_BUS_BUS_ENTRY_T,
    SCH_LABEL_T,
    SCH_HIER_LABEL_T,
    SCH_GLOBAL_LABEL_T,
    SCH_SHEET_PIN_T,
    SCH_JUNCTION_T,
    EOT
};


EDA_ITEM* EE_SELECTION_TOOL::GetNode( VECTOR2I aPosition )
{
    EE_COLLECTOR collector;

    //TODO(snh): Reimplement after exposing KNN interface
    int thresholdMax = KiROUND( getView()->ToWorld( HITTEST_THRESHOLD_PIXELS ) );

    for( int threshold : { 0, thresholdMax/2, thresholdMax } )
    {
        collector.m_Threshold = threshold;
        collector.Collect( m_frame->GetScreen(), nodeTypes, (wxPoint) aPosition );

        if( collector.GetCount() > 0 )
            break;
    }

    return collector.GetCount() ? collector[ 0 ] : nullptr;
}


int EE_SELECTION_TOOL::SelectNode( const TOOL_EVENT& aEvent )
{
    VECTOR2I cursorPos = getViewControls()->GetCursorPosition( !aEvent.Modifier( MD_ALT ) );

    SelectPoint( cursorPos, nodeTypes  );

    return 0;
}


int EE_SELECTION_TOOL::SelectConnection( const TOOL_EVENT& aEvent )
{
    static KICAD_T wiresAndBuses[] = { SCH_LINE_LOCATE_WIRE_T, SCH_LINE_LOCATE_BUS_T, EOT };

    RequestSelection( wiresAndBuses );

    if( m_selection.Empty() )
        return 0;

    SCH_LINE* line = (SCH_LINE*) m_selection.Front();
    EDA_ITEMS items;

    m_frame->GetScreen()->ClearDrawingState();
    auto conns = m_frame->GetScreen()->MarkConnections( line );

    for( auto item : conns )
        select( item );

    if( m_selection.GetSize() > 1 )
        m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );

    return 0;
}


int EE_SELECTION_TOOL::AddItemToSel( const TOOL_EVENT& aEvent )
{
    AddItemToSel( aEvent.Parameter<EDA_ITEM*>() );
    m_selection.SetIsHover( false );
    return 0;
}


void EE_SELECTION_TOOL::AddItemToSel( EDA_ITEM* aItem, bool aQuietMode )
{
    if( aItem )
    {
        select( aItem );

        // Inform other potentially interested tools
        if( !aQuietMode )
            m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );
    }
}


int EE_SELECTION_TOOL::AddItemsToSel( const TOOL_EVENT& aEvent )
{
    AddItemsToSel( aEvent.Parameter<EDA_ITEMS*>(), false );
    m_selection.SetIsHover( false );
    return 0;
}


void EE_SELECTION_TOOL::AddItemsToSel( EDA_ITEMS* aList, bool aQuietMode )
{
    if( aList )
    {
        for( EDA_ITEM* item : *aList )
            select( item );

        // Inform other potentially interested tools
        if( !aQuietMode )
            m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );
    }
}


int EE_SELECTION_TOOL::RemoveItemFromSel( const TOOL_EVENT& aEvent )
{
    RemoveItemFromSel( aEvent.Parameter<EDA_ITEM*>() );
    m_selection.SetIsHover( false );
    return 0;
}


void EE_SELECTION_TOOL::RemoveItemFromSel( EDA_ITEM* aItem, bool aQuietMode )
{
    if( aItem )
    {
        unselect( aItem );

        // Inform other potentially interested tools
        if( !aQuietMode )
            m_toolMgr->ProcessEvent( EVENTS::UnselectedEvent );
    }
}


int EE_SELECTION_TOOL::RemoveItemsFromSel( const TOOL_EVENT& aEvent )
{
    RemoveItemsFromSel( aEvent.Parameter<EDA_ITEMS*>(), false );
    m_selection.SetIsHover( false );
    return 0;
}


void EE_SELECTION_TOOL::RemoveItemsFromSel( EDA_ITEMS* aList, bool aQuietMode )
{
    if( aList )
    {
        for( EDA_ITEM* item : *aList )
            unselect( item );

        // Inform other potentially interested tools
        if( !aQuietMode )
            m_toolMgr->ProcessEvent( EVENTS::UnselectedEvent );
    }
}


void EE_SELECTION_TOOL::BrightenItem( EDA_ITEM* aItem )
{
    highlight( aItem, BRIGHTENED );
}


void EE_SELECTION_TOOL::UnbrightenItem( EDA_ITEM* aItem )
{
    unhighlight( aItem, BRIGHTENED );
}


int EE_SELECTION_TOOL::ClearSelection( const TOOL_EVENT& aEvent )
{
    ClearSelection();

    return 0;
}


void EE_SELECTION_TOOL::RebuildSelection()
{
    m_selection.Clear();

    if( m_isLibEdit )
    {
        LIB_PART* start = static_cast<LIB_EDIT_FRAME*>( m_frame )->GetCurPart();

        for( LIB_ITEM& item : start->GetDrawItems() )
        {
            if( item.IsSelected() )
                select( static_cast<EDA_ITEM*>( &item ) );
        }
    }
    else
    {
        for( auto item : m_frame->GetScreen()->Items() )
        {
            // If the field and component are selected, only use the component
            if( item->IsSelected() )
            {
                select( item );
            }
            else
            {
                if( item->Type() == SCH_COMPONENT_T )
                {
                    for( SCH_FIELD& field : static_cast<SCH_COMPONENT*>( item )->GetFields() )
                    {
                        if( field.IsSelected() )
                            select( &field );
                    }
                }

                if( item->Type() == SCH_SHEET_T )
                {
                    for( SCH_FIELD& field : static_cast<SCH_SHEET*>( item )->GetFields() )
                    {
                        if( field.IsSelected() )
                            select( &field );
                    }

                    for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( item )->GetPins() )
                    {
                        if( pin->IsSelected() )
                            select( pin );
                    }
                }

            }
        }
    }

    updateReferencePoint();

    // Inform other potentially interested tools
    m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );
}


int EE_SELECTION_TOOL::SelectionMenu( const TOOL_EVENT& aEvent )
{
    EE_COLLECTOR* collector = aEvent.Parameter<EE_COLLECTOR*>();

    if( !doSelectionMenu( collector ) )
        collector->m_MenuCancelled = true;

    return 0;
}


bool EE_SELECTION_TOOL::doSelectionMenu( EE_COLLECTOR* aCollector )
{
    EDA_ITEM*   current = nullptr;
    bool        selectAll = false;
    bool        expandSelection = false;

    do
    {
        /// The user has requested the full, non-limited list of selection items
        if( expandSelection )
            aCollector->Combine();

        expandSelection = false;

        int         limit = std::min( 9, aCollector->GetCount() );
        ACTION_MENU menu( true );

        for( int i = 0; i < limit; ++i )
        {
            wxString  text;
            EDA_ITEM* item = ( *aCollector )[i];
            text           = item->GetSelectMenuText( m_frame->GetUserUnits() );

            wxString menuText = wxString::Format( "&%d. %s\t%d", i + 1, text, i + 1 );
            menu.Add( menuText, i + 1, item->GetMenuImage() );
        }

        menu.AppendSeparator();
        menu.Add( _( "Select &All\tA" ), limit + 1, net_highlight_schematic_xpm );

        if( !expandSelection && aCollector->HasAdditionalItems() )
            menu.Add( _( "&Expand Selection\tE" ), limit + 2, nullptr );

        if( aCollector->m_MenuTitle.Length() )
        {
            menu.SetTitle( aCollector->m_MenuTitle );
            menu.SetIcon( info_xpm );
            menu.DisplayTitle( true );
        }
        else
        {
            menu.DisplayTitle( false );
        }

        SetContextMenu( &menu, CMENU_NOW );

        while( TOOL_EVENT* evt = Wait() )
        {
            if( evt->Action() == TA_CHOICE_MENU_UPDATE )
            {
                if( selectAll )
                {
                    for( int i = 0; i < aCollector->GetCount(); ++i )
                        unhighlight( ( *aCollector )[i], BRIGHTENED );
                }
                else if( current )
                {
                    unhighlight( current, BRIGHTENED );
                }

                int id = *evt->GetCommandId();

                // User has pointed an item, so show it in a different way
                if( id > 0 && id <= limit )
                {
                    current = ( *aCollector )[id - 1];
                    highlight( current, BRIGHTENED );
                }
                else
                {
                    current = nullptr;
                }

                // User has pointed on the "Select All" option
                if( id == limit + 1 )
                {
                    for( int i = 0; i < aCollector->GetCount(); ++i )
                        highlight( ( *aCollector )[i], BRIGHTENED );
                    selectAll = true;
                }
                else
                {
                    selectAll = false;
                }
            }
            else if( evt->Action() == TA_CHOICE_MENU_CHOICE )
            {
                if( selectAll )
                {
                    for( int i = 0; i < aCollector->GetCount(); ++i )
                        unhighlight( ( *aCollector )[i], BRIGHTENED );
                }
                else if( current )
                    unhighlight( current, BRIGHTENED );

                OPT<int> id = evt->GetCommandId();

                // User has selected the "Select All" option
                if( id == limit + 1 )
                {
                    selectAll = true;
                    current   = nullptr;
                }
                // User has selected an item, so this one will be returned
                else if( id && ( *id > 0 ) && ( *id <= limit ) )
                {
                    selectAll = false;
                    current   = ( *aCollector )[*id - 1];
                }
                else
                {
                    selectAll = false;
                    current   = nullptr;
                }
            }
            else if( evt->Action() == TA_CHOICE_MENU_CLOSED )
            {
                break;
            }

            getView()->UpdateItems();
            m_frame->GetCanvas()->Refresh();
        }
    } while( expandSelection );

    if( selectAll )
        return true;
    else if( current )
    {
        unhighlight( current, BRIGHTENED );

        getView()->UpdateItems();
        m_frame->GetCanvas()->Refresh();

        aCollector->Empty();
        aCollector->Append( current );
        return true;
    }

    return false;
}


bool EE_SELECTION_TOOL::Selectable( const EDA_ITEM* aItem, bool checkVisibilityOnly ) const
{
    // NOTE: in the future this is where eeschema layer/itemtype visibility will be handled
    LIB_EDIT_FRAME* symbeditFrame = dynamic_cast< LIB_EDIT_FRAME* >( m_frame );

    switch( aItem->Type() )
    {
    case SCH_PIN_T:
        if( !static_cast<const SCH_PIN*>( aItem )->IsVisible() && !m_frame->GetShowAllPins() )
            return false;
        break;

    case LIB_PART_T:    // In libedit we do not want to select the symbol itself.
        return false;

    case LIB_FIELD_T:   // LIB_FIELD object can always be edited.
        break;

    case LIB_ARC_T:
    case LIB_CIRCLE_T:
    case LIB_TEXT_T:
    case LIB_RECTANGLE_T:
    case LIB_POLYLINE_T:
    case LIB_BEZIER_T:
    case LIB_PIN_T:
    {
        if( symbeditFrame )
        {
            LIB_ITEM* lib_item = (LIB_ITEM*) aItem;

            if( lib_item->GetUnit() && lib_item->GetUnit() != symbeditFrame->GetUnit() )
                return false;

            if( lib_item->GetConvert() && lib_item->GetConvert() != symbeditFrame->GetConvert() )
                return false;
        }

        break;
    }

    case SCH_MARKER_T:  // Always selectable
        return true;

    default:            // Suppress warnings
        break;
    }

    return true;
}


void EE_SELECTION_TOOL::ClearSelection()
{
    if( m_selection.Empty() )
        return;

    while( m_selection.GetSize() )
        unhighlight( (EDA_ITEM*) m_selection.Front(), SELECTED, &m_selection );

    getView()->Update( &m_selection );

    m_selection.SetIsHover( false );
    m_selection.ClearReferencePoint();

    // Inform other potentially interested tools
    m_toolMgr->ProcessEvent( EVENTS::ClearedEvent );
}


void EE_SELECTION_TOOL::select( EDA_ITEM* aItem )
{
    highlight( aItem, SELECTED, &m_selection );
}


void EE_SELECTION_TOOL::unselect( EDA_ITEM* aItem )
{
    unhighlight( aItem, SELECTED, &m_selection );
}


void EE_SELECTION_TOOL::highlight( EDA_ITEM* aItem, int aMode, EE_SELECTION* aGroup )
{
    KICAD_T itemType = aItem->Type();

    if( aMode == SELECTED )
        aItem->SetSelected();
    else if( aMode == BRIGHTENED )
        aItem->SetBrightened();

    if( aGroup )
        aGroup->Add( aItem );

    // Highlight pins and fields.  (All the other component children are currently only
    // represented in the LIB_PART and will inherit the settings of the parent component.)
    if( itemType == SCH_COMPONENT_T )
    {
        if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
        {
            SCH_PIN_PTRS pins = static_cast<SCH_COMPONENT*>( aItem )->GetSchPins(
                    &schframe->GetCurrentSheet() );

            for( SCH_PIN* pin : pins )
            {
                if( aMode == SELECTED )
                    pin->SetSelected();
                else if( aMode == BRIGHTENED )
                    pin->SetBrightened();
            }
        }

        for( SCH_FIELD& field : static_cast<SCH_COMPONENT*>( aItem )->GetFields() )
        {
            if( aMode == SELECTED )
                field.SetSelected();
            else if( aMode == BRIGHTENED )
                field.SetBrightened();
        }
    }
    else if( itemType == SCH_SHEET_T )
    {
        for( SCH_FIELD& field : static_cast<SCH_SHEET*>( aItem )->GetFields() )
        {
            if( aMode == SELECTED )
                field.SetSelected();
            else if( aMode == BRIGHTENED )
                field.SetBrightened();
        }

        for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( aItem )->GetPins() )
        {
            if( aMode == SELECTED )
                pin->SetSelected();
            else if( aMode == BRIGHTENED )
                pin->SetBrightened();
        }
    }

    if( itemType == SCH_PIN_T || itemType == SCH_FIELD_T || itemType == SCH_SHEET_PIN_T )
        getView()->Update( aItem->GetParent() );
    else
        getView()->Update( aItem );
}


void EE_SELECTION_TOOL::unhighlight( EDA_ITEM* aItem, int aMode, EE_SELECTION* aGroup )
{
    KICAD_T itemType = aItem->Type();

    if( aMode == SELECTED )
        aItem->ClearSelected();
    else if( aMode == BRIGHTENED )
        aItem->ClearBrightened();

    if( aGroup )
        aGroup->Remove( aItem );

    // Unhighlight pins and fields.  (All the other component children are currently only
    // represented in the LIB_PART.)
    if( itemType == SCH_COMPONENT_T )
    {
        if( auto schframe = dynamic_cast<SCH_EDIT_FRAME*>( m_frame ) )
        {
            SCH_PIN_PTRS pins = static_cast<SCH_COMPONENT*>( aItem )->GetSchPins(
                    &schframe->GetCurrentSheet() );

            for( SCH_PIN* pin : pins )
            {
                if( aMode == SELECTED )
                    pin->ClearSelected();
                else if( aMode == BRIGHTENED )
                    pin->ClearBrightened();
            }
        }

        for( SCH_FIELD& field : static_cast<SCH_COMPONENT*>( aItem )->GetFields() )
        {
            if( aMode == SELECTED )
                field.ClearSelected();
            else if( aMode == BRIGHTENED )
                field.ClearBrightened();
        }
    }
    else if( itemType == SCH_SHEET_T )
    {
        for( SCH_FIELD& field : static_cast<SCH_SHEET*>( aItem )->GetFields() )
        {
            if( aMode == SELECTED )
                field.ClearSelected();
            else if( aMode == BRIGHTENED )
                field.ClearBrightened();
        }

        for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( aItem )->GetPins() )
        {
            if( aMode == SELECTED )
                pin->ClearSelected();
            else if( aMode == BRIGHTENED )
                pin->ClearBrightened();
        }
    }

    if( itemType == SCH_PIN_T || itemType == SCH_FIELD_T || itemType == SCH_SHEET_PIN_T )
        getView()->Update( aItem->GetParent() );
    else
        getView()->Update( aItem );
}


bool EE_SELECTION_TOOL::selectionContains( const VECTOR2I& aPoint ) const
{
    const unsigned GRIP_MARGIN = 20;
    VECTOR2I margin = getView()->ToWorld( VECTOR2I( GRIP_MARGIN, GRIP_MARGIN ), false );

    // Check if the point is located within any of the currently selected items bounding boxes
    for( auto item : m_selection )
    {
        BOX2I itemBox = item->ViewBBox();
        itemBox.Inflate( margin.x, margin.y );    // Give some margin for gripping an item

        if( itemBox.Contains( aPoint ) )
            return true;
    }

    return false;
}


void EE_SELECTION_TOOL::setTransitions()
{
    Go( &EE_SELECTION_TOOL::UpdateMenu,          ACTIONS::updateMenu.MakeEvent() );

    Go( &EE_SELECTION_TOOL::Main,                EE_ACTIONS::selectionActivate.MakeEvent() );
    Go( &EE_SELECTION_TOOL::SelectNode,          EE_ACTIONS::selectNode.MakeEvent() );
    Go( &EE_SELECTION_TOOL::SelectConnection,    EE_ACTIONS::selectConnection.MakeEvent() );
    Go( &EE_SELECTION_TOOL::ClearSelection,      EE_ACTIONS::clearSelection.MakeEvent() );

    Go( &EE_SELECTION_TOOL::AddItemToSel,        EE_ACTIONS::addItemToSel.MakeEvent() );
    Go( &EE_SELECTION_TOOL::AddItemsToSel,       EE_ACTIONS::addItemsToSel.MakeEvent() );
    Go( &EE_SELECTION_TOOL::RemoveItemFromSel,   EE_ACTIONS::removeItemFromSel.MakeEvent() );
    Go( &EE_SELECTION_TOOL::RemoveItemsFromSel,  EE_ACTIONS::removeItemsFromSel.MakeEvent() );
    Go( &EE_SELECTION_TOOL::SelectionMenu,       EE_ACTIONS::selectionMenu.MakeEvent() );

    Go( &EE_SELECTION_TOOL::SelectAll,           EE_ACTIONS::selectAll.MakeEvent() );
}


