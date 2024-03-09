/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004 Jean-Pierre Charras, jaen-pierre.charras@gipsa-lab.inpg.com
 * Copyright (C) 2004-2024 KiCad Developers, see change_log.txt for contributors.
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

#ifndef _SCH_BUS_ENTRY_H_
#define _SCH_BUS_ENTRY_H_

#include <gal/color4d.h>
#include <sch_item.h>

#define TARGET_BUSENTRY_RADIUS schIUScale.MilsToIU( 12 )   // Circle diameter drawn at the ends


/**
 * Base class for a bus or wire entry.
 */
class SCH_BUS_ENTRY_BASE : public SCH_ITEM
{
public:
    SCH_BUS_ENTRY_BASE( KICAD_T aType, const VECTOR2I& pos = VECTOR2I( 0, 0 ),
                        bool aFlipY = false );

    bool IsDanglingStart() const { return m_isDanglingStart; }
    bool IsDanglingEnd() const { return m_isDanglingEnd; }

    void SetEndDangling( bool aDanglingState ) { m_isDanglingEnd = aDanglingState; }

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_BUS_ENTRY_BASE() { }

    void SetLastResolvedState( const SCH_ITEM* aItem ) override
    {
        const SCH_BUS_ENTRY_BASE* aEntry = dynamic_cast<const SCH_BUS_ENTRY_BASE*>( aItem );

        if( aEntry )
        {
            m_lastResolvedWidth = aEntry->m_lastResolvedWidth;
            m_lastResolvedLineStyle = aEntry->m_lastResolvedLineStyle;
            m_lastResolvedColor = aEntry->m_lastResolvedColor;
        }
    }

    /**
     * Return true for items which are moved with the anchor point at mouse cursor
     *  and false for items moved with no reference to anchor
     * @return false for a bus entry
     */
    bool IsMovableFromAnchorPoint() const override { return false; }

    VECTOR2I GetEnd() const;

    VECTOR2I GetSize() const { return m_size; }
    void SetSize( const VECTOR2I& aSize ) { m_size = aSize; }

    // Base class getter unused; necessary for property to compile
    int GetPenWidth() const override;
    void SetPenWidth( int aWidth );

    virtual bool HasLineStroke() const override { return true; }
    virtual STROKE_PARAMS GetStroke() const override { return m_stroke; }
    virtual void SetStroke( const STROKE_PARAMS& aStroke ) override { m_stroke = aStroke; }

    LINE_STYLE GetLineStyle() const;
    void SetLineStyle( LINE_STYLE aStyle );

    COLOR4D GetBusEntryColor() const;
    void SetBusEntryColor( const COLOR4D& aColor );

    void SwapData( SCH_ITEM* aItem ) override;

    void ViewGetLayers( int aLayers[], int& aCount ) const override;

    void Print( const RENDER_SETTINGS* aSettings, const VECTOR2I& aOffset ) override;

    const BOX2I GetBoundingBox() const override;

    void Move( const VECTOR2I& aMoveVector ) override
    {
        m_pos += aMoveVector;
    }

    void MirrorHorizontally( int aCenter ) override;
    void MirrorVertically( int aCenter ) override;
    void Rotate( const VECTOR2I& aCenter ) override;

    bool IsDangling() const override;

    bool IsConnectable() const override { return true; }

    bool HasConnectivityChanges( const SCH_ITEM* aItem,
                                 const SCH_SHEET_PATH* aInstance = nullptr ) const override;

    std::vector<VECTOR2I> GetConnectionPoints() const override;

    VECTOR2I GetPosition() const override { return m_pos; }
    void     SetPosition( const VECTOR2I& aPosition ) override { m_pos = aPosition; }

    bool HitTest( const VECTOR2I& aPosition, int aAccuracy = 0 ) const override;
    bool IsPointClickableAnchor( const VECTOR2I& aPos ) const override
    {
        return ( GetPosition() == aPos && IsDanglingStart() )
               || ( GetEnd() == aPos && IsDanglingEnd() );
    }

