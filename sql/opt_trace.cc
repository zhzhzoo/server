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

#define QT_OPT_TRACE enum_query_type(QT_WITHOUT_INTRODUCERS \
  | QT_TO_SYSTEM_CHARSET | QT_SHOW_SELECT_NUMBER \
  | QT_ITEM_IDENT_SKIP_DB_NAMES)

static void do_add(Json_writer *writer, COND *cond)
{
  StringBuffer<1024> str;
  if (cond)
  {
    cond->print(&str, QT_OPT_TRACE);
    writer->add_str(str);
  }
  else
    writer->add_null();
}


static void do_add(Json_writer *writer, THD *thd, SELECT_LEX *select_lex)
{
  StringBuffer<1024> str;
  if (select_lex)
  {
    select_lex->print(thd, &str, QT_OPT_TRACE);
    writer->add_str(str);
  }
  else
    writer->add_null();
}


static void do_add(Json_writer *writer, THD *thd, TABLE_LIST *tl)
{
  StringBuffer<1024> str;
  if (tl)
  {
    tl->print(thd, (table_map) 0L, &str, QT_OPT_TRACE);
    writer->add_str(str);
  }
  else
    writer->add_null();
}


static void do_add(Json_writer *writer, ORDER *o)
{
  StringBuffer<128> str;
  if (o)
  {
    SELECT_LEX::print_order(&str, o, QT_OPT_TRACE);
    writer->add_str(str);
  }
  else
    writer->add_null();
}


const char *Opt_trace_ctx::flag_names[] = {"enabled", "one_line", "default",
                                               NullS};

const char *Opt_trace_ctx::feature_names[] = {
    "greedy_search",      "range_optimizer", "dynamic_range",
    "repeated_subselect", "default",         NullS};

const Opt_trace_ctx::feature_value Opt_trace_ctx::default_features =
    Opt_trace_ctx::feature_value(Opt_trace_ctx::GREEDY_SEARCH |
                                 Opt_trace_ctx::RANGE_OPTIMIZER |
                                 Opt_trace_ctx::DYNAMIC_RANGE |
                                 Opt_trace_ctx::REPEATED_SUBSELECT);


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


void Opt_trace_ctx::reset()
{
  while (traces.elements())
  {
    delete traces.pop();
  }
  if (current_trace)
  {
    delete_on_end = true;
  }
}


bool Opt_trace_ctx::enabled() const
{
  return thd->variables.optimizer_trace & FLAG_ENABLED;
}


bool Opt_trace_ctx::one_line() const
{
  return thd->variables.optimizer_trace & FLAG_ONE_LINE;
}


bool Opt_trace_ctx::end_marker() const
{
  return thd->variables.end_markers_in_json;
}


ulong Opt_trace_ctx::memlimit() const
{
  return thd->variables.optimizer_trace_max_mem_size;
}


long Opt_trace_ctx::offset() const
{
  return thd->variables.optimizer_trace_offset;
}


long Opt_trace_ctx::limit() const
{
  return thd->variables.optimizer_trace_limit;
}


// In case there are mutliple nesting Opt_trace_start's
// we keep start_cnt to record how many Opt_trace_start's
// we have seen. If start_cnt = 0 and we encounter an
// Opt_trace_start, the Opt_trace_info is not created yet
// so create it. If start_cnt > 0, we have already created
// Opt_trace_info so we just increase start_cnt and return.
// In ~Opt_trace_start we actually stop the trace when
// start_cnt decreases to 0.
Opt_trace_start::Opt_trace_start(THD *thd,
                                 Opt_trace_ctx *ctx,
                                 const char *query,
                                 size_t query_length,
                                 const CHARSET_INFO *query_charset)
{
  this->ctx = ctx;
  ctx->thd = thd;
  if (ctx->enabled())
  {
    enabled_at_init = true;
    if (ctx->start_cnt++ != 0)
      return;

#ifndef DBUG_OFF
    ctx->push_state(Opt_trace_ctx::OPT_OBJECT);
#endif
    ctx->current_trace = new Opt_trace_info(query, query_length, query_charset,
                                            ctx->one_line(), ctx->end_marker());
  }
  else
  {
    enabled_at_init = false;
  }
}


Opt_trace_start::~Opt_trace_start()
{
  if (!enabled_at_init)
  {
    return;
  }
  if (--ctx->start_cnt == 0)
  {
    if (ctx->delete_on_end)
    {
      delete ctx->current_trace;
      ctx->delete_on_end = false;
    }
    else
    {
      ctx->traces.push(ctx->current_trace);
    }
    ctx->current_trace = NULL;
#ifndef DBUG_OFF
    DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
    ctx->pop_state();
#endif

    if (ctx->traces.elements() >= 3) // keep only the 2 newest traces
    {
      delete ctx->traces.at(0);
      ctx->traces.del(0);
    }
  }
  DBUG_ASSERT(ctx->start_cnt >= 0);
}


