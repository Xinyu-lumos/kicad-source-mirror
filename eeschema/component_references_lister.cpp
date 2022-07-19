/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2018 jean-pierre Charras <jp.charras at wanadoo.fr>
 * Copyright (C) 1992-2011 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 1992-2022 KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file component_references_lister.cpp
 * @brief functions to create a symbol flat list and to annotate schematic.
 */

#include <sch_reference_list.h>
#include <core/kicad_algo.h>

#include <wx/regex.h>
#include <algorithm>
#include <vector>
#include <unordered_set>

#include <string_utils.h>
#include <erc_settings.h>
#include <sch_symbol.h>
#include <sch_edit_frame.h>


void SCH_REFERENCE_LIST::RemoveItem( unsigned int aIndex )
{
    if( aIndex < flatList.size() )
        flatList.erase( flatList.begin() + aIndex );
}


bool SCH_REFERENCE_LIST::Contains( const SCH_REFERENCE& aItem ) const
{
    for( unsigned ii = 0; ii < GetCount(); ii++ )
    {
        if( flatList[ii].IsSameInstance( aItem ) )
            return true;
    }

    return false;
}


bool SCH_REFERENCE_LIST::sortByXPosition( const SCH_REFERENCE& item1, const SCH_REFERENCE& item2 )
{
    int ii = item1.CompareRef( item2 );

    if( ii == 0 )
        ii = item1.m_sheetNum - item2.m_sheetNum;

    if( ii == 0 )
        ii = item1.m_symbolPos.x - item2.m_symbolPos.x;

    if( ii == 0 )
        ii = item1.m_symbolPos.y - item2.m_symbolPos.y;

    if( ii == 0 )
        return item1.m_symbolUuid < item2.m_symbolUuid;     // ensure a deterministic sort
    else
        return ii < 0;
}


bool SCH_REFERENCE_LIST::sortByYPosition( const SCH_REFERENCE& item1, const SCH_REFERENCE& item2 )
{
    int ii = item1.CompareRef( item2 );

    if( ii == 0 )
        ii = item1.m_sheetNum - item2.m_sheetNum;

    if( ii == 0 )
        ii = item1.m_symbolPos.y - item2.m_symbolPos.y;

    if( ii == 0 )
        ii = item1.m_symbolPos.x - item2.m_symbolPos.x;

    if( ii == 0 )
        return item1.m_symbolUuid < item2.m_symbolUuid;     // ensure a deterministic sort
    else
        return ii < 0;
}


bool SCH_REFERENCE_LIST::sortByRefAndValue( const SCH_REFERENCE& item1,
                                            const SCH_REFERENCE& item2 )
{
    int ii = item1.CompareRef( item2 );

    if( ii == 0 )
        ii = item1.CompareValue( item2 );

    if( ii == 0 )
        ii = item1.m_unit - item2.m_unit;

    if( ii == 0 )
        ii = item1.m_sheetNum - item2.m_sheetNum;

    if( ii == 0 )
        ii = item1.m_symbolPos.x - item2.m_symbolPos.x;

    if( ii == 0 )
        ii = item1.m_symbolPos.y - item2.m_symbolPos.y;

    if( ii == 0 )
        return item1.m_symbolUuid < item2.m_symbolUuid;     // ensure a deterministic sort
    else
        return ii < 0;
}


bool SCH_REFERENCE_LIST::sortByReferenceOnly( const SCH_REFERENCE& item1,
                                              const SCH_REFERENCE& item2 )
{
    int ii = StrNumCmp( item1.GetRef(), item2.GetRef(), false );

    if( ii == 0 )
        ii = item1.m_unit - item2.m_unit;

    if( ii == 0 )
        return item1.m_symbolUuid < item2.m_symbolUuid;     // ensure a deterministic sort
    else
        return ii < 0;
}


bool SCH_REFERENCE_LIST::sortByTimeStamp( const SCH_REFERENCE& item1,
                                          const SCH_REFERENCE& item2 )
{
    int ii = item1.m_sheetPath.Cmp( item2.m_sheetPath );

    if( ii == 0 )
        return item1.m_symbolUuid < item2.m_symbolUuid;     // ensure a deterministic sort
    else
        return ii < 0;
}


