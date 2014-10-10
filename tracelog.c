/**********************************************************************

  tracelog.c -

  $Author: nyaxt $
  created at: Tue Aug 14 19:37:09 2012

  Copyright (C) 1993-2014 Yukihiro Matsumoto

**********************************************************************/

#include "ruby/ruby.h"
#include "ruby/debug.h"

#include <assert.h>

typedef struct rb_tracelog_event_struct {
    const char* name;
    const char* category;
    char phase;
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

static rb_tracelog_t* g_tracelog; // FIXME: this should be thread local

static rb_tracelog_chunk_t*
chunk_new(void)
{
    rb_tracelog_chunk_t *chunk = ALLOC(rb_tracelog_chunk_t);
    chunk->data_head = xmalloc(TRACELOG_CHUNK_SIZE);
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
    assert(chunk->capacity > chunk->len);
    space_left = chunk->capacity - chunk->len;
    if (space_left < size) {
        chunk->next = chunk_new();
        return chunk_alloc_mem(chunk->next, size);
    }

    chunk->capacity += size;
    ret = (void*)((uintptr_t)chunk->data_head + size);
    chunk->data_head = (void*)ret;
    return ret;
}

/* name, category must be string literal and not dynamically allocated. */
void
tracelog_event_new_from_literal(const char* name, const char* category, char phase)
{
    rb_tracelog_chunk_t* chunk = g_tracelog->tail_chunk; 

    rb_tracelog_event_t* event = chunk_alloc_mem(chunk, sizeof(rb_tracelog_event_t));
    event->name = name;
    event->category = category;
    event->phase = phase;
    event->next = NULL;

    g_tracelog->tail_event->next = event;
}

static void
tracelog_init(void)
{
    rb_tracelog_event_t* event;

    g_tracelog = ALLOC(rb_tracelog_t);
    g_tracelog->head_chunk = chunk_new();
    g_tracelog->tail_chunk = g_tracelog->head_chunk;

    event = chunk_alloc_mem(g_tracelog->tail_chunk, sizeof(rb_tracelog_event_t));
    event->name = "TracingStartSentinel";
    event->category = "TraceLog";
    event->phase = 'I';
    event->next = NULL;

    g_tracelog->head_event = event;
    g_tracelog->tail_event = event;
}

static VALUE rb_cTraceLog;

static VALUE
tracelog_to_a()
{
    VALUE ret = rb_ary_new();

    rb_tracelog_event_t* ev;
    for (ev = g_tracelog->head_event; ev; ev = ev->next) {
        printf("name: %s, cat: %s, ph: %c\n", ev->name, ev->category, ev->phase);
    }

    return ret;
}

void
Init_tracelog(void)
{
    rb_cTraceLog = rb_define_class("TraceLog", rb_cObject);
    rb_define_singleton_method(rb_cTraceLog, "to_a", tracelog_to_a, 0);

    tracelog_init();
}
