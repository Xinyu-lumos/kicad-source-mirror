///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b3)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include "dialog_shim.h"
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_UPDATE_CHECK_PROMPT_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_UPDATE_CHECK_PROMPT_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_messageLine1;
		wxCheckBox* m_cbKiCadUpdates;
		wxCheckBox* m_cbPCMUpdates;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;

	public:

		DIALOG_UPDATE_CHECK_PROMPT_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Check for Updates"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE );

		~DIALOG_UPDATE_CHECK_PROMPT_BASE();

};

