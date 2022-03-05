/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
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

#include <kiface_base.h>
#include <confirm.h>
#include <pcb_edit_frame.h>
#include <pcbnew_settings.h>
#include <zones.h>
#include <bitmaps.h>
#include <widgets/unit_binder.h>
#include <zone.h>
#include <pad.h>
#include <board.h>
#include <trigo.h>
#include <eda_pattern_match.h>

#include <dialog_copper_zones_base.h>
#include <string_utils.h>


class DIALOG_COPPER_ZONE : public DIALOG_COPPER_ZONE_BASE
{
public:
    DIALOG_COPPER_ZONE( PCB_BASE_FRAME* aParent, ZONE_SETTINGS* aSettings );

private:
    using NET_FILTER = std::unique_ptr<EDA_PATTERN_MATCH>;
    using NET_FILTER_LIST = std::vector<NET_FILTER>;

    static constexpr int INVALID_NET_CODE{ 0 };

    static constexpr int DEFAULT_SORT_CONFIG{ -1 };
    static constexpr int NO_PERSISTENT_SORT_MODE{ 0 };
    static constexpr int HIDE_ANONYMOUS_NETS{ 1 << 0 };
    static constexpr int SORT_BY_PAD_COUNT{ 1 << 1 };

    PCB_BASE_FRAME*  m_Parent;

    bool             m_settingsExported;     // settings will be written to all other zones

    ZONE_SETTINGS    m_settings;
    ZONE_SETTINGS*   m_ptr;
    bool             m_netSortingByPadCount;
    NET_FILTER_LIST  m_showNetsFilter;

    int              m_cornerSmoothingType;
    int              m_maxNetCode;
    int              m_currentlySelectedNetcode;
    UNIT_BINDER      m_outlineHatchPitch;

    UNIT_BINDER      m_cornerRadius;
    UNIT_BINDER      m_clearance;
    UNIT_BINDER      m_minWidth;
    UNIT_BINDER      m_antipadClearance;
    UNIT_BINDER      m_spokeWidth;

    UNIT_BINDER      m_gridStyleRotation;
    UNIT_BINDER      m_gridStyleThickness;
    UNIT_BINDER      m_gridStyleGap;
    UNIT_BINDER      m_islandThreshold;
    bool             m_hideAutoGeneratedNets;
    bool             m_isTeardrop;

    std::map<wxString, int>    m_netNameToNetCode;
    std::vector<NETINFO_ITEM*> m_netInfoItemList;

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

    /**
     * @param aUseExportableSetupOnly is true to use exportable parameters only (used to
     *                                export this setup to other zones).
     * @return bool - false if incorrect options, true if ok.
     */
    bool AcceptOptions( bool aUseExportableSetupOnly = false );

    void OnStyleSelection( wxCommandEvent& event ) override;
    void OnLayerSelection( wxDataViewEvent& event ) override;
    void OnNetSortingOptionSelected( wxCommandEvent& event ) override;
    void ExportSetupToOtherCopperZones( wxCommandEvent& event ) override;
    void OnShowNetNameFilterChange( wxCommandEvent& event ) override;
    void OnUpdateUI( wxUpdateUIEvent& ) override;
    void OnButtonCancelClick( wxCommandEvent& event ) override;
    void OnClose( wxCloseEvent& event ) override;
    void OnNetSelectionUpdated( wxCommandEvent& event ) override;
    void OnRemoveIslandsSelection( wxCommandEvent& event ) override;

    void          readNetInformation();
    void          readFilteringAndSortingCriteria();
    void          buildListOfNets( const NETINFO_LIST& nets );
    wxArrayString buildListOfNetsToDisplay();
    void          sortNetsByPadCount( std::vector<NETINFO_ITEM*>& nets, const int maxNetCode );
    void          updateDisplayedListOfNets();
    int           ensureSelectedNetIsVisible( int selectedNetCode, wxArrayString& netsList );
    void          displayNetsList( const wxArrayString& netNamesList, int selectIndex );
    void          updateShowNetsFilter();
    wxString      getUnescapedNetName( const NETINFO_ITEM* net );
    void          sortNetsIfRequired();
    wxArrayString getSortedNetNamesList();
    wxArrayString applyShowFilter( const wxArrayString& sortedNetNames );
    wxArrayString applyHideFilterIfRequired( const wxArrayString& netNames );
    bool          isAutoGenerated( const wxString& netName );
    void          updateCurrentNetSelection();
    void          updateInfoBar();
    void          handleRemoveIslandsSelection();
    void          storePersistentNetSortConfigurations();
    void          loadPersistentNetSortConfigurations();
};