int SCH_REFERENCE_LIST::FindUnit( size_t aIndex, int aUnit, bool aIncludeNew ) const
{
    int NumRef = flatList[aIndex].m_numRef;

    for( size_t ii = 0; ii < flatList.size(); ii++ )
    {
        if(  ( aIndex == ii )
          || ( flatList[ii].m_isNew && !aIncludeNew )
          || ( flatList[ii].m_numRef != NumRef )
          || ( flatList[aIndex].CompareRef( flatList[ii] ) != 0 ) )
            continue;

        if( flatList[ii].m_unit == aUnit )
            return (int) ii;
    }

    return -1;
}


int SCH_REFERENCE_LIST::FindRefByPath( const wxString& aPath ) const
{
    for( size_t i = 0; i < flatList.size(); ++i )
    {
        if( flatList[i].GetPath() == aPath )
            return i;
    }

    return -1;
}


int SCH_REFERENCE_LIST::FindRef( const wxString& aRef ) const
{
    for( size_t i = 0; i < flatList.size(); ++i )
    {
        if( flatList[i].GetRef() == aRef )
            return i;
    }

    return -1;
}


void SCH_REFERENCE_LIST::GetRefsInUse( int aIndex, std::vector< int >& aIdList,
                                       int aMinRefId ) const
{
    aIdList.clear();

    for( const SCH_REFERENCE& ref : flatList )
    {
        // Don't add new references to the list as we will reannotate those
        if( flatList[aIndex].CompareRef( ref ) == 0 && ref.m_numRef >= aMinRefId && !ref.m_isNew )
            aIdList.push_back( ref.m_numRef );
    }

    std::sort( aIdList.begin(), aIdList.end() );

    // Ensure each reference number appears only once.  If there are symbols with
    // multiple parts per package the same number will be stored for each part.
    alg::remove_duplicates( aIdList );
}


std::vector<int> SCH_REFERENCE_LIST::GetUnitsMatchingRef( const SCH_REFERENCE& aRef ) const
{
    std::vector<int> unitsList;

    // Always add this reference to the list
    unitsList.push_back( aRef.m_unit );

    for( SCH_REFERENCE ref : flatList )
    {
        if( ref.CompareValue( aRef ) != 0 )
            continue;

        if( ref.CompareLibName( aRef ) != 0 )
            continue;

        // Split if needed before comparing ref and number
        if( ref.IsSplitNeeded() )
            ref.Split();

        if( ref.CompareRef( aRef ) != 0 )
            continue;

        if( ref.m_numRef != aRef.m_numRef )
            continue;

        unitsList.push_back( ref.m_unit );
    }

    std::sort( unitsList.begin(), unitsList.end() );

    // Ensure each reference number appears only once.  If there are symbols with
    // multiple parts per package the same number will be stored for each part.
    alg::remove_duplicates( unitsList );

    return unitsList;
}


int SCH_REFERENCE_LIST::FindFirstUnusedReference( const SCH_REFERENCE& aRef, int aMinValue,
                                                  const std::vector<int>& aRequiredUnits ) const
{
    // Create a map of references indexed by reference number, only including those with the same
    // reference prefix as aRef
    std::map<int, std::vector<SCH_REFERENCE>> refNumberMap;

    for( const SCH_REFERENCE& ref : flatList )
    {
        // search only for the current reference prefix:
        if( ref.CompareRef( aRef ) != 0 )
            continue;

        if( ref.m_isNew )
            continue; // It will be reannotated

        refNumberMap[ref.m_numRef].push_back( ref );
    }

    // Start at the given minimum value
    int minFreeNumber = aMinValue;

    for( ; refNumberMap[minFreeNumber].size() > 0; ++minFreeNumber )
    {
        auto isNumberInUse = [&]() -> bool
                             {
                                for( const int& unit : aRequiredUnits )
                                {
                                    for( const SCH_REFERENCE& ref : refNumberMap[minFreeNumber] )
                                    {
                                        if( ref.CompareLibName( aRef ) || ref.CompareValue( aRef )
                                            || ref.GetUnit() == unit )
                                        {
                                            return true;
                                        }
                                    }
                                }

                                return false;
                             };

        if( !isNumberInUse() )
            return minFreeNumber;
    }

    return minFreeNumber;
}


