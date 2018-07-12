/* Copyright (C) 2014 SkySQL Ab, MariaDB Corporation Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA */

#include "mariadb.h"
#include "sql_priv.h"
#include "sql_string.h"
#include "sql_list.h"
#include "sql_array.h"

#include "my_json_writer.h"

void Json_writer::append_indent()
{
  if (one_line)
    return;
  if (!document_start)
    output.append('\n');
  for (int i=0; i< indent_level; i++)
    output.append(' ');
}

void Json_writer::start_object()
{
  this_member_has_name = false;
  fmt_helper.on_start_object();

  if (end_marker && !this_member_has_name)
    member_name_beginnings.append_val(*member_name_beginnings.back());

  if (!element_started)
    start_element();

  output.append("{");
  indent_level+=INDENT_SIZE;
  first_child.append_val(true);
  element_started= false;
  document_start= false;
}

void Json_writer::start_array()
{
  bool orig_this_member_has_name = this_member_has_name;
  this_member_has_name = false;

  if (fmt_helper.on_start_array())
    return;

  if (end_marker && !orig_this_member_has_name)
    member_name_beginnings.append_val(*member_name_beginnings.back());

  if (!element_started)
    start_element();

  output.append("[");
  indent_level+=INDENT_SIZE;
  first_child.append_val(true);
  element_started= false;
  document_start= false;
}

void Json_writer::end_object()
{
  indent_level-=INDENT_SIZE;
  if (!first_child.pop())
    append_indent();
  output.append("}");
  add_end_marker();
}


void Json_writer::end_array()
{
  if (fmt_helper.on_end_array())
    return;

  indent_level-=INDENT_SIZE;
  if (!first_child.pop())
    append_indent();
  output.append("]");
  // if fmt_helper helped with this array we don't add end marker
  add_end_marker();
}


Json_writer& Json_writer::add_member(const char *name)
{
  size_t len = strlen(name);

  if (fmt_helper.on_add_member(name))
    return *this; // handled

  if (end_marker)
  {
    member_name_beginnings.append_val(*member_name_beginnings.back() + len);
    member_names.append(name);
    this_member_has_name = true;
  }

  // assert that we are in an object
  DBUG_ASSERT(!element_started);
  start_element();

  output.append('"');
  output.append(name);
  output.append("\": ");
  return *this;
}


/* 
  Used by formatting helper to print something that is formatted by the helper.
  We should only separate it from the previous element.
*/

void Json_writer::start_sub_element()
{
  //element_started= true;
  if (*first_child.back())
    *first_child.back()= false;
  else
    output.append(',');

  append_indent();
}


void Json_writer::start_element()
{
  element_started= true;

  if (*first_child.back())
    *first_child.back()= false;
  else
    output.append(',');

  append_indent();
}

void Json_writer::add_ll(longlong val)
{
  char buf[64];
  my_snprintf(buf, sizeof(buf), "%lld", val);
  add_unquoted_str(buf);
}


/* Add a memory size, printing in Kb, Kb, Gb if necessary */
void Json_writer::add_size(longlong val)
{
  char buf[64];
  if (val < 1024) 
    my_snprintf(buf, sizeof(buf), "%lld", val);
  else if (val < 1024*1024*16)
  {
    /* Values less than 16MB are specified in KB for precision */
    size_t len= my_snprintf(buf, sizeof(buf), "%lld", val/1024);
    strcpy(buf + len, "Kb");
  }
  else
  {
    size_t len= my_snprintf(buf, sizeof(buf), "%lld", val/(1024*1024));
    strcpy(buf + len, "Mb");
  }
  add_str(buf);
}


void Json_writer::add_double(double val)
{
  char buf[64];
  my_snprintf(buf, sizeof(buf), "%lg", val);
  add_unquoted_str(buf);
}


void Json_writer::add_bool(bool val)
{
  add_unquoted_str(val? "true" : "false");
}


void Json_writer::add_null()
{
  add_unquoted_str("null");
}


void Json_writer::add_unquoted_str(const char* str)
{
  size_t len = strlen(str);
  if (fmt_helper.on_add_str(str, len, /* unquoted= */true))
    return;

  if (!element_started)
    start_element();

  remove_name();
  output.append(str);
  element_started= false;
}


void Json_writer::add_str(const char *str, size_t len)
{
  if (fmt_helper.on_add_str(str, len))
    return;

  if (!element_started)
    start_element();

  remove_name();
  output.append('"');
  output.append(str, len);
  output.append('"');
  element_started= false;
}