int InvokeCopperZonesEditor( PCB_BASE_FRAME* aCaller, ZONE_SETTINGS* aSettings )
{
    DIALOG_COPPER_ZONE dlg( aCaller, aSettings );

    return dlg.ShowQuasiModal();
}


// The pad count for each netcode, stored in a buffer for a fast access.
// This is needed by the sort function sortNetsByNodes()
static std::vector<int> padCountListByNet;


// Sort nets by decreasing pad count.
// For same pad count, sort by alphabetic names
static bool sortNetsByNodes( const NETINFO_ITEM* a, const NETINFO_ITEM* b )
{
    int countA = padCountListByNet[a->GetNetCode()];
    int countB = padCountListByNet[b->GetNetCode()];

    if( countA == countB )
        return a->GetNetname() < b->GetNetname();
    else
        return countB < countA;
}


// Sort nets by alphabetic names
static bool sortNetsByNames( const NETINFO_ITEM* a, const NETINFO_ITEM* b )
{
    return a->GetNetname() < b->GetNetname();
}


DIALOG_COPPER_ZONE::DIALOG_COPPER_ZONE( PCB_BASE_FRAME* aParent, ZONE_SETTINGS* aSettings ) :
        DIALOG_COPPER_ZONE_BASE( aParent ),
        m_cornerSmoothingType( ZONE_SETTINGS::SMOOTHING_UNDEFINED ),
        m_outlineHatchPitch( aParent, m_stBorderHatchPitchText,
                             m_outlineHatchPitchCtrl, m_outlineHatchUnits ),
        m_cornerRadius( aParent, m_cornerRadiusLabel, m_cornerRadiusCtrl, m_cornerRadiusUnits ),
        m_clearance( aParent, m_clearanceLabel, m_clearanceCtrl, m_clearanceUnits ),
        m_minWidth( aParent, m_minWidthLabel, m_minWidthCtrl, m_minWidthUnits ),
        m_antipadClearance( aParent, m_antipadLabel, m_antipadCtrl, m_antipadUnits ),
        m_spokeWidth( aParent, m_spokeWidthLabel, m_spokeWidthCtrl, m_spokeWidthUnits ),
        m_gridStyleRotation( aParent, m_staticTextGrindOrient, m_tcGridStyleOrientation,
                             m_staticTextRotUnits ),
        m_gridStyleThickness( aParent, m_staticTextStyleThickness, m_tcGridStyleThickness,
                              m_GridStyleThicknessUnits ),
        m_gridStyleGap( aParent, m_staticTextGridGap, m_tcGridStyleGap, m_GridStyleGapUnits ),
        m_islandThreshold( aParent, m_islandThresholdLabel, m_tcIslandThreshold,
                           m_islandThresholdUnits ),
        m_hideAutoGeneratedNets{ false }
{
    m_Parent = aParent;

    m_ptr = aSettings;
    m_settings = *aSettings;
    m_settings.SetupLayersList( m_layers, m_Parent,
                                LSET::AllCuMask( aParent->GetBoard()->GetCopperLayerCount() ),
                                false );
    m_isTeardrop = m_settings.m_TeardropType != TEARDROP_TYPE::TD_NONE;

    switch( m_settings.m_TeardropType )
    {
    case TEARDROP_TYPE::TD_NONE:
        // standard copper zone
        break;

    case TEARDROP_TYPE::TD_VIAPAD:
        SetTitle( _( "Teardrop on Vias/Pads Properties" ) );
        break;

    case TEARDROP_TYPE::TD_TRACKEND:
        SetTitle( _( "Teardrop on Tracks Properties" ) );
        break;

    default:
        SetTitle( _( "Teardrop Properties" ) );
        break;
    }

    m_settingsExported = false;
    m_currentlySelectedNetcode = INVALID_NET_CODE;
    m_maxNetCode = INVALID_NET_CODE;

    m_netSortingByPadCount = true;      // false = alphabetic sort, true = pad count sort

    m_ShowNetNameFilter->SetHint( _( "Filter" ) );

    m_cbRemoveIslands->Bind( wxEVT_CHOICE,
            [&]( wxCommandEvent& )
            {
                // Area mode is index 2
                bool val = m_cbRemoveIslands->GetSelection() == 2;

                m_tcIslandThreshold->Enable( val );
                m_islandThresholdLabel->Enable( val );
                m_islandThresholdUnits->Enable( val );
            } );

    SetupStandardButtons();

    finishDialogSettings();
}