std::vector<SYMBOL_INSTANCE_REFERENCE> SCH_REFERENCE_LIST::GetSymbolInstances() const
{
    std::vector<SYMBOL_INSTANCE_REFERENCE> retval;

    for( const SCH_REFERENCE& ref : flatList )
    {
        SYMBOL_INSTANCE_REFERENCE instance;
        instance.m_Path = ref.GetPath();
        instance.m_Reference = ref.GetRef();
        instance.m_Unit = ref.GetUnit();
        instance.m_Value = ref.GetValue();
        instance.m_Footprint = ref.GetFootprint();

        retval.push_back( instance );
    }

    return retval;
}


int SCH_REFERENCE_LIST::createFirstFreeRefId( std::vector<int>& aIdList, int aFirstValue )
{
    int expectedId = aFirstValue;

    // We search for expected Id a value >= aFirstValue.
    // Skip existing Id < aFirstValue
    unsigned ii = 0;

    for( ; ii < aIdList.size(); ii++ )
    {
        if( expectedId <= aIdList[ii] )
            break;
    }

    // Ids are sorted by increasing value, from aFirstValue
    // So we search from aFirstValue the first not used value, i.e. the first hole in list.
    for( ; ii < aIdList.size(); ii++ )
    {
        if( expectedId != aIdList[ii] )    // This id is not yet used.
        {
            // Insert this free Id, in order to keep list sorted
            aIdList.insert( aIdList.begin() + ii, expectedId );
            return expectedId;
        }

        expectedId++;
    }

    // All existing Id are tested, and all values are found in use.
    // So Create a new one.
    aIdList.push_back( expectedId );
    return expectedId;
}


// A helper function to build a full reference string of a SCH_REFERENCE item
wxString buildFullReference( const SCH_REFERENCE& aItem, int aUnitNumber = -1 )
{
    wxString fullref;
    fullref = aItem.GetRef() + aItem.GetRefNumber();

    if( aUnitNumber < 0 )
        fullref << ".." << aItem.GetUnit();
    else
        fullref << ".." << aUnitNumber;

    return fullref;
}


void SCH_REFERENCE_LIST::ReannotateByOptions( ANNOTATE_ORDER_T             aSortOption,
                                              ANNOTATE_ALGO_T              aAlgoOption,
                                              int                          aStartNumber,
                                              const SCH_REFERENCE_LIST&    aAdditionalRefs,
                                              bool                         aStartAtCurrent )
{
    SplitReferences();

    // All multi-unit symbols always locked to ensure consistent re-annotation
    SCH_MULTI_UNIT_REFERENCE_MAP lockedSymbols;

    for( size_t i = 0; i < GetCount(); i++ )
    {
        SCH_REFERENCE& ref = flatList[i];
        wxString       refstr = ref.GetSymbol()->GetRef( &ref.GetSheetPath() );

        // Never lock unassigned references
        if( refstr[refstr.Len() - 1] == '?' )
            continue;

        ref.m_isNew = true; // We want to reannotate all references

        lockedSymbols[refstr].AddItem( ref );
    }

    AnnotateByOptions( aSortOption, aAlgoOption, aStartNumber, lockedSymbols, aAdditionalRefs,
                       aStartAtCurrent );
}


void SCH_REFERENCE_LIST::ReannotateDuplicates( const SCH_REFERENCE_LIST& aAdditionalReferences )
{
    ReannotateByOptions( UNSORTED, INCREMENTAL_BY_REF, 0, aAdditionalReferences, true );
}


void SCH_REFERENCE_LIST::AnnotateByOptions( ANNOTATE_ORDER_T             aSortOption,
                                            ANNOTATE_ALGO_T              aAlgoOption,
                                            int                          aStartNumber,
                                            SCH_MULTI_UNIT_REFERENCE_MAP aLockedUnitMap,
                                            const SCH_REFERENCE_LIST&    aAdditionalRefs,
                                            bool                         aStartAtCurrent )
{
    switch( aSortOption )
    {
    default:
    case SORT_BY_X_POSITION: SortByXCoordinate(); break;
    case SORT_BY_Y_POSITION: SortByYCoordinate(); break;
    }

    bool useSheetNum = false;
    int  idStep = 100;

    switch( aAlgoOption )
    {
    default:
    case INCREMENTAL_BY_REF: break;

    case SHEET_NUMBER_X_100: useSheetNum = true; break;

    case SHEET_NUMBER_X_1000:
        useSheetNum = true;
        idStep = 1000;
        break;
    }

    Annotate( useSheetNum, idStep, aStartNumber, aLockedUnitMap, aAdditionalRefs, aStartAtCurrent );
}


