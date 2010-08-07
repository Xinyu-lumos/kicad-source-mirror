/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2010 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 1992-2010 Kicad Developers, see change_log.txt for contributors.
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


#include "xnode.h"

void XNODE::Format( OUTPUTFORMATTER* out, int nestLevel ) throw( IOError )
{
    // output attributes first if they exist

    // output children if they exist.

    // output "contents" if it exists.  Use quote need checker to wrap contents if needed.

    // A good XML element will not have both children AND contents, usually one or the other.
    // children != attributes in the above statement.

//    for( XNODE*...
}


void XNODE::FormatContents( OUTPUTFORMATTER* out, int nestLevel ) throw( IOError )
{
    // overridden in ELEM_HOLDER
}


void XATTR::Format( OUTPUTFORMATTER* out, int nestLevel ) throw( IOError )
{
    // output attributes first if they exist

    // output children if they exist.

    // output "contents" if it exists.  Use quote need checker to wrap contents if needed.

    // A good XML element will not have both children AND contents, usually one or the other.
    // children != attributes in the above statement.

//    for( XNODE*...
}


void XATTR::FormatContents( OUTPUTFORMATTER* out, int nestLevel ) throw( IOError )
{
    // overridden in ELEM_HOLDER
}


// EOF