bool DIALOG_COPPER_ZONE::TransferDataToWindow()
{
    m_constrainOutline->SetValue( m_settings.m_Zone_45_Only );
    m_cbLocked->SetValue( m_settings.m_Locked );
    m_cornerSmoothingChoice->SetSelection( m_settings.GetCornerSmoothingType() );
    m_cornerRadius.SetValue( m_settings.GetCornerRadius() );
    m_PriorityLevelCtrl->SetValue( m_settings.m_ZonePriority );

    if( m_isTeardrop )  // outlines are never smoothed: they have alreay the right shape
    {
        m_cornerSmoothingChoice->SetSelection( 0 );
        m_cornerSmoothingChoice->Enable( false );
        m_cornerRadius.SetValue( 0 );
        m_cornerRadius.Enable( false );
    }

    switch( m_settings.m_ZoneBorderDisplayStyle )
    {
    case ZONE_BORDER_DISPLAY_STYLE::NO_HATCH:      m_OutlineDisplayCtrl->SetSelection( 0 ); break;
    case ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_EDGE: m_OutlineDisplayCtrl->SetSelection( 1 ); break;
    case ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_FULL: m_OutlineDisplayCtrl->SetSelection( 2 ); break;
    }

    m_outlineHatchPitch.SetValue( m_settings.m_BorderHatchPitch );

    m_clearance.SetValue( m_settings.m_ZoneClearance );
    m_minWidth.SetValue( m_settings.m_ZoneMinThickness );

    switch( m_settings.GetPadConnection() )
    {
    default:
    case ZONE_CONNECTION::THERMAL:     m_PadInZoneOpt->SetSelection( 1 ); break;
    case ZONE_CONNECTION::THT_THERMAL: m_PadInZoneOpt->SetSelection( 2 ); break;
    case ZONE_CONNECTION::NONE:        m_PadInZoneOpt->SetSelection( 3 ); break;
    case ZONE_CONNECTION::FULL:        m_PadInZoneOpt->SetSelection( 0 ); break;
    }

    if( m_isTeardrop )
    {
        m_PadInZoneOpt->SetSelection( 0 );
        m_PadInZoneOpt->Enable( false );
    }

    // Do not enable/disable antipad clearance and spoke width.  They might be needed if
    // a footprint or pad overrides the zone to specify a thermal connection.
    m_antipadClearance.SetValue( m_settings.m_ThermalReliefGap );
    m_spokeWidth.SetValue( m_settings.m_ThermalReliefSpokeWidth );

    m_islandThreshold.SetDataType( EDA_DATA_TYPE::AREA );
    m_islandThreshold.SetDoubleValue( static_cast<double>( m_settings.GetMinIslandArea() ) );

    m_cbRemoveIslands->SetSelection( static_cast<int>( m_settings.GetIslandRemovalMode() ) );

    bool val = m_settings.GetIslandRemovalMode() == ISLAND_REMOVAL_MODE::AREA;

    m_tcIslandThreshold->Enable( val );
    m_islandThresholdLabel->Enable( val );
    m_islandThresholdUnits->Enable( val );

    loadPersistentNetSortConfigurations();

    m_sortByPadsOpt->SetValue( m_netSortingByPadCount );
    m_hideAutoGenNetNamesOpt->SetValue( m_hideAutoGeneratedNets );

    m_currentlySelectedNetcode = m_settings.m_NetcodeSelection;

    // Initialize information required to display nets list
    readNetInformation();

    if( !m_isTeardrop && m_settings.m_FillMode == ZONE_FILL_MODE::HATCH_PATTERN )
        m_GridStyleCtrl->SetSelection( 1 );
    else
        m_GridStyleCtrl->SetSelection( 0 );

    m_GridStyleCtrl->Enable( !m_isTeardrop );

    m_gridStyleRotation.SetUnits( EDA_UNITS::DEGREES );
    m_gridStyleRotation.SetAngleValue( m_settings.m_HatchOrientation );

    // Gives a reasonable value to grid style parameters, if currently there are no defined
    // parameters for grid pattern thickness and gap (if the value is 0)
    // the grid pattern thickness default value is (arbitrary) m_ZoneMinThickness * 4
    // or 1mm
    // the grid pattern gap default value is (arbitrary) m_ZoneMinThickness * 6
    // or 1.5 mm
    int bestvalue = m_settings.m_HatchThickness;

    if( bestvalue <= 0 )     // No defined value for m_HatchThickness
        bestvalue = std::max( m_settings.m_ZoneMinThickness * 4, Millimeter2iu( 1.0 ) );

    m_gridStyleThickness.SetValue( std::max( bestvalue, m_settings.m_ZoneMinThickness ) );

    bestvalue = m_settings.m_HatchGap;

    if( bestvalue <= 0 )     // No defined value for m_HatchGap
        bestvalue = std::max( m_settings.m_ZoneMinThickness * 6, Millimeter2iu( 1.5 ) );

    m_gridStyleGap.SetValue( std::max( bestvalue, m_settings.m_ZoneMinThickness ) );

    m_spinCtrlSmoothLevel->SetValue( m_settings.m_HatchSmoothingLevel );
    m_spinCtrlSmoothValue->SetValue( m_settings.m_HatchSmoothingValue );

    m_tcZoneName->SetValue( m_settings.m_Name );

    updateInfoBar();
    handleRemoveIslandsSelection();

    updateDisplayedListOfNets();

    SetInitialFocus( m_ShowNetNameFilter );

    // Enable/Disable some widgets
    wxCommandEvent event;
    OnStyleSelection( event );

    Fit();

    return true;
}