void SCH_REFERENCE_LIST::Annotate( bool aUseSheetNum, int aSheetIntervalId, int aStartNumber,
                                   SCH_MULTI_UNIT_REFERENCE_MAP aLockedUnitMap,
                                   const SCH_REFERENCE_LIST& aAdditionalRefs, bool aStartAtCurrent )
{
    if ( flatList.size() == 0 )
        return;

    size_t originalSize = GetCount();

    // For multi units symbols, store the list of already used full references.
    // The algorithm tries to allocate the new reference to symbols having the same
    // old reference.
    // This algo works fine as long as the previous annotation has no duplicates.
    // But when a hierarchy is reannotated with this option, the previous annotation can
    // have duplicate references, and obviously we must fix these duplicate.
    // therefore do not try to allocate a full reference more than once when trying
    // to keep this order of multi units.
    // inUseRefs keep trace of previously allocated references
    std::unordered_set<wxString> inUseRefs;

    for( size_t i = 0; i < aAdditionalRefs.GetCount(); i++ )
    {
        SCH_REFERENCE additionalRef = aAdditionalRefs[i];
        additionalRef.Split();

        // Add the additional reference to the multi-unit set if annotated
        if( !additionalRef.m_isNew )
            inUseRefs.insert( buildFullReference( additionalRef ) );

        // We don't want to reannotate the additional references even if not annotated
        // so we change the m_isNew flag to be false after splitting
        additionalRef.m_isNew = false;
        AddItem( additionalRef ); //add to this container
    }

    int LastReferenceNumber = 0;

    /* calculate index of the first symbol with the same reference prefix
     * than the current symbol.  All symbols having the same reference
     * prefix will receive a reference number with consecutive values:
     * IC .. will be set to IC4, IC4, IC5 ...
     */
    unsigned first = 0;

    // calculate the last used number for this reference prefix:
    int minRefId;

    // when using sheet number, ensure ref number >= sheet number* aSheetIntervalId
    if( aUseSheetNum )
        minRefId = flatList[first].m_sheetNum * aSheetIntervalId + 1;
    else
        minRefId = aStartNumber + 1;


    for( unsigned ii = 0; ii < flatList.size(); ii++ )
    {
        auto& ref_unit = flatList[ii];

        if( ref_unit.m_flag )
            continue;

        // Check whether this symbol is in aLockedUnitMap.
        SCH_REFERENCE_LIST* lockedList = nullptr;

        for( SCH_MULTI_UNIT_REFERENCE_MAP::value_type& pair : aLockedUnitMap )
        {
            unsigned n_refs = pair.second.GetCount();

            for( unsigned thisRefI = 0; thisRefI < n_refs; ++thisRefI )
            {
                SCH_REFERENCE &thisRef = pair.second[thisRefI];

                if( thisRef.IsSameInstance( ref_unit ) )
                {
                    lockedList = &pair.second;
                    break;
                }
            }

            if( lockedList != nullptr )
                break;
        }

        if(  ( flatList[first].CompareRef( ref_unit ) != 0 )
          || ( aUseSheetNum && ( flatList[first].m_sheetNum != ref_unit.m_sheetNum ) )  )
        {
            // New reference found: we need a new ref number for this reference
            first = ii;

            // when using sheet number, ensure ref number >= sheet number* aSheetIntervalId
            if( aUseSheetNum )
                minRefId = ref_unit.m_sheetNum * aSheetIntervalId + 1;
            else
                minRefId = aStartNumber + 1;
        }

        // Find references greater than current reference (unless not annotated)
        if( aStartAtCurrent && ref_unit.m_numRef > 0 )
            minRefId = ref_unit.m_numRef;

        // Annotation of one part per package symbols (trivial case).
        if( ref_unit.GetLibPart()->GetUnitCount() <= 1 )
        {
            if( ref_unit.m_isNew )
            {
                std::vector<int> idList;
                GetRefsInUse( first, idList, minRefId );
                LastReferenceNumber = createFirstFreeRefId( idList, minRefId );
                ref_unit.m_numRef = LastReferenceNumber;
            }

            ref_unit.m_flag  = 1;
            ref_unit.m_isNew = false;
            continue;
        }

        // If this symbol is in aLockedUnitMap, copy the annotation to all
        // symbols that are not it
        if( lockedList != nullptr )
        {
            unsigned n_refs = lockedList->GetCount();
            std::vector<int> units = lockedList->GetUnitsMatchingRef( ref_unit );

            if( ref_unit.m_isNew )
            {
                LastReferenceNumber = FindFirstUnusedReference( ref_unit, minRefId, units );
                ref_unit.m_numRef = LastReferenceNumber;
                ref_unit.m_isNew = false;
                ref_unit.m_flag = 1;
            }

            for( unsigned lockedRefI = 0; lockedRefI < n_refs; ++lockedRefI )
            {
                SCH_REFERENCE& lockedRef = ( *lockedList )[lockedRefI];

                if( lockedRef.IsSameInstance( ref_unit ) )
                {
                    // This is the symbol we're currently annotating. Hold the unit!
                    ref_unit.m_unit = lockedRef.m_unit;
                    // lock this new full reference
                    inUseRefs.insert( buildFullReference( ref_unit ) );
                }

                if( lockedRef.CompareValue( ref_unit ) != 0 )
                    continue;

                if( lockedRef.CompareLibName( ref_unit ) != 0 )
                    continue;

                // Find the matching symbol
                for( unsigned jj = ii + 1; jj < flatList.size(); jj++ )
                {
                    if( !lockedRef.IsSameInstance( flatList[jj] ) )
                        continue;

                    wxString ref_candidate = buildFullReference( ref_unit, lockedRef.m_unit );

                    // propagate the new reference and unit selection to the "old" symbol,
                    // if this new full reference is not already used (can happens when initial
                    // multiunits symbols have duplicate references)
                    if( inUseRefs.find( ref_candidate ) == inUseRefs.end() )
                    {
                        flatList[jj].m_numRef = ref_unit.m_numRef;
                        flatList[jj].m_isNew = false;
                        flatList[jj].m_flag = 1;
                        // lock this new full reference
                        inUseRefs.insert( ref_candidate );
                        break;
                    }
                }
            }
        }
        else if( ref_unit.m_isNew )
        {
            // Reference belonging to multi-unit symbol that has not yet been annotated. We don't
            // know what group this might belong to, so just find the first unused reference for
            // this specific unit. The other units will be annotated in the following passes.
            std::vector<int> units = { ref_unit.GetUnit() };
            LastReferenceNumber = FindFirstUnusedReference( ref_unit, minRefId, units );
            ref_unit.m_numRef = LastReferenceNumber;
            ref_unit.m_isNew = false;
            ref_unit.m_flag = 1;
        }
    }

    // Remove aAdditionalRefs references
    for( size_t i = originalSize; i < ( aAdditionalRefs.GetCount() + originalSize ); i++ )
        RemoveItem( originalSize );

    wxASSERT( originalSize == GetCount() ); // Make sure we didn't make a mistake
}


