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
#include "my_json_writer.h"

class Opt_trace_info
{
public:
  Opt_trace_info(const char *query, size_t query_length, const CHARSET_INFO *query_charset,
                 bool one_line, bool end_marker)
    :Json(one_line, end_marker)
  {
    this->query.copy(query, query_length, query_charset);
  }
  String query;
  Json_writer Json;
};
