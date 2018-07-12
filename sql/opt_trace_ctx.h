#ifndef OPT_TRACE_CTX_INCLUDED
#define OPT_TRACE_CTX_INCLUDED

class Opt_trace_info;
class Json_writer;
class THD;
template <class T> class List;
class Opt_trace_pending_base
{
public:
  virtual void create() = 0;
  virtual ~Opt_trace_pending_base() {};
};
template <class T>
class Opt_trace_pending;

class Opt_trace_ctx
{
public:
  Opt_trace_ctx() : current_trace(NULL), start_cnt(0), delete_on_end(false),
                    pending_idx(0) {}
  ~Opt_trace_ctx();

  /* if optimizer_trace variable set to enabled */
  bool enabled() const; 
  /* if optimizer_trace variable set to one_line */
  bool one_line() const; 
  /* end marker variable */
  bool end_marker() const; 
  /* memory limit variable */
  ulong memlimit() const; 
  /* offset variable */
  long offset() const; 
  /* limit variable */
  long limit() const; 

  /* if inside at least one Opt_trace_start */
  bool is_started() const { return start_cnt; };

  /* delete all traces */
  void reset();

  /**
     Names of flags for @@@@optimizer_trace variable of @c sys_vars.cc :
     @li "enabled" = tracing enabled
     @li "one_line"= see parameter of @ref Opt_trace_context::start
     @li "default".
  */
  static const char *flag_names[];

  /** Flags' numeric values for @@@@optimizer_trace variable */
  enum { FLAG_DEFAULT = 0, FLAG_ENABLED = 1 << 0, FLAG_ONE_LINE = 1 << 1 };

  /**
     Features' names for @@@@optimizer_trace_features variable of
     @c sys_vars.cc:
     @li "greedy_search" = the greedy search for a plan
     @li "range_optimizer" = the cost analysis of accessing data through
     ranges in indexes
     @li "dynamic_range" = the range optimization performed for each record
                           when access method is dynamic range
     @li "repeated_subselect" = the repeated execution of subselects
     @li "default".
  */
  static const char *feature_names[];

  /** Features' numeric values for @@@@optimizer_trace_features variable */
  enum feature_value {
    GREEDY_SEARCH = 1 << 0,
    RANGE_OPTIMIZER = 1 << 1,
    DYNAMIC_RANGE = 1 << 2,
    REPEATED_SUBSELECT = 1 << 3,
    /*  
      If you add here, update feature_value of empty implementation
      and default_features!
    */
    /** 
       Anything unclassified, including the top object (thus, by "inheritance
       from parent", disabling MISC makes an empty trace).
       This feature cannot be disabled by the user; for this it is important
       that it always has biggest flag; flag's value itself does not matter.
    */
    MISC = 1 << 7
  }; 

  /// Optimizer features which are traced by default.
  static const feature_value default_features;

private:
  Dynamic_array<Opt_trace_info *> traces;
  Opt_trace_info *current_trace;
  THD *thd;
  Json_writer *current_Json();
  int start_cnt;

  /*
     the current trace is not included in IS table (and traces array),
     so when we reset(), all traces in traces array are deleted, but
     current_trace is not. Then we set delete_on_end, so that when the
     trace ends it is deleted rather than put into the traces array.
  */
  bool delete_on_end;

  friend class Opt_trace_start;
  friend class Opt_trace_object;
  friend class Opt_trace_array;
  friend int fill_optimizer_trace_info(THD *, TABLE_LIST *, Item *);
  template <typename T> friend class Opt_trace_pending;

#ifndef DBUG_OFF
  enum Opt_state
  {
    OPT_ARRAY,
    OPT_OBJECT
  } curr_stat;
  List<char> stat_stack;
  void push_state(enum Opt_state st) { stat_stack.push_front((char *)0 + st); }
  enum Opt_state pop_state() { return (enum Opt_state)(stat_stack.pop() - (char *)0); }
  enum Opt_state top_state() { return (enum Opt_state)(stat_stack.head() - (char *)0); }
#endif

  uint pending_idx;
  Dynamic_array<Opt_trace_pending_base *> pending_deque;
  void push_pending(Opt_trace_pending_base *pending)
  {
    if (pending_idx > pending_deque.elements())
    {
      pending_idx = pending_deque.elements();
    }
    pending_deque.push(pending);
  }
  void pop_pending()
  { pending_deque.pop(); }
public:
  void create_pending()
  {
    while (pending_idx < pending_deque.elements())
    {
      pending_deque.at(pending_idx)->create();
      pending_idx++;
    }
  }
};

template <class T>
class Opt_trace_pending : public Opt_trace_pending_base
{
public:
  Opt_trace_pending(Opt_trace_ctx *ctx, const char *name = NULL)
    : ctx(ctx), name(name), initialized(false)
  { ctx->push_pending(this); }
  virtual void create()
  {
    if (!initialized)
    {
      if (name)
        new (buf) T(ctx, name);
      else
        new (buf) T(ctx);
      initialized = true;
    }
  }
  T *get() { return initialized ? (T *)buf : NULL; }
  ~Opt_trace_pending() { if (initialized) ((T *)buf)->~T(); ctx->pop_pending(); }
private:
  char buf[sizeof(T)];
  Opt_trace_ctx *ctx;
  const char *name;
  bool initialized;
};

int fill_optimizer_trace_info(THD *thd, TABLE_LIST *tables, Item *);

#endif
