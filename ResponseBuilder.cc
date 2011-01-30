
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2011 OPeNDAP, Inc.
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

#include "config.h"

static char rcsid[] not_used =
    {"$Id: ResponseBuilder.cc 23477 2010-09-02 21:02:59Z jimg $"
    };

#include <signal.h>

#ifndef WIN32
#include <unistd.h>   // for getopt
#include <sys/wait.h>
#else
#include <io.h>
#include <fcntl.h>
#include <process.h>
#endif

#include <iostream>
#include <string>
#include <set>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <uuid/uuid.h>	// used to build CID header value for data ddx

#include <GetOpt.h>

#include "DAS.h"
#include "DDS.h"
#include "debug.h"
#include "mime_util.h"
#include "Ancillary.h"
#include "util.h"
#include "escaping.h"
#include "ResponseBuilder.h"
#include "XDRStreamMarshaller.h"
#include "InternalErr.h"

#ifndef WIN32
#include "SignalHandler.h"
#include "EventHandler.h"
#include "AlarmHandler.h"
#endif

#define CRLF "\r\n"             // Change here, expr-test.cc and ResponseBuilder.cc

using namespace std;

namespace libdap {

ResponseBuilder::~ResponseBuilder()
{
}

/** Called when initializing a ResponseBuilder that's not going to be passed a
command line arguments. */
void
ResponseBuilder::initialize()
{
    // Set default values. Don't use the C++ constructor initialization so
    // that a subclass can have more control over this process.
    d_dataset = "";
    d_ce = "";
    d_timeout = 0;

    // Load known_keywords
    d_known_keywords.insert("dap2");
    d_known_keywords.insert("dap2.0");

    d_known_keywords.insert("dap3.2");

    d_known_keywords.insert("dap4");
    d_known_keywords.insert("dap4.0");

#ifdef WIN32
    //  We want serving from win32 to behave in a manner
    //  similar to the UNIX way - no CR->NL terminated lines
    //  in files. Hence stdout goes to binary mode.
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}
/**
 * Add the keyword to the set of keywords that apply to this request.
 * @param kw The keyword
 */
void ResponseBuilder::add_keyword(const string &kw)
{
    d_keywords.insert(kw);
}

/**
 * Lookup a keyword and return true if it has been set for this request,
 * otherwise return false.
 * @param kw Keyword
 * @return true if the keyword is set.
 */
bool ResponseBuilder::is_keyword(const string &kw) const
{
    return d_keywords.count(kw) != 0;
    //return d_keywords.find(kw) != d_keywords.end();
}

/**
 * Get a list of the strings that make up the set of current keywords for
 * this request.
 * @return The list of keywords as a list of string objects.
 */
list<string> ResponseBuilder::get_keywords() const
{
    list<string> kws;
    set<string>::const_iterator i;
    for (i = d_keywords.begin(); i != d_keywords.end(); ++i)
	kws.push_front(*i);
    return kws;
}

/**
 * Is the word one of the known keywords for this version of libdap?
 * @param w
 * @return true if the keyword is known
 */
bool
ResponseBuilder::is_known_keyword(const string &w) const
{
    return d_known_keywords.count(w) != 0;
}


/** Return the entire constraint expression in a string.  This
includes both the projection and selection clauses, but not the
question mark.

@brief Get the constraint expression.
@return A string object that contains the constraint expression. */
string
ResponseBuilder::get_ce() const
{
    return d_ce;
}

void
ResponseBuilder::set_ce(string _ce)
{
    // Get the whole CE
    string projection = www2id(_ce, "%", "%20");
    string selection = "";

    // Separate the selection part (which follows/includes the first '&')
    string::size_type amp = projection.find('&');
    if (amp != string::npos) {
	selection = projection.substr(amp);
	projection = projection.substr(0, amp);
    }

    // Extract keywords; add to the ResponseBuilder keywords. For this, scan for
    // a known set of keywords and assume that anything else is part of the
    // projection and should be left alone. Keywords must come before variables
    // The 'projection' string will look like: '' or 'dap4.0' or 'dap4.0,u,v'
    while (!projection.empty()) {
	string::size_type i = projection.find(',');
	string next_word = projection.substr(0, i);
	if (is_known_keyword(next_word)) {
	    add_keyword(next_word);
	    projection = projection.substr(i+1);
	}
	else {
	    break;	// exit on first non-keyword
	}
    }

    // The CE is whatever is left after removing the keywords
    d_ce = projection + selection;
}

/** The ``dataset name'' is the filename or other string that the
filter program will use to access the data. In some cases this
will indicate a disk file containing the data.  In others, it
may represent a database query or some other exotic data
access method.

@brief Get the dataset name.
@return A string object that contains the name of the dataset. */
string
ResponseBuilder::get_dataset_name() const
{
    return d_dataset;
}

void
ResponseBuilder::set_dataset_name(const string ds)
{
    d_dataset = www2id(ds, "%", "%20");
}

/** Set the server's timeout value. A value of zero (the default) means no
    timeout.

    @param t Server timeout in seconds. Default is zero (no timeout). */
void
ResponseBuilder::set_timeout(int t)
{
    d_timeout = t;
}

/** Get the server's timeout value. */
int
ResponseBuilder::get_timeout() const
{
    return d_timeout;
}

/** Use values of this instance to establish a timeout alarm for the server.
    If the timeout value is zero, do nothing.

    @todo When the alarm handler is called, two CRLF pairs are dumped to the
    stream and then an Error object is sent. No attempt is made to write the
    'correct' MIME headers for an Error object. Instead, a savvy client will
    know that when an exception is thrown during a deserialize operation, it
    should scan ahead in the input stream for an Error object. Add this, or a
    sensible variant once libdap++ supports reliable error delivery. Dumb
    clients will never get the Error object... */
void
ResponseBuilder::establish_timeout(ostream &stream) const
{
#ifndef WIN32
    if (d_timeout > 0) {
        SignalHandler *sh = SignalHandler::instance();
        EventHandler *old_eh = sh->register_handler(SIGALRM, new AlarmHandler(stream));
        delete old_eh;
        alarm(d_timeout);
    }
#endif
}

/** This function formats and prints an ASCII representation of a
    DAS on stdout.  This has the effect of sending the DAS object
    back to the client program.

    @brief Transmit a DAS.
    @param out The output stream to which the DAS is to be sent.
    @param das The DAS object to be sent.
    @param anc_location The directory in which the external DAS file resides.
    @param with_mime_headers If true (the default) send MIME headers.
    @return void
    @see DAS */
void ResponseBuilder::send_das(ostream &out, DAS &das, const string &,
		bool with_mime_headers) const {
	if (with_mime_headers)
		set_mime_text(out, dods_das, "", x_plain, last_modified_time(d_dataset));
	das.print(out);

	out << flush;
}

/** This function formats and prints an ASCII representation of a
    DDS on stdout.  When called by a CGI program, this has the
    effect of sending a DDS object back to the client
    program. Either an entire DDS or a constrained DDS may be sent.

    @brief Transmit a DDS.
    @param out The output stream to which the DAS is to be sent.
    @param dds The DDS to send back to a client.
    @param eval A reference to the ConstraintEvaluator to use.
    @param constrained If this argument is true, evaluate the
    current constraint expression and send the `constrained DDS'
    back to the client.
    @param anc_location The directory in which the external DAS file resides.
    @param with_mime_headers If true (the default) send MIME headers.
    @return void
    @see DDS */
void ResponseBuilder::send_dds(ostream &out, DDS &dds,
		ConstraintEvaluator &eval, bool constrained, const string &,
		bool with_mime_headers) const {
	// If constrained, parse the constraint. Throws Error or InternalErr.
	if (constrained)
		eval.parse_constraint(d_ce, dds);

	if (eval.functional_expression())
		throw Error(
				"Function calls can only be used with data requests. To see the structure of the underlying data source, reissue the URL without the function.");

	if (with_mime_headers)
		set_mime_text(out, dods_dds, "", x_plain, last_modified_time(d_dataset));
	if (constrained)
		dds.print_constrained(out);
	else
		dds.print(out);

	out << flush;
}

void ResponseBuilder::dataset_constraint(DDS & dds, ConstraintEvaluator & eval,
		ostream &out, bool ce_eval) const {
	// send constrained DDS
	dds.print_constrained(out);
	out << "Data:\n";
	out << flush;

	// Grab a stream that encodes using XDR.
	XDRStreamMarshaller m(out);

	try {
		// Send all variables in the current projection (send_p())
		for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++)
			if ((*i)->send_p()) {
				DBG(cerr << "Sending " << (*i)->name() << endl);
				(*i)->serialize(eval, dds, m, ce_eval);
			}
	} catch (Error & e) {
		throw;
	}
}

