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

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <uuid/uuid.h>  // used to build CID header value for data ddx

#ifndef WIN32
#include <sys/wait.h>
#else
#include <io.h>
#include <fcntl.h>
#include <process.h>
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <cstring>
#include <ctime>

//#define DODS_DEBUG


#if 0
//FIXME
#include "BaseType.h"
#include "Array.h"
#include "Grid.h"
#endif
#include "DAS.h"
#include "DDS.h"
//#include "Connect.h"
//#include "Response.h"
#include "DDXParserSAX2.h"
#include "Ancillary.h"
#include "ResponseBuilder.h"
#include "XDRStreamMarshaller.h"
#include "XDRFileUnMarshaller.h"

#include "DAPCache3.h"

#include "debug.h"
#include "mime_util.h"	// for last_modified_time() and rfc_822_date()
#include "escaping.h"
#include "util.h"

#ifndef WIN32
#include "SignalHandler.h"
#include "EventHandler.h"
#include "AlarmHandler.h"
#endif

#define CRLF "\r\n"             // Change here, expr-test.cc
using namespace std;

namespace libdap {

ResponseBuilder::~ResponseBuilder() {
}

/** Called when initializing a ResponseBuilder that's not going to be passed a
 command line arguments. */
void ResponseBuilder::initialize() {
    // Set default values. Don't use the C++ constructor initialization so
    // that a subclass can have more control over this process.
    d_dataset = "";
    d_ce = "";
    d_btp_func_ce = "";
    d_timeout = 0;

    d_default_protocol = DAP_PROTOCOL_VERSION;

    // Cache size is given in megabytes and later converted to bytes
    // for internal use.
    d_cache = DAPCache3::get_instance("/tmp/dap_functions_cache/", "f", (1ULL << 22));

#ifdef WIN32
    //  We want serving from win32 to behave in a manner
    //  similar to the UNIX way - no CR->NL terminated lines
    //  in files. Hence stdout goes to binary mode.
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

/** Return the entire constraint expression in a string.  This
 includes both the projection and selection clauses, but not the
 question mark.

 @brief Get the constraint expression.
 @return A string object that contains the constraint expression. */
string ResponseBuilder::get_ce() const {
    return d_ce;
}

void ResponseBuilder::set_ce(string _ce) {
    d_ce = www2id(_ce, "%", "%20");
}

/** The ``dataset name'' is the filename or other string that the
 filter program will use to access the data. In some cases this
 will indicate a disk file containing the data.  In others, it
 may represent a database query or some other exotic data
 access method.

 @brief Get the dataset name.
 @return A string object that contains the name of the dataset. */
string ResponseBuilder::get_dataset_name() const {
    return d_dataset;
}

void ResponseBuilder::set_dataset_name(const string ds) {
    d_dataset = www2id(ds, "%", "%20");
}

/** Set the server's timeout value. A value of zero (the default) means no
 timeout.

 @param t Server timeout in seconds. Default is zero (no timeout). */
void ResponseBuilder::set_timeout(int t) {
    d_timeout = t;
}

/** Get the server's timeout value. */
int ResponseBuilder::get_timeout() const {
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
void ResponseBuilder::establish_timeout(ostream &stream) const {
#ifndef WIN32
    if (d_timeout > 0) {
        SignalHandler *sh = SignalHandler::instance();
        EventHandler *old_eh = sh->register_handler(SIGALRM, new AlarmHandler(stream));
        delete old_eh;
        alarm(d_timeout);
    }
#endif
}

/**
 * Use the dataset name and the function-part of the CE to build a name
 * that can be used to index the result of that CE on the dataset. This
 * name can be used both to store a result for later (re)use or to access
 * a previously-stored result.
 *
 */
static string
build_cache_file_name(const string &dataset, const string &ce)
{
    DBG(cerr << "build_cache_file_name: dataset: " << dataset << ", ce: " << ce << endl);

    string name = dataset + "#" + ce;
    string::size_type pos = name.find_first_of("/(),\"\'");
    while (pos != string::npos) {
        name.replace(pos, 1, "#", 1);
        pos = name.find_first_of("/()\"\'");
    }

    DBG(cerr << "build_cache_file_name: name: " << name << endl);

    return name;
}

#if 0
static bool cached_data_ddx_exists(const string &cache_file_name)
{
    ifstream icache_file(cache_file_name.c_str()); // closes on return

    return !icache_file.fail() && !icache_file.bad() && !icache_file.eof();
}
#endif
/**
 * Is the item named by cache_entry_name valid? This code tests that the
 * cache entry is non-zero in size (returns false if that is the case, although
 * that might not be correct) and that the dataset associated with this
 * ResponseBulder instance is at least as old as the cached entry.
 *
 * @param cache_file_name File name of the cached entry
 * @return True if the thing is valid, false otherwise.
 */
bool ResponseBuilder::is_valid(const string &cache_file_name)
{
    // If the cached response is zero bytes in size, it's not valid.
    // (hmmm...)

    off_t entry_size = 0;
    time_t entry_time = 0;
    struct stat buf;
    if (stat(cache_file_name.c_str(), &buf) == 0) {
        entry_size = buf.st_size;
        entry_time = buf.st_mtime;
    }
    else {
        return false;
    }

    if (entry_size == 0)
        return false;

    time_t dataset_time = entry_time;
    if (stat(d_dataset.c_str(), &buf) == 0) {
        dataset_time = buf.st_mtime;
    }

    // Trick: if the d_dataset is not a file, stat() returns error and
    // the times stay equal and the code uses the cache entry.

    // TODO Fix this so that the code can get a LMT from the correct
    // handler.
    if (dataset_time > entry_time)
        return false;

    return true;
}

/** This function formats and prints an ASCII representation of a
 DAS on stdout.  This has the effect of sending the DAS object
 back to the client program.

 @brief Transmit a DAS.
 @param out The output stream to which the DAS is to be sent.
 @param das The DAS object to be sent.
 @param with_mime_headers If true (the default) send MIME headers.
 @return void
 @see DAS */
void ResponseBuilder::send_das(ostream &out, DAS &das, bool with_mime_headers)
{
    if (with_mime_headers)
        set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), "2.0");

    das.print(out);

    out << flush;
}

/** This function formats and prints an ASCII representation of a
 DAS on stdout.  This has the effect of sending the DAS object
 back to the client program.

 @todo Test me!
 @brief Transmit a DAS.
 @param out The output stream to which the DAS is to be sent.
 @param das The DAS object to be sent.
 @param with_mime_headers If true (the default) send MIME headers.
 @return void
 @see DAS */
void ResponseBuilder::send_das(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool constrained,
                               bool with_mime_headers)
{
    // Set up the alarm.
    establish_timeout(out);
    dds.set_timeout(d_timeout);

    if (!constrained) {
        if (with_mime_headers)
            set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), "2.0");

