
// -*- C++ -*-

// (c) COPYRIGHT URI/MIT 1994-1996
// Please read the full copyright statement in the file COPYRIGH.  
//
// Authors:
//      jhrg,jimg       James Gallagher (jgallagher@gso.uri.edu)

// declarations for utility functions
//
// jhrg 9/21/94

/* $Log: util.h,v $
/* Revision 1.17  1996/11/27 22:40:30  jimg
/* Added DDS as third parameter to function in the CE evaluator
/*
 * Revision 1.16  1996/11/25 03:45:08  jimg
 * Added prototypes for new functions.
 *
 * Revision 1.15  1996/11/22 00:08:34  jimg
 * Added compressor and decompresor prototypes.
 *
 * Revision 1.14  1996/10/28 23:07:37  jimg
 * Added include of UInt32.h and prototype of NewUInt32().
 *
 * Revision 1.13  1996/08/13 20:47:21  jimg
 * Added prototypes for the *_ops() functions.
 *
 * Revision 1.12  1996/05/31 23:31:11  jimg
 * Updated copyright notice.
 *
 * Revision 1.11  1996/05/29 22:09:00  jimg
 * Made changes necessary to support CEs that return the value of a function
 * instead of the value of a variable. This was done so that it would be
 * possible to translate Sequences into Arrays without first reading the
 * entire sequence over the network.
 *
 * Revision 1.10  1996/04/05 00:22:25  jimg
 * Compiled with g++ -Wall and fixed various warnings.
 *
 * Revision 1.9  1996/04/04 17:38:35  jimg
 * Merged changes from version 1.1.1.
 *
 * Revision 1.8  1996/02/02 00:31:25  jimg
 * Merge changes for DODS-1.1.0 into DODS-2.x
 *
 * Revision 1.7  1995/12/06  18:33:34  jimg
 * Added forward decalration of text_to_temp();
 *
 * Revision 1.6  1995/08/26  00:32:12  jimg
 * Removed code enclosed in #ifdef NEVER #endif.
 *
 * Revision 1.5  1995/08/23  00:41:57  jimg
 * xdr_str() now takes a String & instead of a String ** for arg 2.
 *
 * Revision 1.4.2.2  1996/02/23 21:37:34  jimg
 * Updated for new configure.in.
 * Fixed problems on Solaris 2.4.
 *
 * Revision 1.4.2.1  1995/09/29  19:28:06  jimg
 * Fixed problems with xdr.h on an SGI.
 * Fixed conflict of d_int32_t (which was in an enum type defined by BaseType) on
 * the SGI.
 *
 * Revision 1.4  1995/03/04  14:36:50  jimg
 * Fixed xdr_str so that it works with the new String objects.
 * Added xdr_str_array for use with arrays of String objects.
 *
 * Revision 1.3  1995/01/19  20:14:59  jimg
 * Added declarations for new utility functions which return pointers to
 * objects in the BaseType hierarchy.
 *
 * Revision 1.2  1994/11/29  20:21:24  jimg
 * Added xdr_str and xdr_url functions (C linkage). These provide a way for
 * the Str and Url classes to en/decode strings (Urls are effectively strings)
 * with only two parameters. Thus the Array and List classes might actually
 * work as planned.
 */

#include <stdio.h>

#include "BaseType.h"
#include "Byte.h"
#include "Int32.h"
#include "UInt32.h"
#include "Float64.h"
#include "Str.h"
#include "Url.h"
#include "Array.h"
#include "List.h"
#include "Structure.h"
#include "Sequence.h"
#include "Function.h"
#include "Grid.h"

bool unique(SLList<BaseTypePtr> l, const char *var, const char *type);
XDR *new_xdrstdio(FILE *stream, enum xdr_op xop);
XDR *set_xdrstdio(XDR *xdr, FILE *stream, enum xdr_op xop);
void delete_xdrstdio(XDR *xdr);
FILE *text_to_temp(String text);
char *systime();
FILE *compressor(FILE *output);
FILE *decompressor(FILE *input);
String dods_root();

bool func_member(int argc, BaseType *argv[], DDS &dds);
bool func_null(int argc, BaseType *argv[], DDS &dds);
BaseType *func_nth(int argc, BaseType *argv[], DDS &dds);
BaseType *func_length(int argc, BaseType *argv[], DDS &dds);

bool byte_ops(int i1, int i2, int op);
bool int_ops(int i1, int i2, int op);
bool double_ops(double i1, double i2, int op);
bool string_ops(String &i1, String &i2, int op);

extern "C" bool_t xdr_str(XDR *xdrs, String &buf);
#if 0
extern "C" bool_t xdr_str_array(XDR *xdrs, String *buf);
#endif

Byte *NewByte(const String &n = (char *)0);
Int32 *NewInt32(const String &n = (char *)0);
UInt32 *NewUInt32(const String &n = (char *)0);
Float64 *NewFloat64(const String &n = (char *)0);
Str *NewStr(const String &n = (char *)0);
Url *NewUrl(const String &n = (char *)0);
Array *NewArray(const String &n = (char *)0, BaseType *v = 0);
List *NewList(const String &n = (char *)0, BaseType *v = 0);
Structure *NewStructure(const String &n = (char *)0);
Sequence *NewSequence(const String &n = (char *)0);
Function *NewFunction(const String &n = (char *)0);
Grid *NewGrid(const String &n = (char *)0);