int SCH_REFERENCE_LIST::CheckAnnotation( ANNOTATION_ERROR_HANDLER aHandler )
{
    int            error = 0;
    wxString       tmp;
    wxString       msg;

    SortByRefAndValue();

    // Split reference designators into name (prefix) and number: IC1 becomes IC, and 1.
    SplitReferences();

    // count not yet annotated items or annotation error.
    for( unsigned ii = 0; ii < flatList.size(); ii++ )
    {
        msg.Empty();
        tmp.Empty();

        if( flatList[ii].m_isNew )    // Not yet annotated
        {
            if( flatList[ii].m_numRef >= 0 )
                tmp << flatList[ii].m_numRef;
            else
                tmp = wxT( "?" );

            if( ( flatList[ii].m_unit > 0 ) && ( flatList[ii].m_unit < 0x7FFFFFFF )  )
            {
                msg.Printf( _( "Item not annotated: %s%s (unit %d)\n" ),
                            flatList[ii].GetRef(),
                            tmp,
                            flatList[ii].m_unit );
            }
            else
            {
                msg.Printf( _( "Item not annotated: %s%s\n" ), flatList[ii].GetRef(), tmp );
            }

            aHandler( ERCE_UNANNOTATED, msg, &flatList[ii], nullptr );
            error++;
            break;
        }

        // Error if unit number selected does not exist (greater than the  number of units in
        // the symbol).  This can happen if a symbol has changed in a library after a
        // previous annotation.
        if( std::max( flatList[ii].GetLibPart()->GetUnitCount(), 1 ) < flatList[ii].m_unit )
        {
            if( flatList[ii].m_numRef >= 0 )
                tmp << flatList[ii].m_numRef;
            else
                tmp = wxT( "?" );

            msg.Printf( _( "Error: symbol %s%s%s (unit %d) exceeds units defined (%d)\n" ),
                        flatList[ii].GetRef(),
                        tmp,
                        LIB_SYMBOL::SubReference( flatList[ii].m_unit ),
                        flatList[ii].m_unit,
                        flatList[ii].GetLibPart()->GetUnitCount() );

            aHandler( ERCE_EXTRA_UNITS, msg, &flatList[ii], nullptr );
            error++;
            break;
        }
    }

    // count the duplicated elements (if all are annotated)
    int imax = flatList.size() - 1;

    for( int ii = 0; ii < imax; ii++ )
    {
        msg.Empty();
        tmp.Empty();

        if(  ( flatList[ii].CompareRef( flatList[ii + 1] ) != 0 )
          || ( flatList[ii].m_numRef != flatList[ ii + 1].m_numRef )  )
        {
            continue;
        }

        // Same reference found. If same unit, error!
        if( flatList[ii].m_unit == flatList[ ii + 1].m_unit )
        {
            if( flatList[ii].m_numRef >= 0 )
                tmp << flatList[ii].m_numRef;
            else
                tmp = wxT( "?" );

            if( ( flatList[ii].GetLibPart()->GetUnitCount() > 1 )
                && ( flatList[ii].m_unit > 0 )
                && ( flatList[ii].m_unit < 0x7FFFFFFF ) )
            {
                msg.Printf( _( "Duplicate items %s%s%s\n" ),
                            flatList[ii].GetRef(),
                            tmp,
                            LIB_SYMBOL::SubReference( flatList[ii].m_unit ) );
            }
            else
            {
                msg.Printf( _( "Duplicate items %s%s\n" ), flatList[ii].GetRef(), tmp );
            }

            aHandler( ERCE_DUPLICATE_REFERENCE, msg, &flatList[ii], &flatList[ii+1] );
            error++;
            continue;
        }

        /* Test error if units are different but number of parts per package
         * too high (ex U3 ( 1 part) and we find U3B this is an error) */
        if( flatList[ii].GetLibPart()->GetUnitCount()
            != flatList[ ii + 1].GetLibPart()->GetUnitCount()  )
        {
            if( flatList[ii].m_numRef >= 0 )
                tmp << flatList[ii].m_numRef;
            else
                tmp = wxT( "?" );

            if( ( flatList[ii].m_unit > 0 )
             && ( flatList[ii].m_unit < 0x7FFFFFFF ) )
            {
                msg.Printf( _( "Duplicate items %s%s%s\n" ),
                            flatList[ii].GetRef(),
                            tmp,
                            LIB_SYMBOL::SubReference( flatList[ii].m_unit ) );
            }
            else
            {
                msg.Printf( _( "Duplicate items %s%s\n" ), flatList[ii].GetRef(), tmp );
            }

            aHandler( ERCE_DUPLICATE_REFERENCE, msg, &flatList[ii], &flatList[ii+1] );
            error++;
        }

        // Error if values are different between units, for the same reference
        int next = ii + 1;

        if( flatList[ii].CompareValue( flatList[next] ) != 0 )
        {
            msg.Printf( _( "Different values for %s%d%s (%s) and %s%d%s (%s)" ),
                        flatList[ii].GetRef(),
                        flatList[ii].m_numRef,
                        LIB_SYMBOL::SubReference( flatList[ii].m_unit ),
                        flatList[ii].m_value,
                        flatList[next].GetRef(),
                        flatList[next].m_numRef,
                        LIB_SYMBOL::SubReference( flatList[next].m_unit ),
                        flatList[next].m_value );

            aHandler( ERCE_DIFFERENT_UNIT_VALUE, msg, &flatList[ii], &flatList[ii+1] );
            error++;
        }
    }

    return error;
}


