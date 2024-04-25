/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 CERN
 * Copyright (C) 2021-2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * @author Wayne Stambaugh <stambaughw@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCH_IO_KICAD_SEXPR_H_
#define SCH_IO_KICAD_SEXPR_H_

#include <memory>
#include <sch_io/sch_io.h>
#include <sch_io/sch_io_mgr.h>
#include <sch_file_versions.h>
#include <sch_sheet_path.h>
#include <stack>
#include <wildcards_and_files_ext.h>
#include <wx/string.h>


class KIWAY;
class LINE_READER;
class SCH_SCREEN;
class SCH_SHEET;
struct SCH_SHEET_INSTANCE;
class SCH_BITMAP;
class SCH_JUNCTION;
class SCH_NO_CONNECT;
class SCH_LINE;
class SCH_SHAPE;
class SCH_RULE_AREA;
class SCH_BUS_ENTRY_BASE;
class SCH_TEXT;
class SCH_TEXTBOX;
class SCH_TABLE;
class SCH_SYMBOL;
class SCH_FIELD;
struct SCH_SYMBOL_INSTANCE;
class STRING_UTF8_MAP;
class EE_SELECTION;
class SCH_IO_KICAD_SEXPR_LIB_CACHE;
class LIB_SYMBOL;
class SYMBOL_LIB;
class BUS_ALIAS;

/**
 * A #SCH_IO derivation for loading schematic files using the new s-expression
 * file format.
 *
 * As with all SCH_IOs there is no UI dependencies i.e. windowing calls allowed.
 */
class SCH_IO_KICAD_SEXPR : public SCH_IO
{
public:

    SCH_IO_KICAD_SEXPR();
    virtual ~SCH_IO_KICAD_SEXPR();

    const IO_BASE::IO_FILE_DESC GetSchematicFileDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "KiCad s-expression schematic files" ),
                                      { FILEEXT::KiCadSchematicFileExtension } );
    }

    const IO_BASE::IO_FILE_DESC GetLibraryDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "KiCad symbol library files" ),
                                      { FILEEXT::KiCadSymbolLibFileExtension } );
    }

    /**
     * The property used internally by the plugin to enable cache buffering which prevents
     * the library file from being written every time the cache is changed.  This is useful
     * when writing the schematic cache library file or saving a library to a new file name.
     */
    static const char* PropBuffering;

    int GetModifyHash() const override;

    SCH_SHEET* LoadSchematicFile( const wxString& aFileName, SCHEMATIC* aSchematic,
                                  SCH_SHEET*             aAppendToMe = nullptr,
                                  const STRING_UTF8_MAP* aProperties = nullptr ) override;

    void LoadContent( LINE_READER& aReader, SCH_SHEET* aSheet,
                      int aVersion = SEXPR_SCHEMATIC_FILE_VERSION );

    void SaveSchematicFile( const wxString& aFileName, SCH_SHEET* aSheet, SCHEMATIC* aSchematic,
                            const STRING_UTF8_MAP* aProperties = nullptr ) override;

    void Format( SCH_SHEET* aSheet );

    void Format( EE_SELECTION* aSelection, SCH_SHEET_PATH* aSelectionPath,
                 SCHEMATIC& aSchematic, OUTPUTFORMATTER* aFormatter, bool aForClipboard );

    void EnumerateSymbolLib( wxArrayString&    aSymbolNameList,
                             const wxString&   aLibraryPath,
                             const STRING_UTF8_MAP* aProperties = nullptr ) override;
    void EnumerateSymbolLib( std::vector<LIB_SYMBOL*>& aSymbolList,
                             const wxString&           aLibraryPath,
                             const STRING_UTF8_MAP*         aProperties = nullptr ) override;
    LIB_SYMBOL* LoadSymbol( const wxString& aLibraryPath, const wxString& aAliasName,
                            const STRING_UTF8_MAP* aProperties = nullptr ) override;
    void SaveSymbol( const wxString& aLibraryPath, const LIB_SYMBOL* aSymbol,
                     const STRING_UTF8_MAP* aProperties = nullptr ) override;
    void DeleteSymbol( const wxString& aLibraryPath, const wxString& aSymbolName,
                       const STRING_UTF8_MAP* aProperties = nullptr ) override;
    void CreateLibrary( const wxString& aLibraryPath,
                        const STRING_UTF8_MAP* aProperties = nullptr ) override;
    bool DeleteLibrary( const wxString& aLibraryPath,
                        const STRING_UTF8_MAP* aProperties = nullptr ) override;
    void SaveLibrary( const wxString& aLibraryPath,
                      const STRING_UTF8_MAP* aProperties = nullptr ) override;

    bool IsLibraryWritable( const wxString& aLibraryPath ) override;

    void GetAvailableSymbolFields( std::vector<wxString>& aNames ) override;
    void GetDefaultSymbolFields( std::vector<wxString>& aNames ) override;

    const wxString& GetError() const override { return m_error; }

    static std::vector<LIB_SYMBOL*> ParseLibSymbols( std::string& aSymbolText,
                                                     std::string  aSource,
                                                     int aFileVersion = SEXPR_SCHEMATIC_FILE_VERSION );
    static void FormatLibSymbol( LIB_SYMBOL* aPart, OUTPUTFORMATTER& aFormatter );