void
ResponseBuilder::dataset_constraint_ddx(DDS & dds, ConstraintEvaluator & eval,
                               ostream &out, const string &boundary,
                               const string &start, bool ce_eval) const
{
    // Write the MPM headers for the DDX (text/xml) part of the response
    set_mime_ddx_boundary(out, boundary, start, dap4_ddx);

    // Make cid
    uuid_t uu;
    uuid_generate(uu);
    char uuid[37];
    uuid_unparse(uu, &uuid[0]);
    char domain[256];
    if (getdomainname(domain, 255) != 0 || strlen(domain) == 0)
	strncpy(domain, "opendap.org", 255);

    string cid = string(&uuid[0]) + "@" + string(&domain[0]);

    // Send constrained DDX with a data blob reference
    dds.print_xml(out, true, cid);

    // Write the MPM headers for the data part of the response.
    set_mime_data_boundary(out, boundary, cid, dap4_data, binary);

    // Grab a stream that encodes using XDR.
    XDRStreamMarshaller m( out ) ;

    try {
        // Send all variables in the current projection (send_p())
        for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++)
            if ((*i)->send_p()) {
                DBG(cerr << "Sending " << (*i)->name() << endl);
                (*i)->serialize(eval, dds, m, ce_eval);
            }
    }
    catch (Error & e) {
        throw;
    }
}