void DIALOG_COPPER_ZONE::readNetInformation()
{
    NETINFO_LIST& netInfoList = m_Parent->GetBoard()->GetNetInfo();

    if( netInfoList.GetNetCount() > 0 )
    {
        buildListOfNets( netInfoList );
    }
}


void DIALOG_COPPER_ZONE::buildListOfNets( const NETINFO_LIST& nets )
{
    m_netInfoItemList.clear();
    m_netInfoItemList.reserve( nets.GetNetCount() );

    m_netNameToNetCode.clear();
    m_netNameToNetCode[wxT( "<no net>" )] = INVALID_NET_CODE;

    m_maxNetCode = INVALID_NET_CODE;

    for( NETINFO_ITEM* net : nets )
    {
        const int& netCode = net->GetNetCode();
        const wxString& netName = getUnescapedNetName( net );

        m_netNameToNetCode[netName] = netCode;

        if( netCode > INVALID_NET_CODE && net->IsCurrent() )
        {
            m_netInfoItemList.push_back( net );
            m_maxNetCode = std::max( netCode, m_maxNetCode );
        }
    }
}


void DIALOG_COPPER_ZONE::OnUpdateUI( wxUpdateUIEvent& )
{
    if( m_cornerSmoothingType != m_cornerSmoothingChoice->GetSelection() )
    {
        m_cornerSmoothingType = m_cornerSmoothingChoice->GetSelection();

        if( m_cornerSmoothingChoice->GetSelection() == ZONE_SETTINGS::SMOOTHING_CHAMFER )
            m_cornerRadiusLabel->SetLabel( _( "Chamfer distance:" ) );
        else
            m_cornerRadiusLabel->SetLabel( _( "Fillet radius:" ) );
    }

    m_cornerRadiusCtrl->Enable(m_cornerSmoothingType > ZONE_SETTINGS::SMOOTHING_NONE );
}


void DIALOG_COPPER_ZONE::OnButtonCancelClick( wxCommandEvent& event )
{
    // After an "Export Settings to Other Zones" cancel and close must return
    // ZONE_EXPORT_VALUES instead of wxID_CANCEL.
    Close( true );
}


void DIALOG_COPPER_ZONE::OnNetSelectionUpdated( wxCommandEvent& event )
{
    updateCurrentNetSelection();

    updateInfoBar();

    // When info bar is updated, the nets-list shrinks.
    // Therefore, we need to reestablish the list and maintain the
    // correct selection
    updateDisplayedListOfNets();

    handleRemoveIslandsSelection();
}