    bool HitTest( const BOX2I& aRect, bool aContained, int aAccuracy = 0 ) const override;

    void Plot( PLOTTER* aPlotter, bool aBackground,
               const SCH_PLOT_SETTINGS& aPlotSettings ) const override;

    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

    bool operator <( const SCH_ITEM& aItem ) const override;

    double Similarity( const SCH_ITEM& aItem ) const override;

    bool operator==( const SCH_ITEM& aItem ) const override;

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif

private:
    bool doIsConnected( const VECTOR2I& aPosition ) const override;

protected:
    VECTOR2I      m_pos;
    VECTOR2I      m_size;
    bool          m_isDanglingStart;
    bool          m_isDanglingEnd;
    STROKE_PARAMS m_stroke;

    // If real-time connectivity gets disabled (due to being too slow on a particular
    // design), we can no longer rely on getting the NetClass to find netclass-specific
    // linestyles, linewidths and colors.
    mutable LINE_STYLE m_lastResolvedLineStyle;
    mutable int        m_lastResolvedWidth;
    mutable COLOR4D    m_lastResolvedColor;
};

/**
 * Class for a wire to bus entry.
 */
class SCH_BUS_WIRE_ENTRY : public SCH_BUS_ENTRY_BASE
{
public:
    SCH_BUS_WIRE_ENTRY( const VECTOR2I& pos = VECTOR2I( 0, 0 ), bool aFlipY = false );

    SCH_BUS_WIRE_ENTRY( const VECTOR2I& pos, int aQuadrant );

    ~SCH_BUS_WIRE_ENTRY() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_BUS_WIRE_ENTRY_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_BUS_WIRE_ENTRY" );
    }

    int GetPenWidth() const override;

    void GetEndPoints( std::vector< DANGLING_END_ITEM >& aItemList ) override;

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS );
    }

    wxString GetItemDescription( UNITS_PROVIDER* aUnitsProvider ) const override;

    EDA_ITEM* Clone() const override;

    virtual bool ConnectionPropagatesTo( const EDA_ITEM* aItem ) const override;

    BITMAPS GetMenuImage() const override;

    bool UpdateDanglingState( std::vector<DANGLING_END_ITEM>& aItemListByType,
                              std::vector<DANGLING_END_ITEM>& aItemListByPos,
                              const SCH_SHEET_PATH*           aPath = nullptr ) override;

    /**
     * Pointer to the bus item (usually a bus wire) connected to this bus-wire
     * entry, if it is connected to one.
     */
    SCH_ITEM* m_connected_bus_item;
};

/**
 * Class for a bus to bus entry.
 */
class SCH_BUS_BUS_ENTRY : public SCH_BUS_ENTRY_BASE
{
public:
    SCH_BUS_BUS_ENTRY( const VECTOR2I& pos = VECTOR2I( 0, 0 ), bool aFlipY = false );

    ~SCH_BUS_BUS_ENTRY() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_BUS_BUS_ENTRY_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_BUS_BUS_ENTRY" );
    }

    int GetPenWidth() const override;

    void GetEndPoints( std::vector< DANGLING_END_ITEM >& aItemList ) override;

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T && aItem->GetLayer() == LAYER_BUS;
    }

    wxString GetItemDescription( UNITS_PROVIDER* aUnitsProvider ) const override;

    EDA_ITEM* Clone() const override;

    BITMAPS GetMenuImage() const override;

    bool UpdateDanglingState( std::vector<DANGLING_END_ITEM>& aItemListByType,
                              std::vector<DANGLING_END_ITEM>& aItemListByPos,
                              const SCH_SHEET_PATH*           aPath = nullptr ) override;

    /**
     * Pointer to the bus items (usually bus wires) connected to this bus-bus
     * entry (either or both may be nullptr)
     */
    SCH_ITEM* m_connected_bus_items[2];
};

#endif    // _SCH_BUS_ENTRY_H_
