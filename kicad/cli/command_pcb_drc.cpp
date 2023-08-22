/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022 Mark Roszko <mark.roszko@gmail.com>
 * Copyright (C) 1992-2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "command_pcb_drc.h"
#include <cli/exit_codes.h>
#include "jobs/job_pcb_drc.h"
#include <kiface_base.h>
#include <layer_ids.h>
#include <wx/crt.h>

#include <macros.h>
#include <wx/tokenzr.h>

#define ARG_FORMAT "--format"
#define ARG_ALL_TRACK_ERRORS "--all-track-errors"
#define ARG_UNITS "--units"
#define ARG_SEVERITY_ALL "--severity-all"
#define ARG_SEVERITY_ERROR "--severity-error"
#define ARG_SEVERITY_WARNING "--severity-warning"
#define ARG_SEVERITY_EXCLUSIONS "--severity-exclusions"
#define ARG_EXIT_CODE_VIOLATIONS "--exit-code-violations"

CLI::PCB_DRC_COMMAND::PCB_DRC_COMMAND() : PCB_EXPORT_BASE_COMMAND( "drc" )
{
    m_argParser.add_description( UTF8STDSTR( _( "Runs the Design Rules Check (DRC) on the PCB and creates a report" ) ) );

    m_argParser.add_argument( ARG_FORMAT )
            .default_value( std::string( "report" ) )
            .help( UTF8STDSTR( _( "Output file format, options: json, report" ) ) );

    m_argParser.add_argument( ARG_ALL_TRACK_ERRORS )
            .help( UTF8STDSTR( _( "Report all errors for each track" ) ) )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_UNITS )
            .default_value( std::string( "mm" ) )
            .help( UTF8STDSTR(
                    _( "Report units; valid options: in, mm, mils" ) ) );

    m_argParser.add_argument( ARG_SEVERITY_ALL )
            .help( UTF8STDSTR( _( "Report all DRC violations, this is equivalent to including all the other severity arguments" ) ) )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_SEVERITY_ERROR )
            .help( UTF8STDSTR( _( "Report all DRC error level violations, this can be combined with the other severity arguments" ) ) )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_SEVERITY_WARNING )
            .help( UTF8STDSTR( _( "Report all DRC warning level violations, this can be combined with the other severity arguments" ) ) )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_SEVERITY_EXCLUSIONS )
            .help( UTF8STDSTR( _( "Report all excluded DRC violations, this can be combined with the other severity arguments" ) ) )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_EXIT_CODE_VIOLATIONS )
            .help( UTF8STDSTR( _( "Return a exit code depending on whether or not violations exist" ) ) )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( "-o", ARG_OUTPUT )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Output file name" ) ) );
}


int CLI::PCB_DRC_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_DRC> drcJob( new JOB_PCB_DRC( true ) );

    drcJob->m_outputFile = FROM_UTF8( m_argParser.get<std::string>( ARG_OUTPUT ).c_str() );
    drcJob->m_filename = FROM_UTF8( m_argParser.get<std::string>( ARG_INPUT ).c_str() );
    drcJob->m_reportAllTrackErrors = m_argParser.get<bool>( ARG_ALL_TRACK_ERRORS );
    drcJob->m_exitCodeViolations = m_argParser.get<bool>( ARG_EXIT_CODE_VIOLATIONS );

    if( m_argParser.get<bool>( ARG_SEVERITY_ALL ) )
    {
        drcJob->m_severity = RPT_SEVERITY_ERROR | RPT_SEVERITY_WARNING | RPT_SEVERITY_EXCLUSION;
    }

    if( m_argParser.get<bool>( ARG_SEVERITY_ERROR ) )
    {
        drcJob->m_severity |= RPT_SEVERITY_ERROR;
    }

    if( m_argParser.get<bool>( ARG_SEVERITY_WARNING ) )
    {
        drcJob->m_severity |= RPT_SEVERITY_WARNING;
    }

    if( m_argParser.get<bool>( ARG_SEVERITY_EXCLUSIONS ) )
    {
        drcJob->m_severity |= RPT_SEVERITY_EXCLUSION;
    }

    drcJob->m_reportAllTrackErrors = m_argParser.get<bool>( ARG_ALL_TRACK_ERRORS );

    wxString units = FROM_UTF8( m_argParser.get<std::string>( ARG_UNITS ).c_str() );

    if( units == wxS( "mm" ) )
    {
        drcJob->m_units = JOB_PCB_DRC::UNITS::MILLIMETERS;
    }
    else if( units == wxS( "in" ) )
    {
        drcJob->m_units = JOB_PCB_DRC::UNITS::INCHES;
    }
    else if( units == wxS( "mils" ) )
    {
        drcJob->m_units = JOB_PCB_DRC::UNITS::MILS;
    }
    else if( !units.IsEmpty() )
    {
        wxFprintf( stderr, _( "Invalid units specified\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    wxString format = FROM_UTF8( m_argParser.get<std::string>( ARG_FORMAT ).c_str() );
    if( format == "report" )
    {
        drcJob->m_format = JOB_PCB_DRC::OUTPUT_FORMAT::REPORT;
    }
    else if( format == "json" )
    {
        drcJob->m_format = JOB_PCB_DRC::OUTPUT_FORMAT::JSON;
    }
    else
    {
        wxFprintf( stderr, _( "Invalid report format\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    int exitCode = aKiway.ProcessJob( KIWAY::FACE_PCB, drcJob.get() );

    return exitCode;
}
