#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include "b64_cencode.h"

#define ASCIILOG_C
#include "asciilog.h"

#define ERR(fmt, ...) do {                                              \
  fprintf(stderr, "FATAL ERROR! %s: %s(): " fmt "\n", __FILE__, __func__, ## __VA_ARGS__); \
  exit(1);                                                              \
} while(0)

void _asciilog_init_session_ctx( struct asciilog_context_t* ctx,
                                 int Nfields);

// ASCIILOG_N_FIELDS is unknown here so the asciilog_context_t structure has 0
// elements. I dynamically allocate it later with the proper size
static struct asciilog_context_t* get_global_context(int Nfields)
{
    static struct asciilog_context_t* ctx;

    if(!ctx)
    {
        if(Nfields < 0)
            ERR("Creating a global context not at the start");
        ctx = malloc(sizeof(struct asciilog_context_t) +
                     Nfields * sizeof(ctx->fields[0]));
        if(!ctx) ERR("Couldn't allocate context with %d fields", Nfields);

        _asciilog_init_session_ctx(ctx, Nfields);
    }
    return ctx;
}



static void check_fp(struct asciilog_context_t* ctx)
{
    if(!ctx->root->_fp)
        asciilog_set_output_FILE(ctx, stdout);
}
static void _emit(struct asciilog_context_t* ctx, const char* string)
{
    fprintf(ctx->root->_fp, "%s", string);
    ctx->root->_emitted_something = true;
}

static void _emit_field(struct asciilog_context_t* ctx, int i)
{
    if( ctx->fields[i].binptr == NULL )
        // plain ascii field
        _emit(ctx, ctx->fields[i].c);
    else
    {
        // binary field. Encode with base64 first
        char out_base64[base64_dstlen_to_encode(ctx->fields[i].binlen)];
        base64_encode( out_base64, sizeof(out_base64),
                       ctx->fields[i].binptr, ctx->fields[i].binlen );
        _emit(ctx, out_base64);
    }
}

static void emit(struct asciilog_context_t* ctx, const char* string)
{
    check_fp(ctx);
    _emit(ctx, string);
}

void _asciilog_printf(struct asciilog_context_t* ctx, int Nfields, const char* fmt, ...)
{
    if( ctx == NULL ) ctx = get_global_context(Nfields);
    check_fp(ctx);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(ctx->root->_fp, fmt, ap);
    va_end(ap);

    ctx->root->_emitted_something = true;
}

void _asciilog_flush(struct asciilog_context_t* ctx, int Nfields)
{
    if( ctx == NULL ) ctx = get_global_context(Nfields);
    check_fp(ctx);
    fflush(ctx->root->_fp);
}

void asciilog_set_output_FILE(struct asciilog_context_t* ctx, FILE* fp)
{
    if(ctx->root->_fp)
        ERR("fp is already set");
    if( ctx->root->_emitted_something )
        ERR("Can only change the output at the start");

    ctx->root->_fp = fp;
}

static void flush(struct asciilog_context_t* ctx)
{
    fflush(ctx->root->_fp);
}

void _asciilog_clear_fields_ctx(struct asciilog_context_t* ctx, int Nfields, bool do_free_binary)
{
    ctx->line_has_any_values = false;
    for(int i=0; i<Nfields; i++)
    {
        ctx->fields[i].c[0]   = '-';
        ctx->fields[i].c[1]   = '\0';

        // Here the idea is that once I emit the field I deallocate. This is
        // inefficient, but allows some things to be simple. For instance I can
        // use NULL to recognize empty binary fields. The vast majority of these
        // data files will have no binary fields, and this simplicity is good
        if(do_free_binary)
            free(ctx->fields[i].binptr);
        ctx->fields[i].binptr = NULL;
    }
}

void _asciilog_init_session_ctx( struct asciilog_context_t* ctx, int Nfields)
{
    if( ctx == NULL )
        ERR("Can't init a NULL context");

    // zero out the context, and set its root to point to itself
    *ctx = (struct asciilog_context_t){ .root = ctx };

    _asciilog_clear_fields_ctx( ctx, Nfields, false );
}

void _asciilog_init_child_ctx(      struct asciilog_context_t* ctx,
                              const struct asciilog_context_t* ctx_src,
                              int Nfields)
{
    if( ctx     == NULL ) ERR("Can't init a NULL context");
    if( ctx_src == NULL ) ctx_src = get_global_context(Nfields);

    if( !ctx_src->root->_legend_finished )
        ERR("Cannot create children contexts before writing a legend. Hard to keep state consistent otherwise.");


    // Copy all the context except for the flexible array at the end. The root
    // context pointer is copied as well, which is the desired behavior: this
    // child has the same root node as the parent
    *ctx = *ctx_src;

    // reset the flexible array
    _asciilog_clear_fields_ctx( ctx, Nfields, false );
}