SCH_REFERENCE::SCH_REFERENCE( SCH_SYMBOL* aSymbol, LIB_SYMBOL* aLibSymbol,
                              const SCH_SHEET_PATH& aSheetPath )
{
    wxASSERT( aSymbol != nullptr );

    m_rootSymbol = aSymbol;
    m_libPart    = aLibSymbol;     // Warning: can be nullptr for orphan symbols
                                   // (i.e. with a symbol library not found)
    m_unit       = aSymbol->GetUnitSelection( &aSheetPath );
    m_footprint  = aSymbol->GetFootprint( &aSheetPath, true );
    m_sheetPath  = aSheetPath;
    m_isNew      = false;
    m_flag       = 0;
    m_symbolUuid = aSymbol->m_Uuid;
    m_symbolPos  = aSymbol->GetPosition();
    m_sheetNum   = 0;

    if( aSymbol->GetRef( &aSheetPath ).IsEmpty() )
        aSymbol->SetRef( &aSheetPath, wxT( "DefRef?" ) );

    wxString ref = aSymbol->GetRef( &aSheetPath );
    SetRef( ref );

    m_numRef = -1;

    if( aSymbol->GetValue( &aSheetPath, false ).IsEmpty() )
        aSymbol->SetValue( &aSheetPath, wxT( "~" ) );

    m_value = aSymbol->GetValue( &aSheetPath, false );
}


