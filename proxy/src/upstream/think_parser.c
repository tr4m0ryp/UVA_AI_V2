#include "upstream/think_parser.h"
#include <stdlib.h>
#include <string.h>

#define OPEN_TAG   "<think>"
#define CLOSE_TAG  "</think>"
#define OPEN_LEN   7
#define CLOSE_LEN  8
#define MAX_TAG    8   /* max(OPEN_LEN, CLOSE_LEN) */
#define OUT_CAP    4096

typedef enum {
    TP_CONTENT,
    TP_MAYBE_OPEN,
    TP_REASONING,
    TP_MAYBE_CLOSE
} tp_state_t;

struct think_parser {
    think_emit_fn  emit;
    void          *userdata;
    tp_state_t     state;
    char           tag[MAX_TAG + 1];
    int            tag_len;
    char           out[OUT_CAP];
    int            out_len;
    int            out_is_reasoning;
};

static void flush_out(think_parser_t *tp)
{
    if (tp->out_len > 0) {
        tp->out[tp->out_len] = '\0';
        tp->emit(tp->out, tp->out_is_reasoning, tp->userdata);
        tp->out_len = 0;
    }
}

static void push_char(think_parser_t *tp, char c, int is_reasoning)
{
    if (tp->out_len > 0 && tp->out_is_reasoning != is_reasoning)
        flush_out(tp);
    tp->out_is_reasoning = is_reasoning;
    tp->out[tp->out_len++] = c;
    if (tp->out_len >= OUT_CAP - 1)
        flush_out(tp);
}

static void push_str(think_parser_t *tp, const char *s, int len,
                     int is_reasoning)
{
    for (int i = 0; i < len; i++)
        push_char(tp, s[i], is_reasoning);
}

think_parser_t *think_parser_new(think_emit_fn emit, void *userdata)
{
    think_parser_t *tp = calloc(1, sizeof(*tp));
    if (!tp) return NULL;
    tp->emit = emit;
    tp->userdata = userdata;
    tp->state = TP_CONTENT;
    return tp;
}

/* Handle mismatch in TP_MAYBE_OPEN: emit buffered, restart if last is '<' */
static void mismatch_open(think_parser_t *tp)
{
    char last = tp->tag[tp->tag_len - 1];
    if (last == '<') {
        push_str(tp, tp->tag, tp->tag_len - 1, 0);
        tp->tag[0] = '<';
        tp->tag_len = 1;
    } else {
        push_str(tp, tp->tag, tp->tag_len, 0);
        tp->tag_len = 0;
        tp->state = TP_CONTENT;
    }
}

/* Handle mismatch in TP_MAYBE_CLOSE: emit buffered, restart if last is '<' */
static void mismatch_close(think_parser_t *tp)
{
    char last = tp->tag[tp->tag_len - 1];
    if (last == '<') {
        push_str(tp, tp->tag, tp->tag_len - 1, 1);
        tp->tag[0] = '<';
        tp->tag_len = 1;
    } else {
        push_str(tp, tp->tag, tp->tag_len, 1);
        tp->tag_len = 0;
        tp->state = TP_REASONING;
    }
}

void think_parser_feed(think_parser_t *tp, const char *text)
{
    if (!tp || !text) return;

    for (const char *p = text; *p; p++) {
        char c = *p;

        switch (tp->state) {
        case TP_CONTENT:
            if (c == '<') {
                tp->tag[0] = '<';
                tp->tag_len = 1;
                tp->state = TP_MAYBE_OPEN;
            } else {
                push_char(tp, c, 0);
            }
            break;

        case TP_MAYBE_OPEN:
            tp->tag[tp->tag_len++] = c;
            if (tp->tag_len == OPEN_LEN) {
                tp->tag[tp->tag_len] = '\0';
                if (strcmp(tp->tag, OPEN_TAG) == 0) {
                    flush_out(tp);
                    tp->state = TP_REASONING;
                    tp->tag_len = 0;
                } else {
                    mismatch_open(tp);
                }
            } else {
                tp->tag[tp->tag_len] = '\0';
                if (strncmp(tp->tag, OPEN_TAG, tp->tag_len) != 0)
                    mismatch_open(tp);
            }
            break;

        case TP_REASONING:
            if (c == '<') {
                tp->tag[0] = '<';
                tp->tag_len = 1;
                tp->state = TP_MAYBE_CLOSE;
            } else {
                push_char(tp, c, 1);
            }
            break;

        case TP_MAYBE_CLOSE:
            tp->tag[tp->tag_len++] = c;
            if (tp->tag_len == CLOSE_LEN) {
                tp->tag[tp->tag_len] = '\0';
                if (strcmp(tp->tag, CLOSE_TAG) == 0) {
                    flush_out(tp);
                    tp->state = TP_CONTENT;
                    tp->tag_len = 0;
                } else {
                    mismatch_close(tp);
                }
            } else {
                tp->tag[tp->tag_len] = '\0';
                if (strncmp(tp->tag, CLOSE_TAG, tp->tag_len) != 0)
                    mismatch_close(tp);
            }
            break;
        }
    }
}

void think_parser_finish(think_parser_t *tp)
{
    if (!tp) return;
    if (tp->tag_len > 0) {
        int r = (tp->state == TP_MAYBE_CLOSE ||
                 tp->state == TP_REASONING) ? 1 : 0;
        push_str(tp, tp->tag, tp->tag_len, r);
        tp->tag_len = 0;
    }
    flush_out(tp);
}

void think_parser_free(think_parser_t *tp)
{
    free(tp);
}

char *think_strip(const char *text, char **reasoning_out)
{
    if (!text) return strdup("");

    size_t len = strlen(text);
    char *content = malloc(len + 1);
    char *reasoning = reasoning_out ? malloc(len + 1) : NULL;
    if (!content) return NULL;

    size_t ci = 0, ri = 0;
    int in_think = 0;
    const char *p = text;

    while (*p) {
        if (!in_think && strncmp(p, OPEN_TAG, OPEN_LEN) == 0) {
            in_think = 1;
            p += OPEN_LEN;
            continue;
        }
        if (in_think && strncmp(p, CLOSE_TAG, CLOSE_LEN) == 0) {
            in_think = 0;
            p += CLOSE_LEN;
            continue;
        }
        if (in_think) {
            if (reasoning) reasoning[ri++] = *p;
        } else {
            content[ci++] = *p;
        }
        p++;
    }

    content[ci] = '\0';
    if (reasoning) {
        reasoning[ri] = '\0';
        *reasoning_out = reasoning;
    }
    return content;
}