void _asciilog_free_ctx( struct asciilog_context_t* ctx, int Nfields )
{
    for(int i=0; i<Nfields; i++)
    {
        free(ctx->fields[i].binptr);
        ctx->fields[i].binptr = NULL;
    }
}

void _asciilog_emit_legend(struct asciilog_context_t* ctx, const char* legend, int Nfields)
{
    if( ctx == NULL ) ctx = get_global_context(Nfields);

    if( ctx->root->_legend_finished )
        ERR("already have a legend");
    ctx->root->_legend_finished = true;

    emit(ctx, legend);
    flush(ctx);
}

static bool is_field_null(const asciilog_field_t* field)
{
    return field->binptr == NULL && field->c[0] == '-' && field->c[1] == '\0';
}

static struct asciilog_context_t*
set_field_prelude(struct asciilog_context_t* ctx,
                  const char* fieldname, int idx)
{
    if( ctx == NULL ) ctx = get_global_context(-1);

    if(!ctx->root->_legend_finished)
        ERR("need a legend to do this");
    if(!is_field_null(&ctx->fields[idx]))
        ERR("Field '%s' already set. Old value: '%s'",
            fieldname, ctx->fields[idx].c);
    ctx->line_has_any_values = true;

    return ctx;
}

// printf() is type agnostic as far as the ABI is concerned, so I pass it the
// correct raw bits without letting C know of the details: all possible integer
// types are passed in via union asciilog_context_t. Past that the code path is
// the same regardless of type. The guts of printf() reinterprets the bits based
// on the format string. Floating-point types are handled differently by the
// ABI, so I do handle those specially
void
_asciilog_set_field_value_int(struct asciilog_context_t* ctx,
                              const char* fieldname, int idx,
                              const char* fmt, union asciilog_field_types_t arg)
{
    ctx = set_field_prelude(ctx, fieldname, idx);
    if( (int)sizeof(ctx->fields[0].c) <=
        snprintf(ctx->fields[idx].c, sizeof(ctx->fields[0].c), fmt, arg) )
    {
        ERR("Field size exceeded for field '%s'", fieldname);
    }
}
void
_asciilog_set_field_value_double(struct asciilog_context_t* ctx,
                                 const char* fieldname, int idx,
                                 const char* fmt, double arg)
{
    ctx = set_field_prelude(ctx, fieldname, idx);
    if( (int)sizeof(ctx->fields[0].c) <=
        snprintf(ctx->fields[idx].c, sizeof(ctx->fields[0].c), fmt, arg) )
    {
        ERR("Field size exceeded for field '%s'", fieldname);
    }
}

void
_asciilog_set_field_value_binary(struct asciilog_context_t* ctx,
                                 const char* fieldname __attribute__((unused)), int idx,
                                 const void* data, int len)
{
    ctx = set_field_prelude(ctx, fieldname, idx);

    // Here I allocate the memory for the data, and copy into it. The idea is
    // that once I emit the field I deallocate. This is inefficient, but allows
    // some things to be simple. For instance I can use NULL to recognize empty
    // binary fields. The vast majority of these data files will have no binary
    // fields, and this simplicity is good
    ctx->fields[idx].binlen = len;
    ctx->fields[idx].binptr = realloc(ctx->fields[idx].binptr, len);
    memcpy(ctx->fields[idx].binptr, data, len);
}

void _asciilog_emit_record(struct asciilog_context_t* ctx, int Nfields)
{
    if( ctx == NULL ) ctx = get_global_context(-1);

    if(!ctx->root->_legend_finished)
        ERR("need a legend to do this");

    if(!ctx->line_has_any_values)
        ERR("Tried to emit a log line without any values being set");

    check_fp(ctx);

    flockfile(ctx->root->_fp);
    {
        for(int i=0; i<Nfields-1; i++)
        {
            _emit_field(ctx, i);
            _emit(ctx, " ");
        }
        _emit_field(ctx, Nfields-1);
        _emit(ctx, "\n");
    }
    funlockfile(ctx->root->_fp);

    // I want to be able to process streaming data, so I flush the buffer now
    flush(ctx);

    _asciilog_clear_fields_ctx(ctx, Nfields, true);
}