        dds.print_das(out);
        out << flush;

        return;
    }

    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        DBG(cerr << "Found function(s) in CE: " << d_btp_func_ce << endl);

        // These are used for the cached or newly created DDS object
        BaseTypeFactory factory;
        DDS *fdds;

        // Get the cache filename for this thing. Do not use the default
        // name mangling; instead use what build_cache_file_name() does.
        string cache_file_name = d_cache->get_cache_file_name(build_cache_file_name(d_dataset, d_btp_func_ce), false);
        int fd;
        try {
            // If the object in the cache is not valid, remove it. The read_lock will
            // then fail and the code will drop down to the create_and_lock() call.
            // is_valid() tests for a non-zero object and for d_dateset newer than
            // the cached object.
            if (!is_valid(cache_file_name))
                d_cache->purge_file(cache_file_name);

            if (d_cache->get_read_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - cached hit: " << cache_file_name << endl );
                fdds = get_cached_data_ddx(cache_file_name, &factory);
            }

            // If here, the cache_file_name could not be locked for read access;
            // try to build it. First make an empty file and get an exclusive lock on it.
            // TODO Make this an 'else if'?
            if (d_cache->create_and_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - caching " << cache_file_name << endl );

                eval.parse_constraint(d_btp_func_ce, dds);
                fdds = eval.eval_function_clauses(dds);

                // TODO cache it using fd. Since this is advisory locking, this will work...
                // Improve?
                cache_data_ddx(cache_file_name, *fdds);

                // Change the exclusive lock on the new file to a shared lock. This keeps
                // other processes from purging the new file and ensures that the reading
                // process can use it.
                d_cache->exclusive_to_shared_lock(fd);

                // Now update the total cache size info and purge if needed. The new file's
                // name is passed into the purge method because this process cannot detect its
                // own lock on the file.
                unsigned long long size = d_cache->update_cache_info(cache_file_name);
                if (d_cache->cache_too_big(size))
                    d_cache->update_and_purge(cache_file_name);
            }
            else if (d_cache->get_read_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - cached hit: " << cache_file_name << endl );
                fdds = get_cached_data_ddx(cache_file_name, &factory);
            }
            else {
                throw InternalErr(__FILE__, __LINE__, "Cache error during function invocation.");
            }
        }
        catch (...) {
            DBG(cerr << "caught exception, unlocking cache and re-throw." << endl );
            // I think this call is not needed. jhrg 10/23/12
            d_cache->unlock_cache();
            throw;
        }

        if (with_mime_headers)
            set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        fdds->print_das(out);

        delete fdds;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        if (with_mime_headers)
            set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print_das(out);
    }

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
void ResponseBuilder::send_dds(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool constrained,
                               bool with_mime_headers)
{
    if (!constrained) {
        if (with_mime_headers)
            set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print(out);
        out << flush;
        return;
    }

    // Set up the alarm.
    establish_timeout(out);
    dds.set_timeout(d_timeout);

    // Split constraint into two halves
    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        DBG(cerr << "Found function(s) in CE: " << d_btp_func_ce << endl);

        // These are used for the cached or newly created DDS object
        BaseTypeFactory factory;
        DDS *fdds;

        // Get the cache filename for this thing. Do not use the default
        // name mangling; instead use what build_cache_file_name() does.
        string cache_file_name = d_cache->get_cache_file_name(build_cache_file_name(d_dataset, d_btp_func_ce), false);
        int fd;
        try {
            // If the object in the cache is not valid, remove it. The read_lock will
            // then fail and the code will drop down to the create_and_lock() call.
            // is_valid() tests for a non-zero object and for d_dateset newer than
            // the cached object.
            if (!is_valid(cache_file_name))
                d_cache->purge_file(cache_file_name);

            if (d_cache->get_read_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - cached hit: " << cache_file_name << endl );
                fdds = get_cached_data_ddx(cache_file_name, &factory);
            }

            // If here, the cache_file_name could not be locked for read access;
            // try to build it. First make an empty file and get an exclusive lock on it.
            if (d_cache->create_and_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - caching " << cache_file_name << endl );

                eval.parse_constraint(d_btp_func_ce, dds);
                fdds = eval.eval_function_clauses(dds);

                // TODO cache it using fd. Since this is advisory locking, this will work...
                // Improve?
                cache_data_ddx(cache_file_name, *fdds);

                // Change the exclusive lock on the new file to a shared lock. This keeps
                // other processes from purging the new file and ensures that the reading
                // process can use it.
                d_cache->exclusive_to_shared_lock(fd);

                // Now update the total cache size info and purge if needed. The new file's
                // name is passed into the purge method because this process cannot detect its
                // own lock on the file.
                unsigned long long size = d_cache->update_cache_info(cache_file_name);
                if (d_cache->cache_too_big(size))
                    d_cache->update_and_purge(cache_file_name);
            }
            else if (d_cache->get_read_lock(cache_file_name, fd)) {
                    DBG(cerr << "function ce - cached hit: " << cache_file_name << endl );
                    fdds = get_cached_data_ddx(cache_file_name, &factory);
            }
            else {
                throw InternalErr(__FILE__, __LINE__, "Cache error during function invocation.");
            }
        }
        catch (...) {
            DBG(cerr << "caught exception, unlocking cache and re-throw." << endl );
            // I think this call is not needed. jhrg 10/23/12
            d_cache->unlock_cache();
            throw;
        }

        // Server functions might mark variables to use their read()
        // methods. Clear that so the CE in d_ce will control what is
        // sent. If that is empty (there was only a function call) all
        // of the variables in the intermediate DDS (i.e., the function
        // result) will be sent.
        fdds->mark_all(false);

        eval.parse_constraint(d_ce, *fdds);

        if (with_mime_headers)
            set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        fdds->print_constrained(out);

        delete fdds;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        if (with_mime_headers)
            set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print_constrained(out);
    }

    out << flush;
}

