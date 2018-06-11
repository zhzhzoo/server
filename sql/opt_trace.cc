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

#include "mariadb.h"
#include "field.h"
#include "sql_array.h"
#include "sql_class.h"
#include "sql_select.h"
#include "sql_show.h"
#include "sql_string.h"
#include "table.h"
#include "opt_trace_info.h"
#include "opt_trace.h"

#define FT_KEYPART   (MAX_FIELDS+10)

static bool TRACE_ENABLED(THD *thd)
{
  return thd->variables.optimizer_trace & OPTIMIZER_TRACE_ENABLED;
}

static void do_add(Json_writer *writer, COND *cond)
{
  char buff[1024];
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  str.length(0);
  str.extra_allocation(1024);
  if (cond)
    cond->print(&str, QT_ORDINARY);
  writer->add_str(str);
}

static void do_add(Json_writer *writer, THD *thd, TABLE_LIST *tl)
{
  char buff[64];
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  str.length(0);
  tl->print(thd, (table_map) 0L, &str,
      enum_query_type(QT_TO_SYSTEM_CHARSET | QT_SHOW_SELECT_NUMBER));
                      
  writer->add_str(str);
}

Opt_trace_ctx::~Opt_trace_ctx()
{
  if (current_trace)
  {
    delete current_trace;
  }

  while (traces.elements())
  {
    delete traces.pop();
  }
}

Opt_trace_start::Opt_trace_start(THD *thd,
                                 Opt_trace_ctx *ctx,
                                 const char *query,
                                 size_t query_length,
                                 const CHARSET_INFO *query_charset)
{
  DBUG_ASSERT(ctx->current_trace == NULL);
  sql_print_information("Started");

  if (ctx->start_cnt++ != 0)
  {
    this->ctx = ctx;
    return;
  }

  if (TRACE_ENABLED(thd))
  {
    this->ctx = ctx;
    ctx->current_trace = new Opt_trace_info(query, query_length, query_charset);
    ctx->thd = thd;
  }
}

Opt_trace_start::~Opt_trace_start()
{
  sql_print_information("~Started");
  if (--ctx->start_cnt == 0)
  {
    ctx->traces.push(ctx->current_trace);
    ctx->current_trace = NULL;

    if (ctx->traces.elements() >= 3) // keep only the 2 newest traces
    {
      delete ctx->traces.at(0);
      ctx->traces.del(0);
    }
  }
}

Opt_trace_object::Opt_trace_object(Opt_trace_ctx *ctx)
{
  this->ctx = ctx;
  ctx->current_Json()->start_object();
}

Opt_trace_object::Opt_trace_object(Opt_trace_ctx *ctx, const char *k)
{
  this->ctx = ctx;
  ctx->current_Json()->add_member(k).start_object();
}

Opt_trace_object::~Opt_trace_object()
{
  ctx->current_Json()->end_object();
}

Opt_trace_object &Opt_trace_object::add(const char *k, longlong v)
{
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, uint v)
{
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, ulong v)
{
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, ulonglong v)
{
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, bool v)
{
  ctx->current_Json()->add_member(k).add_bool(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, double v)
{
  ctx->current_Json()->add_member(k).add_double(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, int v)
{
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, const char *v)
{
  ctx->current_Json()->add_member(k).add_str(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, const String &v)
{
  ctx->current_Json()->add_member(k).add_str(v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, LEX_CSTRING &v)
{
  ctx->current_Json()->add_member(k).add_str(v.str);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, COND *v)
{
  do_add(&ctx->current_Json()->add_member(k), v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add(const char *k, TABLE_LIST *v)
{
  do_add(&ctx->current_Json()->add_member(k), ctx->thd, v);
  return *this;
}

Opt_trace_object &Opt_trace_object::add_null(const char *k)
{
  ctx->current_Json()->add_member(k).add_null();
  return *this;
}

Opt_trace_array::Opt_trace_array(Opt_trace_ctx *ctx)
{
  this->ctx = ctx;
  ctx->current_Json()->start_array();
}

Opt_trace_array::Opt_trace_array(Opt_trace_ctx *ctx, const char *k)
{
  this->ctx = ctx;
  ctx->current_Json()->add_member(k).start_array();
}

Opt_trace_array::~Opt_trace_array()
{
  ctx->current_Json()->end_array();
}

Opt_trace_array &Opt_trace_array::add(longlong v)
{
  ctx->current_Json()->add_ll(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(uint v)
{
  ctx->current_Json()->add_ll(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(ulong v)
{
  ctx->current_Json()->add_ll(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(ulonglong v)
{
  ctx->current_Json()->add_ll(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(bool v)
{
  ctx->current_Json()->add_bool(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(double v)
{
  ctx->current_Json()->add_double(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(int v)
{
  ctx->current_Json()->add_ll(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(const char* v)
{
  ctx->current_Json()->add_str(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(const String &v)
{
  ctx->current_Json()->add_str(v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(LEX_CSTRING &v)
{
  ctx->current_Json()->add_str(v.str);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(COND *v)
{
  do_add(ctx->current_Json(), v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add(TABLE_LIST *v)
{
  do_add(ctx->current_Json(), ctx->thd, v);
  return *this;
}

Opt_trace_array &Opt_trace_array::add_null()
{
  ctx->current_Json()->add_null();
  return *this;
}

ST_FIELD_INFO optimizer_trace_info[] = {
    /* name, length, type, value, maybe_null, old_name, open_method */
    {"QUERY", 65535, MYSQL_TYPE_STRING, 0, false, NULL, SKIP_OPEN_TABLE},
    {"OPT_TRACE", 65535, MYSQL_TYPE_STRING, 0, false, NULL, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

Json_writer *Opt_trace_ctx::current_Json()
{
  DBUG_ASSERT(current_trace != NULL);

  return &current_trace->Json;
}


int fill_optimizer_trace_info(THD *thd, TABLE_LIST *tables, Item *)
{
  TABLE *table = tables->table;
  Dynamic_array<Opt_trace_info *> *traces = &(thd->opt_trace.traces);
  int i, n = traces->elements();

  for (i = 0; i < n; i++)
  {
    Opt_trace_info *trace = traces->at(i);
    table->field[0]->store(trace->query.ptr(),
                           trace->query.length(),
                           trace->query.charset());
    table->field[1]->store(trace->Json.output.ptr(),
                           trace->Json.output.length(),
                           trace->Json.output.charset());
    if (schema_table_store_record(thd, table)) return 1;
  }

  return 0;
}