/** Send the data in the DDS object back to the client program. The data is
    encoded using a Marshaller, and enclosed in a MIME document which is all sent
    to \c data_stream. If this is being called from a CGI, \c data_stream is
    probably \c stdout and writing to it has the effect of sending the
    response back to the client.

    @brief Transmit data.
    @param dds A DDS object containing the data to be sent.
    @param eval A reference to the ConstraintEvaluator to use.
    @param data_stream Write the response to this stream.
    @param anc_location A directory to search for ancillary files (in
    addition to the CWD).  This is used in a call to
    get_data_last_modified_time().
    @param with_mime_headers If true, include the MIME headers in the response.
    Defaults to true.
    @return void */
void
ResponseBuilder::send_data(DDS & dds, ConstraintEvaluator & eval,
                      ostream & data_stream, const string &,
                      bool with_mime_headers) const
{
	// Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    eval.parse_constraint(d_ce, dds);   // Throws Error if the ce doesn't
					// parse.

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
    	DDS *fdds = eval.eval_function_clauses(dds);
        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, "", x_plain, last_modified_time(d_dataset));

        dataset_constraint(*fdds, eval, data_stream, false);
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, "", x_plain, last_modified_time(d_dataset));

        dataset_constraint(dds, eval, data_stream);
    }

    data_stream << flush ;
}

/** Send the DDX response. The DDX never contains data, instead it holds a
    reference to a Blob response which is used to get the data values. The
    DDS and DAS objects are built using code that already exists in the
    servers.

    @param dds The dataset's DDS \e with attributes in the variables.
    @param eval A reference to the ConstraintEvaluator to use.
    @param out Destination
    @param with_mime_headers If true, include the MIME headers in the response.
    Defaults to true. */
void
ResponseBuilder::send_ddx(DDS &dds, ConstraintEvaluator &eval, ostream &out,
                     bool with_mime_headers) const
{
    // If constrained, parse the constraint. Throws Error or InternalErr.
    if (!d_ce.empty())
        eval.parse_constraint(d_ce, dds);

    if (eval.functional_expression())
        throw Error("Function calls can only be used with data requests. To see the structure of the underlying data source, reissue the URL without the function.");

    if (with_mime_headers)
            set_mime_text(out, dap4_ddx, "", x_plain, last_modified_time(d_dataset));
        dds.print_xml(out, !d_ce.empty(), "");
}

/** Send the data in the DDS object back to the client program. The data is
    encoded using a Marshaller, and enclosed in a MIME document which is all sent
    to \c data_stream. If this is being called from a CGI, \c data_stream is
    probably \c stdout and writing to it has the effect of sending the
    response back to the client.

    @brief Transmit data.
    @param dds A DDS object containing the data to be sent.
    @param eval A reference to the ConstraintEvaluator to use.
    @param data_stream Write the response to this stream.
    @param anc_location A directory to search for ancillary files (in
    addition to the CWD).  This is used in a call to
    get_data_last_modified_time().
    @param with_mime_headers If true, include the MIME headers in the response.
    Defaults to true.
    @return void */
void
ResponseBuilder::send_data_ddx(DDS & dds, ConstraintEvaluator & eval,
                      ostream & data_stream, const string &start,
                      const string &boundary, const string &,
                      bool with_mime_headers) const
{
    // Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    eval.parse_constraint(d_ce, dds);   // Throws Error if the ce doesn't
					// parse.

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
    	DDS *fdds = eval.eval_function_clauses(dds);
        if (with_mime_headers)
            set_mime_multipart(data_stream, boundary, start, dap4_data_ddx,
        	    "", x_plain, last_modified_time(d_dataset));
        data_stream << flush ;
        dataset_constraint(*fdds, eval, data_stream, false);
    	delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_multipart(data_stream, boundary, start, dap4_data_ddx,
        	    "", x_plain, last_modified_time(d_dataset));
        data_stream << flush ;
        dataset_constraint_ddx(dds, eval, data_stream, boundary, start);
    }

    data_stream << flush ;

    if (with_mime_headers)
	data_stream << CRLF << "--" << boundary << "--" << CRLF;
}

} // namespace libdap

