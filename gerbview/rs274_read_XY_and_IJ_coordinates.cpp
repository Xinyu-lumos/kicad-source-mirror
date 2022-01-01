/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2010-2014 Jean-Pierre Charras  jp.charras at wanadoo.fr
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

#include <math/util.h>      // for KiROUND

#include <gerber_file_image.h>
#include <base_units.h>


/* These routines read the text string point from Text.
 * On exit, Text points the beginning of the sequence unread
 */

// conversion scale from gerber file units to Gerbview internal units
// depending on the gerber file format
// this scale list assumes gerber units are imperial.
// for metric gerber units, the imperial to metric conversion is made in read functions
#define SCALE_LIST_SIZE 9
static double scale_list[SCALE_LIST_SIZE] =
{
    1000.0 * IU_PER_MILS,   // x.1 format (certainly useless)
    100.0 * IU_PER_MILS,    // x.2 format (certainly useless)
    10.0 * IU_PER_MILS,     // x.3 format
    1.0 * IU_PER_MILS,      // x.4 format
    0.1 * IU_PER_MILS,      // x.5 format
    0.01 * IU_PER_MILS,     // x.6 format
    0.001 * IU_PER_MILS,     // x.7 format  (currently the max allowed precision)
    0.0001 * IU_PER_MILS,   // provided, but not used
    0.00001 * IU_PER_MILS,  // provided, but not used
};


/**
 * Convert a coordinate given in floating point to GerbView's internal units
 * (currently = 10 nanometers).
 */
int scaletoIU( double aCoord, bool isMetric )
{
    int ret;

    if( isMetric )  // gerber are units in mm
        ret = KiROUND( aCoord * IU_PER_MM );
    else            // gerber are units in inches
        ret = KiROUND( aCoord * IU_PER_MILS * 1000.0 );

    return ret;
}


VECTOR2I GERBER_FILE_IMAGE::ReadXYCoord( char*& Text, bool aExcellonMode )
{
    VECTOR2I pos;
    int      type_coord = 0, current_coord, nbdigits;
    bool     is_float   = false;
    char*    text;
    char     line[256];


    if( m_Relative )
        pos.x = pos.y = 0;
    else
        pos = m_CurrentPos;

    if( Text == nullptr )
        return pos;

    text = line;

    while( *Text )
    {
        if( ( *Text == 'X' ) || ( *Text == 'Y' ) || ( *Text == 'A' ) )
        {
            type_coord = *Text;
            Text++;
            text     = line;
            nbdigits = 0;

            while( IsNumber( *Text ) )
            {
                if( *Text == '.' )  // Force decimal format if reading a floating point number
                    is_float = true;

                // count digits only (sign and decimal point are not counted)
                if( (*Text >= '0') && (*Text <='9') )
                    nbdigits++;

                *(text++) = *(Text++);
            }

            *text = 0;

            if( is_float )
            {
                // When X or Y (or A) values are float numbers, they are given in mm or inches
                if( m_GerbMetric )  // units are mm
                    current_coord = KiROUND( atof( line ) * IU_PER_MILS / 0.0254 );
                else    // units are inches
                    current_coord = KiROUND( atof( line ) * IU_PER_MILS * 1000 );
            }
            else
            {
                int fmt_scale = (type_coord == 'X') ? m_FmtScale.x : m_FmtScale.y;

                if( m_NoTrailingZeros )
                {
                    // no trailing zero format, we need to add missing zeros.
                    int digit_count = (type_coord == 'X') ? m_FmtLen.x : m_FmtLen.y;

                    while( nbdigits < digit_count )
                    {
                        *(text++) = '0';
                        nbdigits++;
                    }

                    if( aExcellonMode )
                    {
                        // Truncate the extra digits if the len is more than expected
                        // because the conversion to internal units expect exactly
                        // digit_count digits
                        while( nbdigits > digit_count )
                        {
                            *(text--) = 0;
                            nbdigits--;
                        }
                    }

                    *text = 0;
                }

                current_coord = atoi( line );
                double real_scale = scale_list[fmt_scale];

                if( m_GerbMetric )
                    real_scale = real_scale / 25.4;

                current_coord = KiROUND( current_coord * real_scale );
            }

            if( type_coord == 'X' )
            {
                pos.x = current_coord;
            }
            else if( type_coord == 'Y' )
            {
                pos.y = current_coord;
            }
            else if( type_coord == 'A' )
            {
                m_ArcRadius = current_coord;
                m_LastArcDataType = ARC_INFO_TYPE_RADIUS;
            }

            continue;
        }
        else
        {
            break;
        }
    }

    if( m_Relative )
    {
        pos.x += m_CurrentPos.x;
        pos.y += m_CurrentPos.y;
    }

    m_CurrentPos = pos;
    return pos;
}


