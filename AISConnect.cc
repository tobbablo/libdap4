
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002,2003 OPeNDAP, Inc.
// Author: James Gallagher <jgallagher@opendap.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
 
#include "config_dap.h"

#include "AISConnect.h"

/** Build an AISConnect. This calls Connect's constructor with \c name and
    accepts its default values for the other parameters. The AIS database to
    use is read from the configuration file <c>~/.dodsrc</c>.
    
    @param name Create a virtual, AIS-enhanced, connection for this OPeNDAP
    data source.
    @exception AISDatabaseReadFailed Thrown if the AIS database listed in the
    <c>~/.dodsrc</c> file could not be opened. */
AISConnect::AISConnect(const string &name) throw(AISDatabaseReadFailed)
    : Connect(name), d_ais_merge(0)
{
    const string &ais_db = RCReader::instance()->get_ais_database();
    if (ais_db != "")
	d_ais_merge = new AISMerge(ais_db);
}

/** Destroy an AISConnect. */
AISConnect::~AISConnect()
{
    delete d_ais_merge;
}

/** Request an AIS-enhanced DAS object. Read the DAS object for this virtual
    connection's data source. Then check the AIS database and merge in any
    ancillary resources listed for it.

    @param das Deposit information to this object.
    @exception Error Thrown if the DAS request fails due to user error.
    @exception InternalErr Thrown if either the DAS request or the AIS merge
    request fails. */
void
AISConnect::request_das(DAS &das) throw(Error, InternalErr)
{
    Connect::request_das(das);

    d_ais_merge->merge(URL(false), das); // URL(false) --> URL w/o CE
}

// $Log: AISConnect.cc,v $
// Revision 1.1  2003/02/27 23:17:00  jimg
// Added.
//