void ResponseBuilder::dataset_constraint(ostream &out, DDS & dds, ConstraintEvaluator & eval, bool ce_eval) const {
    // send constrained DDS
    dds.print_constrained(out);
    out << "Data:\n";
    out << flush;
#ifdef CHECKSUMS
    // Grab a stream that encodes using XDR.
    XDRStreamMarshaller m(out, true, true);
#else
    XDRStreamMarshaller m(out, false, true);
#endif
    try {
        // Send all variables in the current projection (send_p())
        for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++)
            if ((*i)->send_p()) {
                DBG(cerr << "Sending " << (*i)->name() << endl);
#ifdef CHECKSUMS
                if ((*i)->type() != dods_structure_c && (*i)->type() != dods_grid_c)
                    m.reset_checksum();

                (*i)->serialize(eval, dds, m, ce_eval);

                if ((*i)->type() != dods_structure_c && (*i)->type() != dods_grid_c)
                    cerr << (*i)->name() << ": " << m.get_checksum() << endl;
#else
                (*i)->serialize(eval, dds, m, ce_eval);
#endif
            }
    }
    catch (Error & e) {
        throw;
    }
}

void ResponseBuilder::dataset_constraint_ddx(ostream &out, DDS & dds, ConstraintEvaluator & eval, const string &boundary, const string &start, bool ce_eval) const
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
    XDRStreamMarshaller m(out);

    try {
        // Send all variables in the current projection (send_p())
        for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++) {
            if ((*i)->send_p()) {
                DBG(cerr << "Sending " << (*i)->name() << endl);
                (*i)->serialize(eval, dds, m, ce_eval);
            }
        }
    }
    catch (Error & e) {
        throw;
    }
}