void Json_writer::remove_name()
{
  if (this_member_has_name)
  {
    member_name_beginnings.pop();
    this_member_has_name = false;
    member_names.length(*member_name_beginnings.back());
  }
}


void Json_writer::add_end_marker()
{
  if (!end_marker)
    return;
  int end_pos = member_name_beginnings.pop();
  int start_pos = *member_name_beginnings.back();
  if (start_pos != end_pos)
  {
    output.append(" /* ");
    output.append(member_names.c_ptr() + start_pos, end_pos - start_pos);
    output.append(" */");
  }
  member_names.length(start_pos);
}


bool Single_line_formatting_helper::on_add_member(const char *name)
{
  DBUG_ASSERT(state== INACTIVE || state == DISABLED);
  if (state != DISABLED)
  {
    // remove everything from the array
    buf_ptr= buffer;

    //append member name to the array
    size_t len= strlen(name);
    if (len < MAX_LINE_LEN)
    {
      memcpy(buf_ptr, name, len);
      buf_ptr+=len;
      *(buf_ptr++)= 0;

      line_len= owner->indent_level + (uint)len + 1;
      state= ADD_MEMBER;
      return true; // handled
    }
  }
  return false; // not handled
}


bool Single_line_formatting_helper::on_start_array()
{
  if (state == ADD_MEMBER)
  {
    state= IN_ARRAY;
    return true; // handled
  }
  else
  {
    if (state != DISABLED)
      state= INACTIVE;
    // TODO: what if we have accumulated some stuff already? shouldn't we
    // flush it?
    return false; // not handled
  }
}


bool Single_line_formatting_helper::on_end_array()
{
  if (state == IN_ARRAY)
  {
    flush_on_one_line();
    state= INACTIVE;
    return true; // handled
  }
  return false; // not handled
}


void Single_line_formatting_helper::on_start_object()
{
  // Nested objects will not be printed on one line
  disable_and_flush();
}


bool Single_line_formatting_helper::on_add_str(const char *str, size_t len,
                                               bool unquoted)
{
  if (state == IN_ARRAY)
  {
    // New length will be:
    //  "$string", 
    //  quote + quote + comma + space = 4
    //  or unquoted
    //  comma + space = 2
    size_t additional_len = unquoted ? 2 : 4;

    if (line_len + len + additional_len > MAX_LINE_LEN)
    {
      disable_and_flush();
      return false; // didn't handle the last element
    }

    //append string to array
    buf_unquoted[buf_ptr - buffer] = unquoted;
    memcpy(buf_ptr, str, len);
    buf_ptr+=len;
    *(buf_ptr++)= 0;
    line_len += (uint)len + additional_len;
    return true; // handled
  }

  disable_and_flush();
  return false; // not handled
}


/*
  Append everything accumulated to the output on one line
*/

void Single_line_formatting_helper::flush_on_one_line()
{
  owner->start_sub_element();
  char *ptr= buffer;
  int nr= 0;
  while (ptr < buf_ptr)
  {
    char *str= ptr;

    if (nr == 0)
    {
      owner->output.append('"');
      owner->output.append(str);
      owner->output.append("\": ");
      owner->output.append('[');
    }
    else
    {
      if (nr != 1)
        owner->output.append(", ");
      if (buf_unquoted[str - buffer])
        owner->output.append(str);
      else
      {
        owner->output.append('"');
        owner->output.append(str);
        owner->output.append('"');
      }
    }
    nr++;

    while (*ptr!=0)
      ptr++;
    ptr++;
  }
  owner->output.append(']');
  /* We've printed out the contents of the buffer, mark it as empty */
  buf_ptr= buffer;
}


void Single_line_formatting_helper::disable_and_flush()
{
  if (state == DISABLED)
    return;

  bool start_array= (state == IN_ARRAY);
  state= DISABLED;
  // deactivate ourselves and flush all accumulated calls.
  char *ptr= buffer;
  int nr= 0;
  while (ptr < buf_ptr)
  {
    char *str= ptr;
    if (nr == 0)
    {
      owner->add_member(str);
      if (start_array)
        owner->start_array();
    }
    else
    {
      //if (nr == 1)
      //  owner->start_array();
      if (buf_unquoted[str - buffer])
        owner->add_unquoted_str(str);
      else
        owner->add_str(str);
    }
    
    nr++;
    while (*ptr!=0)
      ptr++;
    ptr++;
  }
  buf_ptr= buffer;
  state= INACTIVE;
}