// When creating an Opt_trace_object, if the trace is started,
// we set its ctx to be the Opt_trace_ctx, otherwise set it to NULL.
// Only append to this Opt_trace_object when the ctx is not NULL,
// i.e. the trace is started when the Opt_trace_object is created.
// Same to Opt_trace_array.
Opt_trace_object::Opt_trace_object(Opt_trace_ctx *ctx)
{
  if (!ctx->is_started())
  {
    this->ctx = NULL;
    return;
  }
#ifndef DBUG_OFF
  ctx->push_state(Opt_trace_ctx::OPT_OBJECT);
#endif
  this->ctx = ctx;
  ctx->current_Json()->start_object();
}


Opt_trace_object::Opt_trace_object(Opt_trace_ctx *ctx, const char *k)
{
  if (!ctx->is_started())
  {
    this->ctx = NULL;
    return;
  }
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
  ctx->push_state(Opt_trace_ctx::OPT_OBJECT);
#endif
  this->ctx = ctx;
  ctx->current_Json()->add_member(k).start_object();
}


Opt_trace_object::~Opt_trace_object()
{
  if (!ctx) return;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
  ctx->pop_state();
#endif
  ctx->current_Json()->end_object();
}


Opt_trace_object &Opt_trace_object::add(const char *k, longlong v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, uint v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, ulong v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, ulonglong v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, bool v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_bool(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, double v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_double(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, int v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, long int v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_ll(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, const char *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_str(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, const String &v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_str(v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, LEX_CSTRING v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_str(&v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, COND *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  do_add(&ctx->current_Json()->add_member(k), v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, SELECT_LEX *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  do_add(&ctx->current_Json()->add_member(k), ctx->thd, v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, TABLE_LIST *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  do_add(&ctx->current_Json()->add_member(k), ctx->thd, v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, TABLE *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  do_add(&ctx->current_Json()->add_member(k), ctx->thd, v->pos_in_table_list);
  return *this;
}


Opt_trace_object &Opt_trace_object::add(const char *k, ORDER *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  do_add(&ctx->current_Json()->add_member(k), v);
  return *this;
}


Opt_trace_object &Opt_trace_object::add_null(const char *k)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
#endif
  ctx->current_Json()->add_member(k).add_null();
  return *this;
}


Opt_trace_array::Opt_trace_array(Opt_trace_ctx *ctx)
{
  if (!ctx->is_started())
  {
    this->ctx = NULL;
    return;
  }
#ifndef DBUG_OFF
  ctx->push_state(Opt_trace_ctx::OPT_ARRAY);
#endif
  this->ctx = ctx;
  ctx->current_Json()->start_array();
}


Opt_trace_array::Opt_trace_array(Opt_trace_ctx *ctx, const char *k)
{
  if (!ctx->is_started())
  {
    this->ctx = NULL;
    return;
  }
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_OBJECT);
  ctx->push_state(Opt_trace_ctx::OPT_ARRAY);
#endif
  this->ctx = ctx;
  ctx->current_Json()->add_member(k).start_array();
}


Opt_trace_array::~Opt_trace_array()
{
  if (!ctx) return;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
  ctx->pop_state();
#endif
  ctx->current_Json()->end_array();
}


Opt_trace_array &Opt_trace_array::add(longlong v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_ll(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(uint v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_ll(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(ulong v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_ll(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(ulonglong v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_ll(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(bool v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_bool(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(double v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_double(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(int v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_ll(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(long int v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_ll(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(const char* v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_str(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(const String &v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_str(v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(LEX_CSTRING v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  ctx->current_Json()->add_str(&v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(COND *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  do_add(ctx->current_Json(), v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(SELECT_LEX *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  do_add(ctx->current_Json(), ctx->thd, v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(TABLE_LIST *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  do_add(ctx->current_Json(), ctx->thd, v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(TABLE *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  do_add(ctx->current_Json(), ctx->thd, v->pos_in_table_list);
  return *this;
}


Opt_trace_array &Opt_trace_array::add(ORDER *v)
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
  do_add(ctx->current_Json(), v);
  return *this;
}


Opt_trace_array &Opt_trace_array::add_null()
{
  if (!ctx) return *this;
#ifndef DBUG_OFF
  DBUG_ASSERT(ctx->top_state() == Opt_trace_ctx::OPT_ARRAY);
#endif
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
