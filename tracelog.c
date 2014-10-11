/**********************************************************************

  tracelog.c -

  $Author: nyaxt $
  created at: Tue Aug 14 19:37:09 2012

  Copyright (C) 1993-2014 Yukihiro Matsumoto

**********************************************************************/

#include "ruby/ruby.h"
#include "ruby/debug.h"

#include <assert.h>
#include <time.h>
#include <sys/mman.h>

static uint64_t
clock_abs_ns(void)
{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    // assert(ret == 0);
    (void)ret;

    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t)(ts.tv_nsec);
}

typedef struct rb_tracelog_arg_struct {
    VALUE (*serialize_cb)(void*);
    void* data;
} rb_tracelog_args_t;

typedef struct rb_tracelog_event_struct {
    const char* name;
    const char* category;
    struct rb_tracelog_arg_struct *args;
    char phase;
    uint64_t timestamp;
    struct rb_tracelog_event_struct *next;
} rb_tracelog_event_t;

typedef struct rb_tracelog_chunk_struct {
    void *data_head;
    size_t len;
    size_t capacity;
    struct rb_tracelog_chunk_struct *next;
} rb_tracelog_chunk_t;

#define TRACELOG_CHUNK_SIZE 32 * 1024 * 1024

typedef struct rb_tracelog_struct {
    struct rb_tracelog_chunk_struct *head_chunk;
    struct rb_tracelog_chunk_struct *tail_chunk;

    struct rb_tracelog_event_struct *head_event;
    struct rb_tracelog_event_struct *tail_event;
} rb_tracelog_t;

void*
tracelog_malloc(size_t size)
{
    void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(ret != MAP_FAILED);
    return ret;
}

static rb_tracelog_t* g_tracelog; // FIXME: this should be thread local

static rb_tracelog_chunk_t*
chunk_new(void)
{
    rb_tracelog_chunk_t *chunk = ALLOC(rb_tracelog_chunk_t);
    chunk->data_head = tracelog_malloc(TRACELOG_CHUNK_SIZE);
    chunk->len = 0;
    chunk->capacity = TRACELOG_CHUNK_SIZE;
    chunk->next = NULL;
    return chunk;
}

static void*
chunk_alloc_mem(rb_tracelog_chunk_t* chunk, size_t size)
{
    size_t space_left;
    void* ret;

    assert(!chunk->next);
    assert(chunk->capacity >= chunk->len);
    space_left = chunk->capacity - chunk->len;
    if (space_left < size) {
        rb_tracelog_chunk_t *new_chunk = chunk_new();
        chunk->next = new_chunk;
        g_tracelog->tail_chunk = new_chunk;
        return chunk_alloc_mem(new_chunk, size);
    }

    chunk->len += size;
    ret = chunk->data_head;
    chunk->data_head = (void*)((uintptr_t)chunk->data_head + size);
    return ret;
}

static void*
tracelog_alloc_mem(size_t size)
{
    return chunk_alloc_mem(g_tracelog->tail_chunk, size);
}

static rb_tracelog_event_t*
tracelog_event_new()
{
    rb_tracelog_event_t* event = tracelog_alloc_mem(sizeof(rb_tracelog_event_t));
    event->next = NULL;
    g_tracelog->tail_event->next = event;
    g_tracelog->tail_event = event;
    return event;
}

static rb_tracelog_event_t*
tracelog_event_new_from_literal(const char* name, const char* category, char phase)
{
    rb_tracelog_event_t* event;
    event = tracelog_event_new();
    event->name = name;
    event->category = category;
    event->args = NULL;
    event->phase = phase;
    event->timestamp = clock_abs_ns();
    return event;
}

/* name, category must be string literal and not dynamically allocated. */
void
rb_tracelog_event_new_from_literal(const char* name, const char* category, char phase)
{
    if (!g_tracelog) return;

    tracelog_event_new_from_literal(name, category, phase);
}

typedef struct rb_tracelog_arg_string_literal_struct {
    const char* key;
    const char* val;
} rb_tracelog_arg_string_literal_t;

static VALUE
arg_string_literal_serialize(void* data)
{
    rb_tracelog_arg_string_literal_t *arg = (rb_tracelog_arg_string_literal_t*)data;
    VALUE key = rb_str_new_cstr(arg->key);
    VALUE val = rb_str_new_cstr(arg->val);

    VALUE args = rb_hash_new();
    rb_hash_aset(args, key, val);
    return args;
}

void
rb_tracelog_event_new2(const char* name, const char* category, const char* argname, const char* argdata, char phase)
{
    rb_tracelog_arg_string_literal_t *data;
    rb_tracelog_event_t *event;

    if (!g_tracelog) return;

    data = tracelog_alloc_mem(sizeof(struct rb_tracelog_arg_string_literal_struct));
    data->key = argname;
    data->val = argdata;

    event = tracelog_event_new_from_literal(name, category, phase);
    event->args = tracelog_alloc_mem(sizeof(struct rb_tracelog_arg_struct));
    event->args->serialize_cb = arg_string_literal_serialize;
    event->args->data = data;
}

static void
tracelog_init(void)
{
    rb_tracelog_event_t* event;

    g_tracelog = ALLOC(rb_tracelog_t);
    g_tracelog->head_chunk = chunk_new();
    g_tracelog->tail_chunk = g_tracelog->head_chunk;

    event = tracelog_alloc_mem(sizeof(rb_tracelog_event_t));
    event->name = "TracingStartSentinel";
    event->category = "TraceLog";
    event->args = NULL;
    event->phase = 'I';
    event->timestamp = clock_abs_ns();
    event->next = NULL;

    g_tracelog->head_event = event;
    g_tracelog->tail_event = event;
}

static VALUE rb_cTraceLog;

static VALUE
tracelog_to_a()
{
    VALUE ret = rb_ary_new();

    rb_tracelog_event_t* event;
    for (event = g_tracelog->head_event; event; event = event->next) {
        VALUE rb_name = rb_str_new_cstr(event->name);
        VALUE rb_category = rb_str_new_cstr(event->category);
        VALUE rb_args = event->args ? event->args->serialize_cb(event->args->data) : Qnil;
        VALUE rb_phase = rb_str_new(&event->phase, 1);
        VALUE rb_timestamp = ULL2NUM(event->timestamp);

        VALUE rb_event = rb_ary_new_from_args(5, rb_name, rb_category, rb_args, rb_phase, rb_timestamp);
        rb_ary_push(ret, rb_event);
    }

    return ret;
}

void
Init_tracelog(void)
{
    tracelog_init();

    rb_cTraceLog = rb_define_class("TraceLog", rb_cObject);
    rb_define_singleton_method(rb_cTraceLog, "to_a", tracelog_to_a, 0);

    /*
    rb_add_event_hook(tracelog_event_hook, RUBY_EVENT_ALL, 
    */
}
