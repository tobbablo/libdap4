
// Utility functions used by the api.
//
// jhrg 9/21/94

// $Log: util.cc,v $
// Revision 1.5  1995/01/11 16:10:47  jimg
// Added to new functions which manage XDR stdio stream pointers. One creates
// a new xdrstdio pointer associated wit the given FILE * and the other
// deletes it. The creation function returns the XDR *, so it can be used to
// initialize a new XDR * (see BaseType.cc).
//
// Revision 1.4  1994/11/29  20:21:23  jimg
// Added xdr_str and xdr_url functions (C linkage). These provide a way for
// the Str and Url classes to en/decode strings (Urls are effectively strings)
// with only two parameters. Thus the Array ad List classes might actually
// work as planned.
//
// Revision 1.3  1994/11/22  14:06:22  jimg
// Added code for data transmission to parts of the type hierarchy. Not
// complete yet.
// Fixed erros in type hierarchy headers (typos, incorrect comments, ...).
//
// Revision 1.2  1994/10/18  00:25:36  jimg
// Fixed error in char * [] allocation.
// Added debugging code.
//

static char rcsid[]={"$Id: util.cc,v 1.5 1995/01/11 16:10:47 jimg Exp $"};

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <rpc/xdr.h>

#include <SLList.h>

#include "BaseType.h"
#include "Str.h"
#include "Url.h"
#include "errmsg.h"
#include "config.h"
#include "debug.h"

#ifdef TRACE_NEW
#include "trace_new.h"
#endif

// Compare elements in a SLList of (BaseType *)s and return true if there are
// no duplicate elements, otherwise return false. Uses the same number of
// compares as qsort. (Guess why :-)
//
// NB: The elements of the array to be sorted are pointers to chars; the
// compare function gets pointers to those elements, thus the cast to (const
// char **) and the dereference to get (const char *) for strcmp's arguments.

static int
char_cmp(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

bool
unique(SLList<BaseTypePtr> l, const char *var_name, const char *type_name)
{
    // copy the identifier names to an array of char
    char **names = new char *[l.length()];
    if (!names)
	err_quit("util.cc:unique - Could not allocate array NAMES.");

    int nelem = 0;
    String s;
    for (Pix p = l.first(); p; l.next(p)) {
	names[nelem++] = strdup((const char *)l(p)->get_var_name());
	DBG(cerr << "NAMES[" << nelem-1 << "]=" << names[nelem-1] << endl);
    }
    
    // sort the array of names
    qsort(names, nelem, sizeof(char *), char_cmp);
	
#ifdef DEBUG2
    cout << "unique:" << endl;
    for (int ii = 0; ii < nelem; ++ii)
	cout << "NAMES[" << ii << "]=" << names[ii] << endl;
#endif

    // look for any instance of consecutive names that are ==
    for (int i = 1; i < nelem; ++i)
	if (!strcmp(names[i-1], names[i])) {
	    cerr << "The variable `" << names[i] 
		 << "' is used more than once in " << type_name << " `"
		 << var_name << "'" << endl;
	    for (i = 0; i < nelem; i++)
		free(names[i]);	// strdup uses malloc
	    delete [] names;
	    return false;
	}

    for (i = 0; i < nelem; i++)
	free(names[i]);		// strdup uses malloc
    delete [] names;

    return true;
}

// This function is used to allocate memory for, and initialize, and new XDR
// pointer. It sets the stream associated with the (XDR *) to STREAM.
//
// NB: STREAM is not one of the C++/libg++ iostream classes; it is a (FILE
// *).

XDR *
new_xdrstdio(FILE *stream, enum xdr_op xop)
{
    XDR *xdr = new(XDR);
    
    xdrstdio_create(xdr, stream, xop);
    
    return xdr;
}

// Delete an XDR pointer allocated using the above function. Do not close the
// associated FILE pointer.

void
delete_xdrstdio(XDR *xdr)
{
    xdr_destroy(xdr);
    delete(xdr);
}

// These functions are used to en/decode Str and Url type variables. They are
// defined as extern C since they are passed via function pointers to
// routines in the xdr library where they are executed. 
// These functions are defined so that *every* direct descendent of BaseType
// has an en/decoder which takes exactly two argumnets: an XDR * and a buffer
// pointer.

extern "C" bool_t
xdr_url(XDR *xdrs, char **buf)
{
    return xdr_string(xdrs, buf, max_url_len);
}

extern "C" bool_t
xdr_str(XDR *xdrs, char **buf)
{
    return xdr_string(xdrs, buf, max_str_len);
}
    