void SCH_REFERENCE::Annotate()
{
    if( m_numRef < 0 )
        m_ref += '?';
    else
        m_ref = TO_UTF8( GetRef() << GetRefNumber() );

    m_rootSymbol->SetRef( &m_sheetPath, FROM_UTF8( m_ref.c_str() ) );
    m_rootSymbol->SetUnit( m_unit );
    m_rootSymbol->SetUnitSelection( &m_sheetPath, m_unit );
}


void SCH_REFERENCE::Split()
{
    std::string refText = GetRefStr();

    m_numRef = -1;

    int ll = refText.length() - 1;

    if( refText[ll] == '?' )
    {
        m_isNew = true;

        refText.erase( ll );  // delete last char

        SetRefStr( refText );
    }
    else if( isdigit( refText[ll] ) == 0 )
    {
        m_isNew = true;
    }
    else
    {
        while( ll >= 0 )
        {
            if( (refText[ll] <= ' ' ) || isdigit( refText[ll] ) )
                ll--;
            else
            {
                if( isdigit( refText[ll + 1] ) )
                {
                    // null terminated C string into cp
                    const char* cp = refText.c_str() + ll + 1;

                    m_numRef = atoi( cp );
                }

                refText.erase( ll+1 );  // delete from ll+1 to end
                break;
            }
        }

        SetRefStr( refText );
    }
}


bool SCH_REFERENCE::IsSplitNeeded()
{
    std::string refText = GetRefStr();

    int ll = refText.length() - 1;

    return ( refText[ll] == '?' ) || isdigit( refText[ll] );
}


wxString SCH_REFERENCE_LIST::Shorthand( std::vector<SCH_REFERENCE> aList )
{
    wxString retVal;
    size_t   i = 0;

    while( i < aList.size() )
    {
        wxString ref = aList[ i ].GetRef();
        int numRef = aList[ i ].m_numRef;

        size_t range = 1;

        while( i + range < aList.size()
               && aList[ i + range ].GetRef() == ref
               && aList[ i + range ].m_numRef == int( numRef + range ) )
        {
            range++;
        }

        if( !retVal.IsEmpty() )
            retVal << wxT( ", " );

        if( range == 1 )
        {
            retVal << ref << aList[ i ].GetRefNumber();
        }
        else if( range == 2 )
        {
            retVal << ref << aList[ i ].GetRefNumber();
            retVal << wxT( ", " );
            retVal << ref << aList[ i + 1 ].GetRefNumber();
        }
        else
        {
            retVal << ref << aList[ i ].GetRefNumber();
            retVal << wxT( "-" );
            retVal << ref << aList[ i + ( range - 1 ) ].GetRefNumber();
        }

        i+= range;
    }

    return retVal;
}
