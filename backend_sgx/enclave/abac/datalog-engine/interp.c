#include "internal.h"

#include <libnexus_trusted/nexus_util.h>

static char *
my_putc(char * ptr, char c, int * left)
{
    if (*left < 1) {
        return ptr + 1;
    }

    *ptr = c;
    *left--;

    return ptr + 1;
}

static char *
answers_to_string(dl_answers_t a, size_t * buffer_str_len)
{
    char * buffer = NULL;
    char * tmp    = buffer;
    int    buflen = 0;

    int i, j, n;

    *buffer_str_len = 1;

    if (!a) {
        return strndup("", 2);
    }

redo:
    n = dl_getpredarity(a);
    if (n == 0) {
        return strndup("", 2);
    } else {
        for (i = 0; dl_getconst(a, i, 0); i++) {
            tmp = dl_putconst(tmp, dl_getconst(a, i, 0), &buflen);

            for (j = 1; j < n; j++) {
                tmp = my_putc(tmp, '\t', &buflen);
                tmp = dl_putconst(tmp, dl_getconst(a, i, j), &buflen);
            }

            tmp = my_putc(tmp, '\n', &buflen);
        }
    }

    if (buffer == NULL) {
        // let's allocate memory and redo the whole operation
        buflen = (int)(tmp - buffer) + 1;
        buffer = tmp = nexus_malloc(buflen);
        goto redo;
    }

    *buffer_str_len = buflen;

    return buffer;
}

void
loaderror_func(void * data, int lineno, int colno, const char * msg)
{
    log_error("lua: [line=%d, col=%d] %s\n", lineno, colno, msg);
}

int
datalog_evaluate(char * datalog_buffer_IN, char ** string_ans)
{
    dl_db_t      database = dl_open();
    dl_answers_t answers;

    size_t len;

    if (dl_loadbuffer(database, datalog_buffer_IN, strlen(datalog_buffer_IN), loaderror_func)) {
        dl_close(database);
        log_error("could not load buffer\n");
        return -1;
    }

    if (dl_ask(database, &answers)) {
        log_error("could not get answers\n");
        goto err;
    }

    *string_ans = answers_to_string(answers, &len);

    // print the answers
    dl_free(answers);

    dl_close(database);

    return 0;
err:
    dl_close(database);
    return -1;
}
