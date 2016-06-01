/* Copyright (c) 2005, 2011, Oracle and/or its affiliates

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_config.h"
//#include <stdlib.h>
//#include <ctype.h>
//#include <mysql/plugin_ftparser.h>
//#include <m_ctype.h>
#include <fts0tokenize.h>
#include <thai/thbrk.h>
#include <thai/thwchar.h>
#include <thai/thwbrk.h>

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

/*
  Thai full-text parser plugin that acts as a replacement for the
  built-in full-text parser:
  - All non-whitespace characters are significant and are interpreted as
   "word characters."
  - Whitespace characters are space, tab, CR, LF.
  - There is no minimum word length.  Non-whitespace sequences of one
    character or longer are words.
  - Stopwords are used in non-boolean mode, not used in boolean mode.
*/

/*
  thai_parser interface functions:

  Plugin declaration functions:
  - thai_parser_plugin_init()
  - thai_parser_plugin_deinit()

  Parser descriptor functions:
  - thai_parser_parse()
  - thai_parser_init()
  - thai_parser_deinit()
*/


/*
  Initialize the parser plugin at server start or plugin installation.

  SYNOPSIS
    thai_parser_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int thai_parser_plugin_init(void *arg __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    thai_parser_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int thai_parser_plugin_deinit(void *arg __attribute__((unused)))
{
  return(0);
}


/*
  Initialize the parser on the first use in the query

  SYNOPSIS
    thai_parser_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int thai_parser_init(MYSQL_FTPARSER_PARAM *param
                              __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser at the end of the query

  SYNOPSIS
    thai_parser_deinit()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int thai_parser_deinit(MYSQL_FTPARSER_PARAM *param
                                __attribute__((unused)))
{
  return(0);
}


/*
  Pass a word back to the server.

  SYNOPSIS
    add_word()
      param              parsing context of the plugin
      word               a word
      len                word length

  DESCRIPTION
    Fill in boolean metadata for the word (if parsing in boolean mode)
    and pass the word to the server.  The server adds the word to
    a full-text index when parsing for indexing, or adds the word to
    the list of search terms when parsing a search string.
*/

static void add_word(MYSQL_FTPARSER_PARAM *param, const char *word, size_t len)
{
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info=
    { FT_TOKEN_WORD, 0, 0, 0, 0, (word - param->doc), ' ', 0 };

  param->mysql_add_word(param,  const_cast<char*>(word), len, &bool_info);
}

/*
  Parse a document or a search query.

  SYNOPSIS
    thai_parser_parse()
      param              parsing context

  DESCRIPTION
    This is the main plugin function which is called to parse
    a document or a search query. The call mode is set in
    param->mode.  This function simply splits the text into words
    and passes every word to the MySQL full-text indexing engine.
*/

static int thai_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  int numBytePerChar, numCut, numChar;
  int *pos;
  int i;
  uchar *toStr;
  uint *error;
 
  toStr= (uchar *) malloc(sizeof(uchar) * param->length * my_charset_tis620_thai_ci.mbmaxlen);
  error= (uint *) malloc(sizeof(uint) * param->length);
  numBytePerChar= param->cs->mbmaxlen;

  /* convert to tis620, make it compatible with libthai */
  numChar= my_convert((char *)toStr, param->length,&my_charset_tis620_thai_ci, 
                      param->doc, param->length, param->cs,error);
  

  /* find words boundary */ 
  pos = (int *)malloc(sizeof(int) * param->length);
  numCut = th_brk (toStr, pos, param->length);
  
  /* split word, add to index */
  for(i = 0; i <= numCut; i++) {
    if (i == 0) {
      add_word(param, param->doc, pos[i] * numBytePerChar);  
    } else {
      add_word(param, param->doc + (pos[i - 1] * numBytePerChar),
               (pos[i] - pos[i - 1]) * numBytePerChar);  
    }
  }
  
  free(pos);
  free(toStr);
  free(error);
  return(0);
}

static int simple_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  const char *end, *start, *docend= param->doc + param->length;

  for (end= start= param->doc;; end++)
  {
    if (end == docend)
    {
      if (end > start)
        add_word(param, start, end - start);
      break;
    }
    else if (isspace(*end))
    {
      if (end > start)
        add_word(param, start, end - start);
      start= end + 1;
    }
  }
  return(0);
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_ftparser thai_parser_descriptor=
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  thai_parser_parse,              /* parsing function       */
  thai_parser_init,               /* parser init function   */
  thai_parser_deinit              /* parser deinit function */
};
/*
  Plugin library descriptor
*/

mysql_declare_plugin(thaift_parser)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &thai_parser_descriptor,  /* descriptor                      */
  "thaift_parser",            /* name                            */
  "Pruet Boonma and Vee Satayamas",              /* author                          */
  "Thai Full-Text Parser",  /* description                     */
  PLUGIN_LICENSE_GPL,
  thai_parser_plugin_init,  /* init function (when loaded)     */
  thai_parser_plugin_deinit,/* deinit function (when unloaded) */
  0x0001,                     /* version                         */
  NULL,              /* status variables                */
  NULL,    /* system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;