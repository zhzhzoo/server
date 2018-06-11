#ifndef OPT_TRACE_INCLUDED
#define OPT_TRACE_INCLUDED
/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

class Item;
class THD;
class TABLE_LIST;
class TABLE;
class Opt_trace_info;
class Opt_trace_start;
typedef struct st_dynamic_array DYNAMIC_ARRAY;

#include "opt_trace_ctx.h"

class Opt_trace_start
{
public:
  Opt_trace_start(THD *thd,
                  Opt_trace_ctx *ctx,
                  const char *query,
                  size_t query_length,
                  const CHARSET_INFO *query_charset);
  ~Opt_trace_start();
private:
  Opt_trace_ctx *ctx;
};

class Opt_trace_object
{
public:
  Opt_trace_object(Opt_trace_ctx *ctx);
  Opt_trace_object(Opt_trace_ctx *ctx, const char *k);
  ~Opt_trace_object();
  Opt_trace_object &add(const char *k, longlong v);
  Opt_trace_object &add(const char *k, double v);
  Opt_trace_object &add(const char *k, int v);
  Opt_trace_object &add(const char *k, uint v);
  Opt_trace_object &add(const char *k, ulong v);
  Opt_trace_object &add(const char *k, ulonglong v);
  Opt_trace_object &add(const char *k, bool v);
  Opt_trace_object &add(const char *k, const char* v);
  Opt_trace_object &add(const char *k, const String &v);
  Opt_trace_object &add(const char *k, LEX_CSTRING &v);
  Opt_trace_object &add(const char *k, COND *v);
  Opt_trace_object &add(const char *k, TABLE_LIST *v);
  Opt_trace_object &add(const char *k);
  Opt_trace_object &add_null(const char *k);
private:
  Opt_trace_ctx *ctx;
};

class Opt_trace_array
{
public:
  Opt_trace_array(Opt_trace_ctx *ctx);
  Opt_trace_array(Opt_trace_ctx *ctx, const char *k);
  ~Opt_trace_array();
  Opt_trace_array &add(longlong v);
  Opt_trace_array &add(double v);
  Opt_trace_array &add(int v);
  Opt_trace_array &add(uint v);
  Opt_trace_array &add(ulong v);
  Opt_trace_array &add(ulonglong v);
  Opt_trace_array &add(bool v);
  Opt_trace_array &add(const char* v);
  Opt_trace_array &add(const String &v);
  Opt_trace_array &add(LEX_CSTRING &v);
  Opt_trace_array &add(COND *v);
  Opt_trace_array &add(TABLE_LIST *v);
  Opt_trace_array &add_null();
private:
  Opt_trace_ctx *ctx;
};

int fill_optimizer_trace_info(THD *thd, TABLE_LIST *tables, Item *);
#endif
