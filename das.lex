
/*
   Scanner for the DAS. This file works with gnu's flex scanner generator. It
   returns either ATTR, ID, VAL, TYPE or one of the single character tokens
   `{', `}', `;', `,' or `\n' as integers. In the case of an ID or VAL, the
   scanner stores a pointer to the lexeme in yylval (whose type is char *).

   The scanner discards all comment text.

   The scanner returns quoted strings as VALs. Any characters may appear in a
   quoted string except backslash (\) and quote("). To include these escape
   them with a backslash.
   
   The scanner is not reentrant, but can share name spaces with other
   scanners.
   
   Note:
   1) The `defines' file das.tab.h is built using `bison -d'.
   2) Define YY_DECL such that the scanner is called `daslex'.
   3) When bison builds the das.tab.h file, it uses `das' instead of `yy' for
   variable name prefixes (e.g., yylval --> daslval).
   4) The quote stuff is very complicated because we want backslash (\)
   escapes to work and because we want line counts to work too. In order to
   properly scan a quoted string two C functions are used: one to remove the
   escape characters from escape sequences and one to remove the trailing
   quote on the end of the string. 

   jhrg 7/12/94 

   NB: We don't remove the \'s or ending quotes any more -- that way the
   printed das can be reparsed. 9/28/94. 
*/

/*
# $Log: das.lex,v $
# Revision 1.10  1994/12/16 22:22:43  jimg
# Fixed NEVER so that it is anything not caught by the earlier rules.
#
# Revision 1.9  1994/12/09  21:39:29  jimg
# Fixed scanner's treatment of `//' comments which end with an EOF
# (instead of a \n).
#
# Revision 1.8  1994/12/08  16:53:24  jimg
# Modified the NEVER regexp so that `[' and `]' are not allowed in the
# input stream. Previously they were not recognized but also not reported
# as errors.
#
# Revision 1.7  1994/12/07  21:17:07  jimg
# Added `,' (comma) to set of single character tokens recognized by the
# scanner. Comma is the separator for elements in attribute vectors.
#
# Revision 1.6  1994/11/10  19:46:10  jimg
# Added `/' to the set of characters that make up an identifier.
#
# Revision 1.5  1994/10/05  16:41:58  jimg
# Added `TYPE' to the grammar for the DAS.
# See Also: DAS.{cc,h} which were modified to handle TYPE.
#
# Revision 1.4  1994/08/29  14:14:51  jimg
# Fixed a problem with quoted strings - previously quotes were stripped
# when scanned, but this caused problems when they were printed because
# the printed string could not be recanned. In addition, escape characters
# are no longer removed during scanning. The functions that performed these
# operations are still in the scanner, but their calls have been commented
# out.
#
# Revision 1.3  1994/08/02  18:50:03  jimg
# Fixed error in illegal character message. Arrgh!
#
# Revision 1.2  1994/08/02  18:46:43  jimg
# Changed communication mechanism from C++ class back to daslval.
# Auxiliary functions now pass yytext,... instead of using globals.
# Fixed scanning errors.
# Scanner now sets yy_init on successful termination.
# Scanner has improved error reporting - particularly in the unterminated
# comment and quote cases.
# Better rejection of illegal characters.
#
# Revision 1.3  1994/07/25  19:01:17  jimg
# Modified scanner and parser so that they can be compiled with g++ and
# so that they can be linked using g++. They will be combined with a C++
# method using a global instance variable.
# Changed the name of line_num in the scanner to das_line_num so that
# global symbol won't conflict in executables/libraries with multiple
# scanners.
#
# Revision 1.2  1994/07/25  14:26:41  jimg
# Test files for the DAS/DDS parsers and symbol table software.
#
# Revision 1.1  1994/07/21  19:21:32  jimg
# First version of DAS scanner - works with C.
 */