private:
    void loadHierarchy( const SCH_SHEET_PATH& aParentSheetPath, SCH_SHEET* aSheet );
    void loadFile( const wxString& aFileName, SCH_SHEET* aSheet );

    void saveSymbol( SCH_SYMBOL* aSymbol, const SCHEMATIC& aSchematic, int aNestLevel,
                     bool aForClipboard, const SCH_SHEET_PATH* aRelativePath = nullptr );
    void saveField( SCH_FIELD* aField, int aNestLevel );
    void saveBitmap( SCH_BITMAP* aBitmap, int aNestLevel );
    void saveSheet( SCH_SHEET* aSheet, int aNestLevel );
    void saveJunction( SCH_JUNCTION* aJunction, int aNestLevel );
    void saveNoConnect( SCH_NO_CONNECT* aNoConnect, int aNestLevel );
    void saveBusEntry( SCH_BUS_ENTRY_BASE* aBusEntry, int aNestLevel );
    void saveLine( SCH_LINE* aLine, int aNestLevel );
    void saveShape( SCH_SHAPE* aShape, int aNestLevel );
    void saveRuleArea( SCH_RULE_AREA* aRuleArea, int aNestLevel );
    void saveText( SCH_TEXT* aText, int aNestLevel );
    void saveTextBox( SCH_TEXTBOX* aText, int aNestLevel );
    void saveTable( SCH_TABLE* aTable, int aNestLevel );
    void saveBusAlias( std::shared_ptr<BUS_ALIAS> aAlias, int aNestLevel );
    void saveInstances( const std::vector<SCH_SHEET_INSTANCE>& aSheets, int aNestLevel );

    void cacheLib( const wxString& aLibraryFileName, const STRING_UTF8_MAP* aProperties );
    bool isBuffering( const STRING_UTF8_MAP* aProperties );

protected:
    int                     m_version;          ///< Version of file being loaded.
    int                     m_nextFreeFieldId;
    bool                    m_appending;        ///< Schematic load append status.
    wxString                m_error;            ///< For throwing exceptions or errors on partial
                                                ///<  loads.

    wxString                m_path;             ///< Root project path for loading child sheets.
    std::stack<wxString>    m_currentPath;      ///< Stack to maintain nested sheet paths
    SCH_SHEET*              m_rootSheet;        ///< The root sheet of the schematic being loaded.
    SCH_SHEET_PATH          m_currentSheetPath;
    SCHEMATIC*              m_schematic;
    OUTPUTFORMATTER*        m_out;              ///< The formatter for saving SCH_SCREEN objects.
    SCH_IO_KICAD_SEXPR_LIB_CACHE* m_cache;

    /// initialize PLUGIN like a constructor would.
    void init( SCHEMATIC* aSchematic, const STRING_UTF8_MAP* aProperties = nullptr );
};

#endif  // SCH_IO_KICAD_SEXPR_H_