void DIALOG_COPPER_ZONE::OnRemoveIslandsSelection( wxCommandEvent& event )
{
    handleRemoveIslandsSelection();
}


void DIALOG_COPPER_ZONE::handleRemoveIslandsSelection()
{
    bool noNetSelected = m_currentlySelectedNetcode == INVALID_NET_CODE;
    bool enableSize = !noNetSelected && ( m_cbRemoveIslands->GetSelection() == 2 );

    // Zones with no net never have islands removed
    m_cbRemoveIslands->Enable( !noNetSelected );
    m_islandThresholdLabel->Enable( enableSize );
    m_islandThresholdUnits->Enable( enableSize );
    m_tcIslandThreshold->Enable( enableSize );
}


bool DIALOG_COPPER_ZONE::TransferDataFromWindow()
{
    if( m_GridStyleCtrl->GetSelection() > 0 )
        m_settings.m_FillMode = ZONE_FILL_MODE::HATCH_PATTERN;
    else
        m_settings.m_FillMode = ZONE_FILL_MODE::POLYGONS;

    if( !AcceptOptions() )
        return false;

    m_settings.m_HatchOrientation = m_gridStyleRotation.GetAngleValue();
    m_settings.m_HatchThickness = m_gridStyleThickness.GetValue();
    m_settings.m_HatchGap = m_gridStyleGap.GetValue();
    m_settings.m_HatchSmoothingLevel = m_spinCtrlSmoothLevel->GetValue();
    m_settings.m_HatchSmoothingValue = m_spinCtrlSmoothValue->GetValue();

    *m_ptr = m_settings;
    return true;
}


void DIALOG_COPPER_ZONE::OnClose( wxCloseEvent& event )
{
    SetReturnCode( m_settingsExported ? ZONE_EXPORT_VALUES : wxID_CANCEL );
    event.Skip();
}


