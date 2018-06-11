#ifndef OPT_TRACE_CTX_INCLUDED
#define OPT_TRACE_CTX_INCLUDED

class Opt_trace_info;
class Json_writer;
class THD;

class Opt_trace_ctx
{
public:
  Opt_trace_ctx(): current_trace(NULL), start_cnt(0) {}
  ~Opt_trace_ctx();
private:
  Dynamic_array<Opt_trace_info *> traces;
  Opt_trace_info *current_trace;
  THD *thd;
  Json_writer *current_Json();
  int start_cnt;

  friend class Opt_trace_start;
  friend class Opt_trace_object;
  friend class Opt_trace_array;
  friend int fill_optimizer_trace_info(THD *, TABLE_LIST *, Item *);
};

#endif
