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
class Opt_trace_info;
class Opt_trace_start;

class Opt_trace_ctx
{
public:
  ~Opt_trace_ctx();
private:
  Dynamic_array<Opt_trace_info *> traces;
  Opt_trace_info *current_trace;
  Json_writer *current_Json();

  friend class Opt_trace_start;
  friend class Opt_trace_object;
  friend class Opt_trace_array;
  friend int fill_optimizer_trace_info(THD *, TABLE_LIST *, Item *);
};

class Opt_trace_start
{
public:
  Opt_trace_start(Opt_trace_ctx *ctx,
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
  ~Opt_trace_object();
  Opt_trace_object &add(const char *k, longlong v);
  Opt_trace_object &add(const char *k, double v);
  Opt_trace_object &add(const char *k, const char* v);
  Opt_trace_object &add(const char *k, const String &v);
  Opt_trace_object &add_null(const char *k);
private:
  Opt_trace_ctx *ctx;
};

class Opt_trace_array
{
public:
  Opt_trace_array(Opt_trace_ctx *ctx);
  ~Opt_trace_array();
  Opt_trace_array &add(longlong v);
  Opt_trace_array &add(double v);
  Opt_trace_array &add(const char* v);
  Opt_trace_array &add(const String &v);
  Opt_trace_array &add_null();
private:
  Opt_trace_ctx *ctx;
};

int fill_optimizer_trace_info(THD *thd, TABLE_LIST *tables, Item *);
#endif