bool DIALOG_COPPER_ZONE::AcceptOptions( bool aUseExportableSetupOnly )
{
    if( !m_clearance.Validate( 0, Mils2iu( ZONE_CLEARANCE_MAX_VALUE_MIL ) ) )
        return false;

    if( !m_minWidth.Validate( Mils2iu( ZONE_THICKNESS_MIN_VALUE_MIL ), INT_MAX ) )
        return false;

    if( !m_cornerRadius.Validate( 0, INT_MAX ) )
        return false;

    if( !m_spokeWidth.Validate( 0, INT_MAX ) )
        return false;

    m_gridStyleRotation.SetValue( NormalizeAngle180( m_gridStyleRotation.GetValue() ) );

    if( m_settings.m_FillMode == ZONE_FILL_MODE::HATCH_PATTERN )
    {
        int minThickness = m_minWidth.GetValue();

        if( !m_gridStyleThickness.Validate( minThickness, INT_MAX ) )
            return false;

        if( !m_gridStyleGap.Validate( minThickness, INT_MAX ) )
            return false;
    }

    switch( m_PadInZoneOpt->GetSelection() )
    {
    case 3: m_settings.SetPadConnection( ZONE_CONNECTION::NONE );        break;
    case 2: m_settings.SetPadConnection( ZONE_CONNECTION::THT_THERMAL ); break;
    case 1: m_settings.SetPadConnection( ZONE_CONNECTION::THERMAL );     break;
    case 0: m_settings.SetPadConnection( ZONE_CONNECTION::FULL );        break;
    }

    switch( m_OutlineDisplayCtrl->GetSelection() )
    {
    case 0: m_settings.m_ZoneBorderDisplayStyle = ZONE_BORDER_DISPLAY_STYLE::NO_HATCH;      break;
    case 1: m_settings.m_ZoneBorderDisplayStyle = ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_EDGE; break;
    case 2: m_settings.m_ZoneBorderDisplayStyle = ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_FULL; break;
    }

    if( !m_outlineHatchPitch.Validate( Millimeter2iu( ZONE_BORDER_HATCH_MINDIST_MM ),
                                       Millimeter2iu( ZONE_BORDER_HATCH_MAXDIST_MM ) ) )
        return false;

    m_settings.m_BorderHatchPitch = m_outlineHatchPitch.GetValue();

    PCBNEW_SETTINGS* cfg = m_Parent->GetPcbNewSettings();

    cfg->m_Zones.hatching_style = static_cast<int>( m_settings.m_ZoneBorderDisplayStyle );

    m_settings.m_ZoneClearance = m_clearance.GetValue();
    m_settings.m_ZoneMinThickness = m_minWidth.GetValue();

    m_settings.SetCornerSmoothingType( m_cornerSmoothingChoice->GetSelection() );

    m_settings.SetCornerRadius( m_settings.GetCornerSmoothingType() == ZONE_SETTINGS::SMOOTHING_NONE
                                ? 0 : m_cornerRadius.GetValue() );

    m_settings.m_ZonePriority = m_PriorityLevelCtrl->GetValue();

    m_settings.m_Zone_45_Only = m_constrainOutline->GetValue();
    m_settings.m_Locked = m_cbLocked->GetValue();

    m_settings.m_ThermalReliefGap = m_antipadClearance.GetValue();
    m_settings.m_ThermalReliefSpokeWidth = m_spokeWidth.GetValue();

    if( m_settings.m_ThermalReliefSpokeWidth < m_settings.m_ZoneMinThickness )
    {
        DisplayError( this, _( "Thermal spoke width cannot be smaller than the minimum width." ) );
        return false;
    }

    storePersistentNetSortConfigurations();
    cfg->m_Zones.clearance                   = Iu2Mils( m_settings.m_ZoneClearance );
    cfg->m_Zones.min_thickness               = Iu2Mils( m_settings.m_ZoneMinThickness );
    cfg->m_Zones.thermal_relief_gap          = Iu2Mils( m_settings.m_ThermalReliefGap );
    cfg->m_Zones.thermal_relief_copper_width = Iu2Mils( m_settings.m_ThermalReliefSpokeWidth );

    m_settings.SetIslandRemovalMode(
            static_cast<ISLAND_REMOVAL_MODE>( m_cbRemoveIslands->GetSelection() ) );
    m_settings.SetMinIslandArea( m_islandThreshold.GetValue() );

    // If we use only exportable to others zones parameters, exit here:
    if( aUseExportableSetupOnly )
        return true;

    // Get the layer selection for this zone
    int layers = 0;

    for( int ii = 0; ii < m_layers->GetItemCount(); ++ii )
    {
        if( m_layers->GetToggleValue( (unsigned) ii, 0 ) )
            layers++;
    }

    if( layers == 0 )
    {
        DisplayError( this, _( "No layer selected." ) );
        return false;
    }

    m_settings.m_NetcodeSelection = m_currentlySelectedNetcode;

    m_settings.m_Name = m_tcZoneName->GetValue();

    return true;
}


void DIALOG_COPPER_ZONE::updateCurrentNetSelection()
{
    const int netSelection{ m_ListNetNameSelection->GetSelection() };

    if( netSelection )
    {
        const wxString& selectedNetName = m_ListNetNameSelection->GetString( netSelection );
        m_currentlySelectedNetcode = m_netNameToNetCode[selectedNetName];
    }
    else
    {
        m_currentlySelectedNetcode = INVALID_NET_CODE;
    }
}


void DIALOG_COPPER_ZONE::OnStyleSelection( wxCommandEvent& event )
{
    bool enable = m_GridStyleCtrl->GetSelection() >= 1;
    m_tcGridStyleThickness->Enable( enable );
    m_tcGridStyleGap->Enable( enable );
    m_tcGridStyleOrientation->Enable( enable );
    m_spinCtrlSmoothLevel->Enable( enable );
    m_spinCtrlSmoothValue->Enable( enable );
}


void DIALOG_COPPER_ZONE::OnLayerSelection( wxDataViewEvent& event )
{
    if( event.GetColumn() != 0 )
        return;

    int row = m_layers->ItemToRow( event.GetItem() );

    bool checked = m_layers->GetToggleValue( row, 0 );

    wxVariant layerID;
    m_layers->GetValue( layerID, row, 2 );

    m_settings.m_Layers.set( ToLAYER_ID( layerID.GetInteger() ), checked );
}


void DIALOG_COPPER_ZONE::OnNetSortingOptionSelected( wxCommandEvent& event )
{
    updateDisplayedListOfNets();
}