/**
 *  Split the CE so that the server functions that compute new values are
 *  separated into their own string and can be evaluated separately from
 *  the rest of the CE (which can contain simple and slicing projection
 *  as well as other types of function calls).
 *
 */
void
ResponseBuilder::split_ce(ConstraintEvaluator &eval, const string &expr)
{
    string ce;
    if (!expr.empty())
        ce = expr;
    else
        ce = d_ce;

    string btp_function_ce = "";
    string::size_type pos = 0;
    DBG(cerr << "ce: " << ce << endl);

    string::size_type first_paren = ce.find("(", pos);
    string::size_type closing_paren = ce.find(")", pos);
    while (first_paren != string::npos && closing_paren != string::npos) {
        // Maybe a BTP function; get the name of the potential function
        string name = ce.substr(pos, first_paren-pos);
        DBG(cerr << "name: " << name << endl);
        // is this a BTP function
        btp_func f;
        if (eval.find_function(name, &f)) {
            // Found a BTP function
            if (!btp_function_ce.empty())
                btp_function_ce += ",";
            btp_function_ce += ce.substr(pos, closing_paren+1-pos);
            ce.erase(pos, closing_paren+1-pos);
            if (ce[pos] == ',')
                ce.erase(pos, 1);
        }
        else {
            pos = closing_paren + 1;
            // exception?
            if (pos < ce.length() && ce.at(pos) == ',')
                ++pos;
        }

        first_paren = ce.find("(", pos);
        closing_paren = ce.find(")", pos);
    }

    DBG(cerr << "Modified constraint: " << ce << endl);
    DBG(cerr << "BTP Function part: " << btp_function_ce << endl);

    d_ce = ce;
    d_btp_func_ce = btp_function_ce;
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
void ResponseBuilder::send_data(ostream & data_stream, DDS & dds, ConstraintEvaluator & eval, bool with_mime_headers)
{
    // Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

#if 0
    eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
        string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                + "KB is too large; requests for this user are limited to "
                + long_to_string(dds.get_response_limit() / 1024) + "KB.";
        throw Error(msg);
    }
#endif

    // Split constraint into two halves
    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        DBG(cerr << "Found function(s) in CE: " << d_btp_func_ce << endl);

        // These are used for the cached or newly created DDS object
        BaseTypeFactory factory;
        DDS *fdds;

        // Get the cache filename for this thing. Do not use the default
        // name mangling; instead use what build_cache_file_name() does.
        string cache_file_name = d_cache->get_cache_file_name(build_cache_file_name(d_dataset, d_btp_func_ce), false);
        int fd;
        try {
            // If the object in the cache is not valid, remove it. The read_lock will
            // then fail and the code will drop down to the create_and_lock() call.
            // is_valid() tests for a non-zero object and for d_dateset newer than
            // the cached object.
            if (!is_valid(cache_file_name))
                d_cache->purge_file(cache_file_name);

            if (d_cache->get_read_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - cached hit: " << cache_file_name << endl );
                fdds = get_cached_data_ddx(cache_file_name, &factory);
            }

            // If here, the cache_file_name could not be locked for read access;
            // try to build it. First make an empty file and get an exclusive lock on it.
            if (d_cache->create_and_lock(cache_file_name, fd)) {
                DBG(cerr << "function ce - caching " << cache_file_name << endl );

                eval.parse_constraint(d_btp_func_ce, dds);
                fdds = eval.eval_function_clauses(dds);

                // TODO cache it using fd. Since this is advisory locking, this will work...
                // Improve?
                // Until Connect/Response support working with file descriptors, it's
                // better to use the names.
                cache_data_ddx(cache_file_name, *fdds);

                // Change the exclusive lock on the new file to a shared lock. This keeps
                // other processes from purging the new file and ensures that the reading
                // process can use it.
                d_cache->exclusive_to_shared_lock(fd);

                // Now update the total cache size info and purge if needed. The new file's
                // name is passed into the purge method because this process cannot detect its
                // own lock on the file.
                unsigned long long size = d_cache->update_cache_info(cache_file_name);
                if (d_cache->cache_too_big(size))
                    d_cache->update_and_purge(cache_file_name);
            }
            else if (d_cache->get_read_lock(cache_file_name, fd)) {
                    DBG(cerr << "function ce - cached hit: " << cache_file_name << endl );
                    fdds = get_cached_data_ddx(cache_file_name, &factory);
            }
            else {
                throw InternalErr(__FILE__, __LINE__, "Cache error during function invocation.");
            }
        }
        catch (...) {
            DBG(cerr << "caught exception, unlocking cache and re-throw." << endl );
            // I think this call is not needed. jhrg 10/23/12
            d_cache->unlock_cache();
            throw;
        }
#if 0
        // ******** original code here ***********

        // Check to see if the cached data ddx exists and is valid
        if (cached_data_ddx_exists(cache_file_name)) {
            fdds = get_cached_data_ddx(cache_file_name, &factory);
#if 0
            // Use the cache file and don't eval the function(s)
            DBG(cerr << "Reading cache for " << d_dataset + "?" + d_btp_func_ce << endl);
            icache_file.close(); // only opened to see if it's there; Connect/Response do their own thing

            fdds = new DDS(&factory);
            fdds->set_dap_version("4.0"); // TODO note about cid, ...
            // FIXME name should be...
            fdds->filename( d_dataset ) ;
            fdds->set_dataset_name( name_path( d_dataset ) ) ;

            Connect *url = new Connect( d_dataset ) ;
            Response *r = new Response( fopen( cache_file_name.c_str(), "r" ), 0 ) ;
            if( !r->get_stream() )
                throw Error("The input source: " + cache_file_name +  " could not be opened");

            url->read_data( *fdds, r ) ;
            fdds->set_factory( 0 ) ;

            // mark everything as read.
            DDS::Vars_iter i = fdds->var_begin() ;
            DDS::Vars_iter e = fdds->var_end() ;
            for( ; i != e; i++ ) {
                BaseType *b = (*i) ;
                b->set_read_p( true ) ;
            }
            // for_each(dds->var_begin(), dds->var_end(), mfunc(BaseType::set_read_p));

            DAS *das = new DAS ;
            Ancillary::read_ancillary_das( *das, d_dataset ) ;
            fdds->transfer_attributes( das ) ;
#endif
        }
        else {
            eval.parse_constraint(d_btp_func_ce, dds);
            fdds = eval.eval_function_clauses(dds);

            cache_data_ddx(cache_file_name, *fdds);
#if 0
            // TODO cache the fdds here
            ofstream ocache_file(cache_file_name.c_str());

            DBG(cerr << "Caching " << d_dataset + "?" + d_btp_func_ce << endl);
            cache_data_ddx(ocache_file, *fdds);
            ocache_file.close();
#endif
        }
#endif
        DBG(cerr << "Intermediate DDS: " << endl);
        DBG(fdds->print(cerr));
        DBG(cerr << "Parsing remaining CE: " << d_ce << endl);

        // Server functions might mark variables to use their read()
        // methods. Clear that so the CE in d_ce will control what is
        // sent. If that is empty (there was only a function call) all
        // of the variables in the intermediate DDS (i.e., the function
        // result) will be sent.
        fdds->mark_all(false);

        eval.parse_constraint(d_ce, *fdds);

        fdds->tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

        if (fdds->get_response_limit() != 0 && fdds->get_request_size(true) > fdds->get_response_limit()) {
            string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                    + "KB is too large; requests for this user are limited to "
                    + long_to_string(dds.get_response_limit() / 1024) + "KB.";
            throw Error(msg);
        }

        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dataset_constraint(data_stream, *fdds, eval, false);

        d_cache->unlock_and_close(cache_file_name);

        delete fdds;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

        if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
            string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                    + "KB is too large; requests for this user are limited to "
                    + long_to_string(dds.get_response_limit() / 1024) + "KB.";
            throw Error(msg);
        }

        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dataset_constraint(data_stream, dds, eval);
    }

