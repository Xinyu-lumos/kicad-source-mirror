/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2012 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 1992-2021 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <gr_basic.h>
#include <plotters/plotter.h>
#include <trigo.h>
#include <math/util.h>          // for KiROUND
#include <font/font.h>

#include <basic_gal.h>


/**
 * @param aTextSize is the char size (height or width).
 * @return the "best" value for a pen size to draw/plot a bold text.
 */
int GetPenSizeForBold( int aTextSize )
{
    return KiROUND( aTextSize / 5.0 );
}


int GetPenSizeForBold( const wxSize& aTextSize )
{
    return GetPenSizeForBold( std::min( aTextSize.x, aTextSize.y ) );
}


int GetPenSizeForNormal( int aTextSize )
{
    return KiROUND( aTextSize / 8.0 );
}


int GetPenSizeForNormal( const wxSize& aTextSize )
{
    return GetPenSizeForNormal( std::min( aTextSize.x, aTextSize.y ) );
}


/**
 * Don't allow text to become cluttered up in its own fatness.  Bold fonts are generally around
 * aSize/5 in width, so we limit them to aSize/4, and normal text to aSize/6.
 *
 * @param aPenSize is the pen size to clamp.
 * @param aSize is the character size (height or width).
 * @param aBold use true if text accept bold pen size.
 * @return the max pen size allowed.
 */
int Clamp_Text_PenSize( int aPenSize, int aSize, bool aBold )
{
    double scale    = aBold ? 4.0 : 6.0;
    int    maxWidth = KiROUND( (double) aSize / scale );

    return std::min( aPenSize, maxWidth );
}


float Clamp_Text_PenSize( float aPenSize, int aSize, bool aBold )
{
    float scale    = aBold ? 4.0 : 6.0;
    float maxWidth = (float) aSize / scale;

    return std::min( aPenSize, maxWidth );
}


int Clamp_Text_PenSize( int aPenSize, const VECTOR2I& aSize, bool aBold )
{
    int size = std::min( std::abs( aSize.x ), std::abs( aSize.y ) );

    return Clamp_Text_PenSize( aPenSize, size, aBold );
}


int GraphicTextWidth( const wxString& aText, KIFONT::FONT* aFont, const VECTOR2I& aSize,
                      bool aItalic, bool aBold )
{
    basic_gal.SetFontItalic( aItalic );
    basic_gal.SetFontBold( aBold );
    basic_gal.SetGlyphSize( VECTOR2D( aSize ) );

    if( !aFont )
        aFont = KIFONT::FONT::GetFont();

    return KiROUND( aFont->ComputeTextLineSize( &basic_gal, aText ).x );
}


/**
 * Draw a graphic text (like footprint texts).
 *
 *  @param aDC is the current Device Context. NULL if draw within a 3D GL Canvas.
 *  @param aPos is the text position (according to h_justify, v_justify).
 *  @param aColor is the text color.
 *  @param aText is the text to draw.
 *  @param aOrient is the angle.
 *  @param aSize is the text size (size.x or size.y can be < 0 for mirrored texts).
 *  @param aH_justify is the horizontal justification (Left, center, right).
 *  @param aV_justify is the vertical justification (bottom, center, top).
 *  @param aWidth is the line width (pen width) (use default width if aWidth = 0).
 *      if width < 0 : draw segments in sketch mode, width = abs(width)
 *      Use a value min(aSize.x, aSize.y) / 5 for a bold text.
 *  @param aItalic is the true to simulate an italic font.
 *  @param aBold use true to use a bold font. Useful only with default width value (aWidth = 0).
 *  @param aFont is the font to use, or nullptr for the KiCad stroke font
 *  @param aCallback( int x0, int y0, int xf, int yf, void* aData ) is a function called
 *                  (if non null) to draw each segment. used to draw 3D texts or for plotting.
 *                  NULL for normal drawings
 *  @param aCallbackData is the auxiliary parameter aData for the callback function.
 *                       can be nullptr if no auxiliary parameter is needed
 *  @param aPlotter is a PLOTTER instance, when this function is used to plot
 *                  the text. NULL to draw this text.
 */
void GRText( wxDC* aDC, const VECTOR2I& aPos, const COLOR4D& aColor, const wxString& aText,
             const EDA_ANGLE& aOrient, const VECTOR2I& aSize, enum GR_TEXT_H_ALIGN_T aH_justify,
             enum GR_TEXT_V_ALIGN_T aV_justify, int aWidth, bool aItalic, bool aBold,
             KIFONT::FONT* aFont, void (* aCallback)( int x0, int y0, int xf, int yf, void* aData ),
             void* aCallbackData, PLOTTER* aPlotter )
{
    bool fill_mode = true;

    if( !aFont )
        aFont = KIFONT::FONT::GetFont();

    if( aWidth == 0 && aBold ) // Use default values if aWidth == 0
        aWidth = GetPenSizeForBold( std::min( aSize.x, aSize.y ) );

    if( aWidth < 0 )
    {
        aWidth = -aWidth;
        fill_mode = false;
    }

    basic_gal.SetIsFill( fill_mode );
    basic_gal.SetLineWidth( aWidth );
    basic_gal.SetPlotter( aPlotter );
    basic_gal.SetCallback( aCallback, aCallbackData );
    basic_gal.m_DC = aDC;
    basic_gal.m_Color = aColor;
    basic_gal.SetClipBox( nullptr );

    TEXT_ATTRIBUTES attributes;
    attributes.m_Angle = aOrient;
    attributes.m_StrokeWidth = aWidth;
    attributes.m_Italic = aItalic;
    attributes.m_Bold = aBold;
    attributes.m_Halign = aH_justify;
    attributes.m_Valign = aV_justify;

    VECTOR2D size = aSize;
    attributes.m_Mirrored = size.x < 0;

    if( size.x < 0 )
        size.x = - size.x;

    attributes.m_Size = size;

    aFont->Draw( &basic_gal, aText, VECTOR2D( aPos ), attributes );
}