void DIALOG_COPPER_ZONE::storePersistentNetSortConfigurations()
{
    // These configurations are persistent across multiple invokations of
    // this dialog
    int newConfig{ NO_PERSISTENT_SORT_MODE };

    if( m_hideAutoGeneratedNets )
    {
        newConfig |= HIDE_ANONYMOUS_NETS;
    }

    if( m_netSortingByPadCount )
    {
        newConfig |= SORT_BY_PAD_COUNT;
    }

    PCBNEW_SETTINGS* cfg = m_Parent->GetPcbNewSettings();
    cfg->m_Zones.net_sort_mode = newConfig;
}


void DIALOG_COPPER_ZONE::loadPersistentNetSortConfigurations()
{
    PCBNEW_SETTINGS* cfg{ m_Parent->GetPcbNewSettings() };
    int savedConfig{ cfg->m_Zones.net_sort_mode };

    if( savedConfig == DEFAULT_SORT_CONFIG )
    {
        savedConfig = HIDE_ANONYMOUS_NETS;
    }

    m_hideAutoGeneratedNets = ( savedConfig & HIDE_ANONYMOUS_NETS );
    m_netSortingByPadCount = ( savedConfig & SORT_BY_PAD_COUNT );
}


void DIALOG_COPPER_ZONE::ExportSetupToOtherCopperZones( wxCommandEvent& event )
{
    if( !AcceptOptions( true ) )
        return;

    // Export settings ( but layer and netcode ) to others copper zones
    BOARD* pcb = m_Parent->GetBoard();

    for( ZONE* zone : pcb->Zones() )
    {
        // Cannot export settings from a copper zone
        // to a zone keepout:
        if( zone->GetIsRuleArea() )
            continue;

        // Export only to similar zones:
        // Teardrop area -> teardrop area of same type
        // copper zone -> copper zone
        // Exporting current settings to a different zone type make no sense
        if( m_settings.m_TeardropType != zone->GetTeardropAreaType() )
            continue;

        m_settings.ExportSetting( *zone, false );  // false = partial export
        m_settingsExported = true;
        m_Parent->OnModify();
    }
}


void DIALOG_COPPER_ZONE::OnShowNetNameFilterChange( wxCommandEvent& event )
{
    updateDisplayedListOfNets();
}


void DIALOG_COPPER_ZONE::updateDisplayedListOfNets()
{
    readFilteringAndSortingCriteria();

    wxArrayString listOfNets = buildListOfNetsToDisplay();

    const int selectedNet = ensureSelectedNetIsVisible( m_currentlySelectedNetcode, listOfNets );

    displayNetsList( listOfNets, selectedNet );
}


void DIALOG_COPPER_ZONE::readFilteringAndSortingCriteria()
{
    updateShowNetsFilter();

    // Hide nets filter criteria
    m_hideAutoGeneratedNets = m_hideAutoGenNetNamesOpt->GetValue();

    // Nets sort criteria
    m_netSortingByPadCount = m_sortByPadsOpt->GetValue();
}


void DIALOG_COPPER_ZONE::updateShowNetsFilter()
{
    wxString netNameShowFilter = m_ShowNetNameFilter->GetValue();

    if( netNameShowFilter.Len() == 0 )
    {
        netNameShowFilter = wxT( "*" );
    }

    wxStringTokenizer showFilters( netNameShowFilter.Lower(), wxT( "," ) );

    m_showNetsFilter.clear();

    while( showFilters.HasMoreTokens() )
    {
        wxString filter = showFilters.GetNextToken();
        filter.Trim( false );
        filter.Trim( true );

        if( !filter.IsEmpty() )
        {
            m_showNetsFilter.emplace_back( std::make_unique<EDA_PATTERN_MATCH_WILDCARD>() );
            m_showNetsFilter.back()->SetPattern( filter );
        }
    }
}


wxArrayString DIALOG_COPPER_ZONE::buildListOfNetsToDisplay()
{
    sortNetsIfRequired();

    const wxArrayString sortedNetNames = getSortedNetNamesList();

    const wxArrayString netsAfterShowFilter = applyShowFilter( sortedNetNames );

    wxArrayString filteredNetNames = applyHideFilterIfRequired( netsAfterShowFilter );

    return filteredNetNames;
}