#if 0
    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
        DDS *fdds = eval.eval_function_clauses(dds);
        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dataset_constraint(data_stream, *fdds, eval, false);
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dataset_constraint(data_stream, dds, eval);
    }
#endif

    data_stream << flush;
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
void ResponseBuilder::send_ddx(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool with_mime_headers)
{
    // If constrained, parse the constraint. Throws Error or InternalErr.
    if (!d_ce.empty())
        eval.parse_constraint(d_ce, dds);

    if (eval.functional_expression())
        throw Error("Function calls can only be used with data requests. To see the structure of the underlying data source, reissue the URL without the function.");

    if (with_mime_headers)
        set_mime_text(out, dap4_ddx, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

    dds.print_xml_writer(out, !d_ce.empty(), "");
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
void ResponseBuilder::send_data_ddx(ostream & data_stream, DDS & dds, ConstraintEvaluator & eval, const string &start, const string &boundary, bool with_mime_headers)
{

    // Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

    if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
        string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024) + "KB is too large; requests for this user are limited to " + long_to_string(
                dds.get_response_limit() / 1024) + "KB.";
        throw Error(msg);
    }

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
        DDS *fdds = eval.eval_function_clauses(dds);
        if (with_mime_headers)
            set_mime_multipart(data_stream, boundary, start, dap4_data_ddx, x_plain, last_modified_time(d_dataset));
        data_stream << flush;
        // TODO: Change this to dataset_constraint_ddx()
        dataset_constraint(data_stream, *fdds, eval, false);
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_multipart(data_stream, boundary, start, dap4_data_ddx, x_plain, last_modified_time(d_dataset));
        data_stream << flush;
        dataset_constraint_ddx(data_stream, dds, eval, boundary, start);
    }

    data_stream << flush;

    if (with_mime_headers)
        data_stream << CRLF << "--" << boundary << "--" << CRLF;
}

