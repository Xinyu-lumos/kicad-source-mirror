/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2009 Jean-Pierre Charras, jaen-pierre.charras@gipsa-lab.inpg.com
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef _SCH_JUNCTION_H_
#define _SCH_JUNCTION_H_


#include <sch_item.h>
#include <gal/color4d.h>

class NETLIST_OBJECT_LIST;

class SCH_JUNCTION : public SCH_ITEM
{
    wxPoint m_pos;              // Position of the junction.
    int m_diameter;             // Diameter of the junction.  Zero is user default.
    COLOR4D m_color;            // Color of the junction.  #COLOR4D::UNSPECIFIED is user default.

public:
    SCH_JUNCTION( const wxPoint& aPosition = wxPoint( 0, 0 ), int aDiameter = 0,
                  SCH_LAYER_ID aLayer = LAYER_JUNCTION );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_JUNCTION() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_JUNCTION_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_JUNCTION" );
    }

    void SwapData( SCH_ITEM* aItem ) override;

    void ViewGetLayers( int aLayers[], int& aCount ) const override;

    const EDA_RECT GetBoundingBox() const override;

    void Print( RENDER_SETTINGS* aSettings, const wxPoint& aOffset ) override;

    void Move( const wxPoint& aMoveVector ) override
    {
        m_pos += aMoveVector;
    }

    void MirrorY( int aYaxis_position ) override;
    void MirrorX( int aXaxis_position ) override;
    void Rotate( wxPoint aPosition ) override;

    void GetEndPoints( std::vector <DANGLING_END_ITEM>& aItemList ) override;

    bool IsConnectable() const override { return true; }

    std::vector<wxPoint> GetConnectionPoints() const override;

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return ( aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS ) ) ||
                aItem->Type() == SCH_COMPONENT_T;
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override
    {
        return wxString( _( "Junction" ) );
    }

    BITMAP_DEF GetMenuImage() const override;

    wxPoint GetPosition() const override { return m_pos; }
    void SetPosition( const wxPoint& aPosition ) override { m_pos = aPosition; }

    bool IsPointClickableAnchor( const wxPoint& aPos ) const override { return GetPosition() == aPos; }

    int GetDiameter() const;
    void SetDiameter( int aDiameter ) { m_diameter = aDiameter; }

    COLOR4D GetColor() const;
    void SetColor( const COLOR4D& aColor ) { m_color = aColor; }

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    void Plot( PLOTTER* aPlotter ) override;

    EDA_ITEM* Clone() const override;

    virtual bool operator <( const SCH_ITEM& aItem ) const override;

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override;
#endif

private:
    bool doIsConnected( const wxPoint& aPosition ) const override;
};


#endif    // _SCH_JUNCTION_H_