void DIALOG_COPPER_ZONE::sortNetsIfRequired()
{
    if( m_netSortingByPadCount )
    {
        sortNetsByPadCount( m_netInfoItemList, m_maxNetCode );
    }
    else
    {
        sort( m_netInfoItemList.begin(), m_netInfoItemList.end(), sortNetsByNames );
    }
}


void DIALOG_COPPER_ZONE::sortNetsByPadCount( std::vector<NETINFO_ITEM*>& nets,
                                             const int                   maxNetCode )
{
    const std::vector<PAD*> pads = m_Parent->GetBoard()->GetPads();

    padCountListByNet.clear();

    // +1 is required for <no-net> item
    padCountListByNet.assign( maxNetCode + 1, 0 );

    for( PAD* pad : pads )
    {
        const int netCode = pad->GetNetCode();

        if( netCode > INVALID_NET_CODE )
            padCountListByNet[netCode]++;
    }

    sort( nets.begin(), nets.end(), sortNetsByNodes );
}


wxArrayString DIALOG_COPPER_ZONE::getSortedNetNamesList()
{
    wxArrayString sortedNetNames;

    for( NETINFO_ITEM* net : m_netInfoItemList )
    {
        const wxString& netName = getUnescapedNetName( net );
        sortedNetNames.Add( netName );
    }

    return sortedNetNames;
}


wxArrayString DIALOG_COPPER_ZONE::applyShowFilter( const wxArrayString& netNames )
{
    wxArrayString netsAfterShowFilter;

    for( const wxString& netName : netNames )
    {
        for( const NET_FILTER& filter : m_showNetsFilter )
        {
            if( filter->Find( netName.Lower() ) )
            {
                netsAfterShowFilter.Add( netName );
            }
        }
    }

    return netsAfterShowFilter;
}


wxArrayString DIALOG_COPPER_ZONE::applyHideFilterIfRequired( const wxArrayString& netNames )
{
    wxArrayString filteredNetNames;

    if( m_hideAutoGeneratedNets )
    {
        for( const wxString& netName : netNames )
        {
            if( !isAutoGenerated( netName ) )
            {
                filteredNetNames.Add( netName );
            }
        }
    }
    else
    {
        filteredNetNames = netNames;
    }

    filteredNetNames.Insert( wxT( "<no net>" ), INVALID_NET_CODE );

    return filteredNetNames;
}


bool DIALOG_COPPER_ZONE::isAutoGenerated( const wxString& netName )
{
    return netName.StartsWith( wxT( "Net-(" ) ) || netName.StartsWith( wxT( "unconnected-(" ) );
}


void DIALOG_COPPER_ZONE::displayNetsList( const wxArrayString& netNamesList, int selectIndex )
{
    m_ListNetNameSelection->Clear();
    m_ListNetNameSelection->InsertItems( netNamesList, 0 );
    m_ListNetNameSelection->SetSelection( selectIndex );
    m_ListNetNameSelection->EnsureVisible( selectIndex );
}


int DIALOG_COPPER_ZONE::ensureSelectedNetIsVisible( int selectedNetCode, wxArrayString& netsList )
{
    int selectedIndex = 0;
    if( selectedNetCode > INVALID_NET_CODE )
    {
        NETINFO_ITEM* selectedNet = m_Parent->GetBoard()->FindNet( selectedNetCode );

        if( selectedNet )
        {
            const wxString& netName = getUnescapedNetName( selectedNet );
            selectedIndex = netsList.Index( netName );

            if( wxNOT_FOUND == selectedIndex )
            {
                // the currently selected net must *always* be visible.
		        // <no net> is the zero'th index, so pick next lowest
                netsList.Insert( netName, 1 );
                selectedIndex = 1;
            }
        }
    }

    return selectedIndex;
}


wxString DIALOG_COPPER_ZONE::getUnescapedNetName( const NETINFO_ITEM* net )
{
    return UnescapeString( net->GetNetname() );
}


void DIALOG_COPPER_ZONE::updateInfoBar()
{
    if( m_currentlySelectedNetcode <= INVALID_NET_CODE && !m_copperZoneInfo->IsShown() )
    {
        m_copperZoneInfo->ShowMessage(
                _( "Selecting <no net> will create an isolated copper island." ), wxICON_WARNING );
    }
    else if( m_currentlySelectedNetcode > INVALID_NET_CODE && m_copperZoneInfo->IsShown() )
    {
        m_copperZoneInfo->Dismiss();
    }
}
