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
#include "sql_array.h"
#include "sql_string.h"
#include "sql_class.h"
#include "sql_show.h"
#include "field.h"
#include "table.h"
#include "opt_trace_info.h"
#include "opt_trace.h"

Opt_trace_ctx::~Opt_trace_ctx() {
  if (current_trace) {
    delete current_trace;
  }

  while (traces.elements()) {
    delete traces.pop();
  }
}

Opt_trace_start::Opt_trace_start(Opt_trace_ctx *ctx,
                                 const char *query,
                                 size_t query_length,
                                 const CHARSET_INFO *query_charset) {
  DBUG_ASSERT(ctx->current_trace == NULL);

  this->ctx = ctx;
  ctx->current_trace = new Opt_trace_info(query, query_length, query_charset);
  ctx->current_Json()->add_str("Hello world!");
}

Opt_trace_start::~Opt_trace_start() {
  DBUG_ASSERT(ctx->current_trace != NULL);

  ctx->traces.push(ctx->current_trace);
  ctx->current_trace = NULL;
}

Opt_trace_object::Opt_trace_object(Opt_trace_ctx *ctx) {
  this->ctx = ctx;
  ctx->current_Json()->start_object();
}

Opt_trace_object::~Opt_trace_object() {
  ctx->current_Json()->end_object();
}

Opt_trace_object &Opt_trace_object::add(const char *k, longlong v) {
  ctx->current_Json()->add_member(k).add_ll(v);
}

Opt_trace_object &Opt_trace_object::add(const char *k, double v) {
  ctx->current_Json()->add_member(k).add_double(v);
}

Opt_trace_object &Opt_trace_object::add(const char *k, const char *v) {
  ctx->current_Json()->add_member(k).add_str(v);
}

Opt_trace_object &Opt_trace_object::add(const char *k, const String &v) {
  ctx->current_Json()->add_member(k).add_str(v);
}

Opt_trace_object &Opt_trace_object::add_null(const char *k) {
  ctx->current_Json()->add_member(k).add_null();
}

Opt_trace_array::Opt_trace_array(Opt_trace_ctx *ctx) {
  this->ctx = ctx;
  ctx->current_Json()->start_array();
}

Opt_trace_array::~Opt_trace_array() {
  ctx->current_Json()->end_array();
}

Opt_trace_array &Opt_trace_array::add(longlong v) {
  ctx->current_Json()->add_ll(v);
}

Opt_trace_array &Opt_trace_array::add(double v) {
  ctx->current_Json()->add_double(v);
}

Opt_trace_array &Opt_trace_array::add(const char* v) {
  ctx->current_Json()->add_str(v);
}

Opt_trace_array &Opt_trace_array::add(const String &v) {
  ctx->current_Json()->add_str(v);
}

Opt_trace_array &Opt_trace_array::add_null() {
  ctx->current_Json()->add_null();
}

ST_FIELD_INFO optimizer_trace_info[] = {
    /* name, length, type, value, maybe_null, old_name, open_method */
    {"QUERY", 65535, MYSQL_TYPE_STRING, 0, false, NULL, SKIP_OPEN_TABLE},
    {"OPT_TRACE", 65535, MYSQL_TYPE_STRING, 0, false, NULL, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

Json_writer *Opt_trace_ctx::current_Json() {
  DBUG_ASSERT(current_trace != NULL);

  return &current_trace->Json;
}


int fill_optimizer_trace_info(THD *thd, TABLE_LIST *tables, Item *)
{
  TABLE *table = tables->table;
  Dynamic_array<Opt_trace_info *> *traces = &(thd->opt_trace.traces);
  int i, n = traces->elements();

  for (i = 0; i < n; i++) {
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