/** Write a DDS to an output stream. This method is intended to be used
    to write to a cache so that interim results can be reused w/o needing
    to be recomputed. I chose the 'data ddx' response because it combines
    the syntax and semantic metadata along with the data and all
    three DAP2 requests can be satisfied using it.

    @brief Cache data.

    @param cache_file_name Put the data here
    @param dds A DDS object containing the data to be sent.
    @return void */

void ResponseBuilder::cache_data_ddx(const string &cache_file_name, DDS &dds)
{
    DBG(cerr << "Caching " << d_dataset + "?" + d_btp_func_ce << endl);

    ofstream data_stream(cache_file_name.c_str());
    // Test for a valid file open

    string start="dataddx_cache_start", boundary="dataddx_cache_boundary";
#if 1
    // Does this really need the full set of MIME headers? Not including these
    // might make it comparible with the dapreader module in the BES.
    set_mime_multipart(data_stream, boundary, start, dap4_data_ddx, x_plain, last_modified_time(d_dataset));
    data_stream << flush;
#endif

    // dataset_constraint_ddx() needs a ConstraintEvaluator because
    // it calls serialize().
    ConstraintEvaluator eval;

    // Setting the DDS version to 3.2 causes the print_xml() code
    // to write out a 'blob' element with a valid cid. The reader
    // code in Connect needs this (or thinks it does...)
    dds.set_dap_version("3.2");

    dataset_constraint_ddx(data_stream, dds, eval, boundary, start);
    data_stream << flush;

    data_stream << CRLF << "--" << boundary << "--" << CRLF;
    data_stream.close();
}

