// -*- C++ -*-

// (c) COPYRIGHT URI/MIT 1997-1999
// Please read the full copyright statement in the file COPYRIGHT.
//
// Authors:
//      jhrg,jimg       James Gallagher (jgallagher@gso.uri.edu)

// Specialize DDS for returned data. This currently means adding version
// information about the source of the data. Was it from a version 1, 2 or
// later server?
// 
// jhrg 9/19/97

#ifndef _datadds_h
#define _datadds_h 1

#ifdef __GNUG__
#pragma interface
#endif

#include <iostream>
#include <string>
#include <Pix.h>
#include <SLList.h>

#ifndef _dds_h
#include "DDS.h"
#endif

/** This class adds some useful state information to the DDS
    structure.  It is for use on the client side of the DODS
    connection. 
    
    @memo Holds a DODS DDS.
    @see Connect
    */

class DataDDS : public DDS {
private:
    string _server_version;
    int _server_version_major;
    int _server_version_minor;

    void _version_string_to_numbers();

    // The last level read from a sequence. This is used to read nested
    // sequences. 
    int _sequence_level;
public:
  /** The DataDDS constructor needs a name and a version string.  This
      is generally received from the server.
      */
    DataDDS(const string &n = "", const string &v = "");
    virtual ~DataDDS();

  /** Sets the version string.  This typically looks something like:
      #DODS/2.15#, where ``2'' is the major version number, and ``15''
      the minor number.
      */
    void set_version(const string &v);
  /** Returns the major version number. */
    int get_version_major();
  /** Returns the minor version number. */
    int get_version_minor();

  /** Return the last level of a sequence object that was read. Note
      that #Sequence::deserialize()# is the main user of this
      information and it really only matters in cases where the
      Sequence object contains other Sequence objects. In that case,
      this information provides state for #Sequence::deserialize()# so
      that it can return to the level at which it last read.

      @name sequence\_level()
      @memo Returns the level of the last sequence read.  */
    int sequence_level();
    
  /** Set the value for #sequence_level()#. Use this function to store
      state information about the current sequence. This is used
      mostly when reading nested sequences so that
      #Sequence::deserialize()# can return to the correct level when
      resuming a deserialization from a subsequent call.

      @name set\_sequence\_level(int level)
      @memo Sets the level of the sequence being read.  */
    void set_sequence_level(int level);
};

// $Log: DataDDS.h,v $
// Revision 1.8  2000/09/22 02:17:19  jimg
// Rearranged source files so that the CVS logs appear at the end rather than
// the start. Also made the ifdef guard symbols use the same naming scheme and
// wrapped headers included in other headers in those guard symbols (to cut
// down on extraneous file processing - See Lakos).
//
// Revision 1.7  2000/08/02 22:46:49  jimg
// Merged 3.1.8
//
// Revision 1.4.6.1  2000/08/02 21:10:07  jimg
// Removed the header config_dap.h. If this file uses the dods typedefs for
// cardinal datatypes, then it gets those definitions from the header
// dods-datatypes.h.
//
// Revision 1.6  2000/07/09 21:57:09  rmorris
// Mods's to increase portability, minimuze ifdef's in win32 and account
// for differences between the Standard C++ Library - most notably, the
// iostream's.
//
// Revision 1.5  2000/06/07 18:06:58  jimg
// Merged the pc port branch
//
// Revision 1.4.20.1  2000/06/02 18:21:26  rmorris
// Mod's for port to Win32.
//
// Revision 1.4  1999/04/29 02:29:29  jimg
// Merge of no-gnu branch
//
// Revision 1.3.6.1  1999/02/02 21:56:58  jimg
// String to string version
//
// Revision 1.3  1998/02/05 20:13:52  jimg
// DODS now compiles with gcc 2.8.x
//
// Revision 1.2  1998/01/12 14:27:57  tom
// Second pass at class documentation.
//
// Revision 1.1  1997/09/22 22:19:27  jimg
// Created this subclass of DDS to hold version information in the data DDS
//

#endif // _datadds_h
