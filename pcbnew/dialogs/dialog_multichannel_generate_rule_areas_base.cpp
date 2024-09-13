///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.0.0-0-g0efcecf)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_multichannel_generate_rule_areas_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE::DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 800,-1 ), wxDefaultSize );

	m_GeneralBoxSizer = new wxBoxSizer( wxVERTICAL );

	m_GeneralBoxSizer->SetMinSize( wxSize( 800,300 ) );
	wxFlexGridSizer* fgSizer3;
	fgSizer3 = new wxFlexGridSizer( 3, 1, 0, 0 );
	fgSizer3->AddGrowableCol( 0 );
	fgSizer3->AddGrowableRow( 0 );
	fgSizer3->SetFlexibleDirection( wxBOTH );
	fgSizer3->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	fgSizer3->SetMinSize( wxSize( 800,300 ) );
	m_sheetsGrid = new wxGrid( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

	// Grid
	m_sheetsGrid->CreateGrid( 1, 3 );
	m_sheetsGrid->EnableEditing( false );
	m_sheetsGrid->EnableGridLines( true );
	m_sheetsGrid->EnableDragGridSize( false );
	m_sheetsGrid->SetMargins( 0, 0 );

	// Columns
	m_sheetsGrid->SetColSize( 0, 100 );
	m_sheetsGrid->SetColSize( 1, 300 );
	m_sheetsGrid->SetColSize( 2, 100 );
	m_sheetsGrid->AutoSizeColumns();
	m_sheetsGrid->EnableDragColMove( true );
	m_sheetsGrid->EnableDragColSize( true );
	m_sheetsGrid->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Rows
	m_sheetsGrid->AutoSizeRows();
	m_sheetsGrid->EnableDragRowSize( true );
	m_sheetsGrid->SetRowLabelSize( wxGRID_AUTOSIZE );
	m_sheetsGrid->SetRowLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Label Appearance

	// Cell Defaults
	m_sheetsGrid->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
	fgSizer3->Add( m_sheetsGrid, 1, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer13;
	bSizer13 = new wxBoxSizer( wxVERTICAL );

	m_cbReplaceExisting = new wxCheckBox( this, wxID_ANY, _("Replace existing placement rule areas"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer13->Add( m_cbReplaceExisting, 0, wxALL, 5 );

	m_cbGroupItems = new wxCheckBox( this, wxID_ANY, _("Group components with their placement rule areas"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer13->Add( m_cbGroupItems, 0, wxALL, 5 );


	fgSizer3->Add( bSizer13, 1, wxEXPAND, 5 );

	wxBoxSizer* bottomButtonsSizer;
	bottomButtonsSizer = new wxBoxSizer( wxHORIZONTAL );


	bottomButtonsSizer->Add( 10, 0, 0, 0, 5 );

	m_sdbSizerStdButtons = new wxStdDialogButtonSizer();
	m_sdbSizerStdButtonsOK = new wxButton( this, wxID_OK );
	m_sdbSizerStdButtons->AddButton( m_sdbSizerStdButtonsOK );
	m_sdbSizerStdButtonsCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizerStdButtons->AddButton( m_sdbSizerStdButtonsCancel );
	m_sdbSizerStdButtons->Realize();

	bottomButtonsSizer->Add( m_sdbSizerStdButtons, 1, wxEXPAND|wxALL, 5 );


	fgSizer3->Add( bottomButtonsSizer, 0, wxEXPAND|wxLEFT, 5 );


	m_GeneralBoxSizer->Add( fgSizer3, 1, wxEXPAND, 5 );


	this->SetSizer( m_GeneralBoxSizer );
	this->Layout();
	m_GeneralBoxSizer->Fit( this );

	// Connect Events
	this->Connect( wxEVT_INIT_DIALOG, wxInitDialogEventHandler( DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE::OnInitDlg ) );
	this->Connect( wxEVT_UPDATE_UI, wxUpdateUIEventHandler( DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE::OnUpdateUI ) );
}

DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE::~DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE()
{
	// Disconnect Events
	this->Disconnect( wxEVT_INIT_DIALOG, wxInitDialogEventHandler( DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE::OnInitDlg ) );
	this->Disconnect( wxEVT_UPDATE_UI, wxUpdateUIEventHandler( DIALOG_MULTICHANNEL_GENERATE_RULE_AREAS_BASE::OnUpdateUI ) );

}