/**
 * Read the data from the saved response document.
 *
 * @note this method is made of code copied from Connect (process_data(0)
 * but this copy assumes ot is reading a DDX with data written using the
 * code in ResponseBuilder::cache_data_ddx().
 *
 * @note I put this code here instead of using what was in Connect because
 * I did not want all of the handlers to be modified to inlcude libdapclient
 * and thus libcurl and libuuid.
 *
 * @todo Maybe move this code into libdap as a general 'get it from
 * disk' method. Use that code in libdapclient.
 *
 * @param data The input stream
 * @parma fdds Load this DDS object with the variables, attributes and
 * data values from the cached DDS.
 */
void ResponseBuilder::read_data_from_cache(FILE *data, DDS *fdds)
{
    // Rip off the MIME headers from the response if they are present
    string mime = get_next_mime_header(data);
    while (!mime.empty()) {
#if 0
        string header, value;
        parse_mime_header(mime, header, value);
#endif
        mime = get_next_mime_header(data);
    }

    // Parse the DDX; throw an exception on error.
    DDXParser ddx_parser(fdds->get_factory());

    // Read the MPM boundary and then read the subsequent headers
    string boundary = read_multipart_boundary(data);
    DBG(cerr << "MPM Boundary: " << boundary << endl);

    read_multipart_headers(data, "text/xml", dap4_ddx);

    // Parse the DDX, reading up to and including the next boundary.
    // Return the CID for the matching data part
    string data_cid;
    ddx_parser.intern_stream(data, fdds, data_cid, boundary);

    // Munge the CID into something we can work with
    data_cid = cid_to_header_value(data_cid);
    DBG(cerr << "Data CID: " << data_cid << endl);

    // Read the data part's MPM part headers (boundary was read by
    // DDXParse::intern)
    read_multipart_headers(data, "application/octet-stream", dap4_data, data_cid);

    // Now read the data

    XDRFileUnMarshaller um(data);
    for (DDS::Vars_iter i = fdds->var_begin(); i != fdds->var_end(); i++) {
        (*i)->deserialize(um, fdds);
    }
}

/**
 * Read data from cache. Allocates a new DDS using the given factory.
 */
DDS *
ResponseBuilder::get_cached_data_ddx(const string &cache_file_name, BaseTypeFactory *factory)
{
    DBG(cerr << "Reading cache for " << d_dataset + "?" + d_btp_func_ce << endl);

    DDS *fdds = new DDS(factory);

    fdds->filename( d_dataset ) ;
    fdds->set_dataset_name( "function_result_" + name_path( d_dataset ) ) ;

#if 0
    Connect *url = new Connect( d_dataset ) ;
    Response *r = new Response( fopen( cache_file_name.c_str(), "r" ), 0 ) ;
    if( !r->get_stream() )
        throw Error("The input source: " + cache_file_name +  " could not be opened");

    url->read_data( *fdds, r ) ;
#endif

    // fstream data(cache_file_name.c_str());
    FILE *data = fopen( cache_file_name.c_str(), "r" );
    read_data_from_cache(data, fdds);

    fdds->set_factory( 0 ) ;

    // mark everything as read.
    DDS::Vars_iter i = fdds->var_begin() ;
    DDS::Vars_iter e = fdds->var_end() ;
    for( ; i != e; i++ ) {
        BaseType *b = (*i) ;
        b->set_read_p( true ) ;
    }

    // for_each(dds->var_begin(), dds->var_end(), mfunc(BaseType::set_read_p));

    DAS *das = new DAS ;
    Ancillary::read_ancillary_das( *das, d_dataset ) ;
    fdds->transfer_attributes( das ) ;

    return fdds;
}

static const char *descrip[] = { "unknown", "dods_das", "dods_dds", "dods_data", "dods_error", "web_error", "dap4-ddx", "dap4-data", "dap4-error", "dap4-data-ddx", "dods_ddx" };

static const char *encoding[] = { "unknown", "deflate", "x-plain", "gzip", "binary" };

/** Generate an HTTP 1.0 response header for a text document. This is used
 when returning a serialized DAS or DDS object.

 @param strm Write the MIME header to this stream.
 @param type The type of this this response. Defaults to
 application/octet-stream.
 @param ver The version string; denotes the libdap implementation
 version.
 @param enc How is this response encoded? Can be plain or deflate or the
 x_... versions of those. Default is x_plain.
 @param last_modified The time to use for the Last-Modified header value.
 Default is zero which means use the current time. */