VECTOR2I GERBER_FILE_IMAGE::ReadIJCoord( char*& Text )
{
    VECTOR2I pos( 0, 0 );

    int     type_coord = 0, current_coord, nbdigits;
    bool    is_float   = false;
    char*   text;
    char    line[256];

    if( Text == nullptr )
        return pos;

    text = line;

    while( *Text )
    {
        if( ( *Text == 'I' ) || ( *Text == 'J' ) )
        {
            type_coord = *Text;
            Text++;
            text     = line;
            nbdigits = 0;

            while( IsNumber( *Text ) )
            {
                if( *Text == '.' )
                    is_float = true;

                // count digits only (sign and decimal point are not counted)
                if( ( *Text >= '0' ) && ( *Text <= '9' ) )
                    nbdigits++;

                *(text++) = *(Text++);
            }

            *text = 0;

            if( is_float )
            {
                // When X or Y values are float numbers, they are given in mm or inches
                if( m_GerbMetric )  // units are mm
                    current_coord = KiROUND( atof( line ) * IU_PER_MILS / 0.0254 );
                else    // units are inches
                    current_coord = KiROUND( atof( line ) * IU_PER_MILS * 1000 );
            }
            else
            {
                int fmt_scale = ( type_coord == 'I' ) ? m_FmtScale.x : m_FmtScale.y;

                if( m_NoTrailingZeros )
                {
                    int min_digit = ( type_coord == 'I' ) ? m_FmtLen.x : m_FmtLen.y;

                    while( nbdigits < min_digit )
                    {
                        *(text++) = '0';
                        nbdigits++;
                    }

                    *text = 0;
                }

                current_coord = atoi( line );

                double real_scale = scale_list[fmt_scale];

                if( m_GerbMetric )
                    real_scale = real_scale / 25.4;

                current_coord = KiROUND( current_coord * real_scale );
            }

            if( type_coord == 'I' )
                pos.x = current_coord;
            else if( type_coord == 'J' )
                pos.y = current_coord;

            continue;
        }
        else
        {
            break;
        }
    }

    m_IJPos = pos;
    m_LastArcDataType = ARC_INFO_TYPE_CENTER;
    m_LastCoordIsIJPos = true;

    return pos;
}


// Helper functions:

/**
 * Read an integer from an ASCII character buffer.
 *
 * If there is a comma after the integer, then skip over that.
 *
 * @param text is a reference to a character pointer from which bytes are read
 *        and the pointer is advanced for each byte read.
 * @param aSkipSeparator set to true (default) to skip comma.
 * @return The integer read in.
 */
int ReadInt( char*& text, bool aSkipSeparator = true )
{
    int ret;

    // For strtol, a string starting by 0X or 0x is a valid number in hexadecimal or octal.
    // However, 'X'  is a separator in Gerber strings with numbers.
    // We need to detect that
    if( strncasecmp( text, "0X", 2 ) == 0 )
    {
        text++;
        ret = 0;
    }
    else
    {
        ret = (int) strtol( text, &text, 10 );
    }

    if( *text == ',' || isspace( *text ) )
    {
        if( aSkipSeparator )
            ++text;
    }

    return ret;
}


/**
 * Read a double precision floating point number from an ASCII character buffer.
 *
 * If there is a comma after the number, then skip over that.
 *
 * @param text is a reference to a character pointer from which the ASCII double
 *             is read from and the pointer advanced for each character read.
 * @param aSkipSeparator set to true (default) to skip comma.
 * @return number read.
 */
double ReadDouble( char*& text, bool aSkipSeparator = true )
{
    double ret;

    // For strtod, a string starting by 0X or 0x is a valid number in hexadecimal or octal.
    // However, 'X'  is a separator in Gerber strings with numbers.
    // We need to detect that
    if( strncasecmp( text, "0X", 2 ) == 0 )
    {
        text++;
        ret = 0.0;
    }
    else
    {
        ret = strtod( text, &text );
    }

    if( *text == ',' || isspace( *text ) )
    {
        if( aSkipSeparator )
            ++text;
    }

    return ret;
}