%{
static char rcsid[]={"$Id: das.lex,v 1.10 1994/12/16 22:22:43 jimg Exp $"};

#include <string.h>

#define YYSTYPE char *
#define YY_DECL int daslex YY_PROTO(( void ))

#include "das.tab.h"

int das_line_num = 1;
static int start_line;		/* used in quote and comment error handlers */
void trunc1(char *yytext, int yyleng); /* no longer used */
void rmbslash(char *yytext);	/* no longer used */
int yywrap(void);

%}
    
%x quote
%x comment
%x comment_new

ID  	[a-zA-Z_][a-zA-Z0-9_/]*
VAL 	[a-zA-Z0-9_.+-]+
ATTR 	attributes|Attributes|ATTRIBUTES
TYPE    BYTE|Byte|byte|INT32|Int32|int32|FLOAT64|Float64|float64|STRING|String|string|URL|Url|url
NEVER   .*

%%

{ATTR}	    	    	daslval = yytext; return ATTR;
{TYPE}                  daslval = yytext; return TYPE;
{ID}  	    	    	daslval = yytext; return ID;
{VAL}	    	    	daslval = yytext; return VAL;
"{" 	    	    	return (int)*yytext;
"}" 	    	    	return (int)*yytext;
";" 	    	    	return (int)*yytext;
","                     return (int)*yytext;

[ \t]+
\n	    	    	++das_line_num;
<INITIAL><<EOF>>    	yy_init = 1; das_line_num = 1; yyterminate();

\"			BEGIN(quote); start_line = das_line_num; yymore();
<quote>[^"\n\\]*	yymore();
<quote>[^"\n\\]*\n	yymore(); ++das_line_num;
<quote>\\.		yymore();
<quote>\"		{ 
    			  BEGIN(INITIAL); 
			  /* trunc1(yytext, yyleng); */
                          /* rmbslash(yytext); */
			  daslval = yytext;
			  return VAL;
                        }
<quote><<EOF>>		{
                          char msg[256];
			  sprintf(msg,
				  "Unterminated quote (starts on line %d)\n",
				  start_line);
			  YY_FATAL_ERROR(msg);
                        }

"/*"	    	    	BEGIN(comment); start_line = das_line_num;
<comment>[^*\n]*
<comment>[^*\n]*\n  	++das_line_num;
<comment>"*"+[^*/\n]*
<comment>"*"+[^*/\n]*\n ++das_line_num;
<comment>"*"+"/"    	BEGIN(INITIAL);
<comment><<EOF>>	{
                          char msg[256];
			  sprintf(msg,
				  "Unterminated comment (starts on line %d)\n",
				  start_line);
			  YY_FATAL_ERROR(msg);
                        }
			
"//"	    	    	BEGIN(comment_new);
<comment_new>[^\n]*
<comment_new>\n		++das_line_num; BEGIN(INITIAL);
<comment_new><<EOF>>    yy_init = 1; das_line_num = 1; yyterminate();

{NEVER}                 {
                          if (yytext) {	/* suppress msgs about `' chars */
                            fprintf(stderr, "Character `%c' is not", *yytext);
                            fprintf(stderr, " allowed (except within");
			    fprintf(stderr, " quotes) and has been ignored\n");
			  }
			}
%%

/*
   Remove the last character from yytext. This is used to remove the trailing
   quote (") from a quotation (the fifth `quote' patern causes the trailing
   quote to be accumulated by yytext).
*/

void
trunc1(char *yytext, int yyleng)
{
    *(yytext + yyleng - 1) = '\0';
    --yyleng;
}

/*
   Remove (cut) the backslash (\) character in each instance of backslash
   followed by some character. If yytext holds `t \" \+ \\ t', then rmbslash
   will change that to `t " + \ t'.
*/

void
rmbslash(char *yytext)
{
    char *slash = strchr(yytext, (int)'\\');

    while (slash) {
        strcpy(slash, slash + 1);
	slash = strchr(slash + 1, '\\');
    }
}

int 
yywrap(void)
{
    return 1;
}