void ResponseBuilder::set_mime_text(ostream &strm, ObjectType type, EncodingType enc, const time_t last_modified, const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR << CRLF;
    strm << "XOPeNDAP-Server: " << DVR << CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    if (type == dap4_ddx)
        strm << "Content-Type: text/xml" << CRLF;
    else
        strm << "Content-Type: text/plain" << CRLF;

    // Note that Content-Description is from RFC 2045 (MIME, pt 1), not 2616.
    // jhrg 12/23/05
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (type == dods_error) // don't cache our error responses.
        strm << "Cache-Control: no-cache" << CRLF;
    // Don't write a Content-Encoding header for x-plain since that breaks
    // Netscape on NT. jhrg 3/23/97
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;
    strm << CRLF;
}

/** Generate an HTTP 1.0 response header for a html document.

 @param strm Write the MIME header to this stream.
 @param type The type of this this response.
 @param ver The version string; denotes the libdap implementation
 version.
 @param enc How is this response encoded? Can be plain or deflate or the
 x_... versions of those. Default is x_plain.
 @param last_modified The time to use for the Last-Modified header value.
 Default is zero which means use the current time. */
void ResponseBuilder::set_mime_html(ostream &strm, ObjectType type, EncodingType enc, const time_t last_modified, const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR << CRLF;
    strm << "XOPeNDAP-Server: " << DVR << CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    strm << "Content-type: text/html" << CRLF;
    // See note above about Content-Description header. jhrg 12/23/05
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (type == dods_error) // don't cache our error responses.
        strm << "Cache-Control: no-cache" << CRLF;
    // Don't write a Content-Encoding header for x-plain since that breaks
    // Netscape on NT. jhrg 3/23/97
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;
    strm << CRLF;
}

/** Write an HTTP 1.0 response header for our binary response document (i.e.,
 the DataDDS object).

 @param strm Write the MIME header to this stream.
 @param type The type of this this response. Defaults to
 application/octet-stream.
 @param ver The version string; denotes the libdap implementation
 version.
 @param enc How is this response encoded? Can be plain or deflate or the
 x_... versions of those. Default is x_plain.
 @param last_modified The time to use for the Last-Modified header value.
 Default is zero which means use the current time.
 */
void ResponseBuilder::set_mime_binary(ostream &strm, ObjectType type, EncodingType enc, const time_t last_modified, const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR << CRLF;
    strm << "XOPeNDAP-Server: " << DVR << CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    strm << "Content-Type: application/octet-stream" << CRLF;
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;

    strm << CRLF;
}

void ResponseBuilder::set_mime_multipart(ostream &strm, const string &boundary, const string &start, ObjectType type, EncodingType enc, const time_t last_modified, const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR << CRLF;
    strm << "XOPeNDAP-Server: " << DVR << CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    strm << "Content-Type: Multipart/Related; boundary=" << boundary << "; start=\"<" << start << ">\"; type=\"Text/xml\"" << CRLF;

    strm << "Content-Description: " << descrip[type] << CRLF;
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;

    strm << CRLF;
}

void ResponseBuilder::set_mime_ddx_boundary(ostream &strm, const string &boundary, const string &cid, ObjectType type, EncodingType enc) const
{
    strm << "--" << boundary << CRLF;
    strm << "Content-Type: Text/xml; charset=iso-8859-1" << CRLF;
    strm << "Content-Id: <" << cid << ">" << CRLF;
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;

    strm << CRLF;
}

void ResponseBuilder::set_mime_data_boundary(ostream &strm, const string &boundary, const string &cid, ObjectType type, EncodingType enc) const
{
    strm << "--" << boundary << CRLF;
    strm << "Content-Type: application/octet-stream" << CRLF;
    strm << "Content-Id: <" << cid << ">" << CRLF;
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;

    strm << CRLF;
}

/** Generate an HTTP 1.0 response header for an Error object.
 @param strm Write the MIME header to this stream.
 @param code HTTP 1.0 response code. Should be 400, ... 500, ...
 @param reason Reason string of the HTTP 1.0 response header.
 @param version The version string; denotes the DAP spec and implementation
 version. */
void ResponseBuilder::set_mime_error(ostream &strm, int code, const string &reason, const string &protocol) const
{
    strm << "HTTP/1.0 " << code << " " << reason.c_str() << CRLF;

    strm << "XDODS-Server: " << DVR << CRLF;
    strm << "XOPeNDAP-Server: " << DVR << CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;
    strm << "Cache-Control: no-cache" << CRLF;
    strm << CRLF;
}

} // namespace libdap

