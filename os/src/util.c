#include "os/util.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>

#define LEN 32

#define SIGNED_BASE 10

static char *end = "", *neg = "-";

int pandos_pow(int base, unsigned int exp)
{
    int tmp;
    if (exp == 0)
        return 1;

    tmp = pandos_pow(base, exp / 2);
    return (exp % 2 == 0 ? 1 : base) * tmp * tmp;
}

size_t __itoa(void *target, size_t (*writer)(void *, const char *, size_t len),
              int i, int base)
{
    int cpy, digits, wrote, exp;
    unsigned unsigned_i;

    wrote = 0;

    if (base == SIGNED_BASE && i < 0) {
        /* Ignore an error here as it'll crop up later either way */
        wrote += writer(target, neg, 1);
        i = -i;
    }

    for (cpy = i, digits = 0; cpy != 0; ++digits, cpy /= base)
        ;
    if (i == 0)
        ++digits;

    int wr = 1;
    if (i)
        for (unsigned_i = i, exp = pandos_pow(base, digits - 1); digits && wr;
             --digits, unsigned_i %= exp, exp /= base, wrote += wr) {
            int r = (unsigned_i / exp); /* remainder */
            char digit[2] = {r > 9 ? 'a' + r - 10 : '0' + r, '\0'};
            wr = writer(target, digit, 1);
        }
    else
        wrote += writer(target, "0", 1);

    /* always write the string termination char (but don't count it as string
     * length) */
    return wrote + writer(target, end, 1);
}

size_t __bin(void *target, size_t (*writer)(void *, const char *, size_t len),
             int i)
{
    char buffer[LEN + 1];
    for (int p = 0; p < LEN; ++p)
        buffer[p] = i >> (LEN - p - 1) & ~(~0 << 1) ? '1' : '0';
    buffer[LEN] = '\0';
    writer(target, buffer, LEN);
    return LEN;
}

size_t __pandos_printf(void *target,
                       size_t (*writer)(void *, const char *, size_t len),
                       const char *fmt, va_list varg)
{
    size_t wr, last_wrote;

    for (wr = 0; *fmt != '\0'; ++fmt, wr += last_wrote) {
        if (*fmt == '%') {
            ++fmt;
            switch (*fmt) {
                case 's':
                    last_wrote = writer(target, va_arg(varg, char *), 0);
                    break;
                case 'c': {
                    char str[2] = {va_arg(varg, int), '\0'};
                    last_wrote = writer(target, str, 1);
                    break;
                }
                case 'd':
                    last_wrote = __itoa(target, writer, va_arg(varg, int), 10);
                    break;
                case 'p': {
                    memaddr ptr = va_arg(varg, memaddr);
                    if (ptr != (memaddr)NULL) {
                        last_wrote = writer(target, "0x", 2);
                        last_wrote += __itoa(target, writer, (int)ptr, 16);
                    } else
                        last_wrote = writer(target, "(nil)", 5);
                    break;
                }
                case 'b':
                    last_wrote = __bin(target, writer, va_arg(varg, int));
                    break;
                case '%': {
                    char *str = "%";
                    last_wrote = writer(target, str, 1);
                    break;
                }
                default: {
                    char str[3] = {*(fmt - 1), *fmt, '\0'};
                    last_wrote = writer(target, str, 2);
                    break;
                }
            }
        } else {
            char str[2] = {*fmt, '\0'};
            last_wrote = writer(target, str, 1);
        }
        if (!last_wrote)
            break;
    }
    /* if we stopped printing because of a writer error, make sure to print the
     * ending null character */
    if (*fmt != '\0')
        wr += writer(target, end, 1);
    return wr;
}

size_t str_writer(void *dest, const char *data, size_t len)
{
    str_target_t *d;
    int i;
    bool check_len;

    d = (str_target_t *)dest;
    i = 0;
    check_len = len != 0;
    /* Make sure we always write the NULL char (in the appropriate location) */
    if (*data == '\0')
        *(d->str + (d->wrote >= d->size - 1 ? d->wrote : d->wrote + 1)) = '\0';
    else
        while (d->wrote + i < d->size - 1 &&
               (*(d->str + d->wrote + i) = data[i]) != '\0' &&
               (!check_len || len--))
            ++i;

    d->wrote += i;
    return i;
}

size_t nitoa(int i, int base, char *dest, size_t len)
{
    str_target_t w = {dest, len, 0};
    return __itoa((void *)&w, str_writer, i, base);
}

size_t pandos_snprintf(char *dest, size_t len, const char *fmt, ...)
{
    str_target_t w = {dest, len, 0};
    va_list varg;
    va_start(varg, fmt);
    size_t res = __pandos_printf((void *)&w, str_writer, fmt, varg);
    va_end(varg);
    return res;
}

#ifndef __x86_64__
int term_putchar(termreg_t *term, char c)
{
    devreg stat;

    stat = term_status(term);
    if (stat != ST_READY && stat != ST_TRANSMITTED)
        return 1;

    term->transm_command = ((c << CHAR_OFFSET) | CMD_TRANSMIT);
    while ((stat = term_status(term)) == ST_BUSY)
        ;

    term->transm_command = CMD_ACK;
    return (stat == ST_TRANSMITTED) ? 0 : 2;
}

size_t serial_writer(void *dest, const char *data, size_t len)
{
    char *it;
    termreg_t *term;
    bool check_len;

    for (it = (char *)data, term = (termreg_t *)dest, check_len = len != 0;
         *it != '\0' && (!check_len || len--); ++it)
        if (term_putchar(term, *it))
            break;

    return it - data;
}

/*
 * The following functions have been provided by the tutor (but might have been
 * slightly modified along the way): kputchar, next_line
 */

static inline void clean_line(memory_target_t *mem, size_t line, char c)
{
    for (size_t i = 0; i < MEM_WRITER_LINE_LENGTH; i++)
        mem->buffer[line][i] = c;
}

// Skip to next line
static inline void next_line(memory_target_t *mem)
{
    mem->line = (mem->line + 1) % MEM_WRITER_LINES;
    mem->ch = 0;
    /* Clean out the rest of the line for aesthetic purposes */
    clean_line(mem, mem->line, ' ');
    clean_line(mem, (mem->line + 1) % MEM_WRITER_LINES, '-');
}

static void kputchar(memory_target_t *mem, char ch)
{
    if (ch == '\n')
        next_line(mem);
    else {
        mem->buffer[mem->line][mem->ch++] = ch;
        if (mem->ch >= MEM_WRITER_LINE_LENGTH)
            next_line(mem);
    }
}

size_t memory_writer(void *dest, const char *data, size_t len)
{
    char *it;
    memory_target_t *mem;
    bool check_len;

    /* Clean the first line during the first print */
    mem = (memory_target_t *)dest;
    if (mem->line == 0 && mem->ch == 0) {
        clean_line(mem, mem->line, ' ');
        clean_line(mem, mem->line + 1, ' ');
    }

    for (it = (char *)data, check_len = len != 0;
         *it != '\0' && (!check_len || len--); ++it)
        kputchar(mem, *it);

    return it - data;
}

size_t pandos_fprintf(int fd, const char *fmt, ...)
{
    termreg_t *term = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, fd);
    va_list varg;
    va_start(varg, fmt);
    size_t res = __pandos_printf(term, serial_writer, fmt, varg);
    va_end(varg);
    return res;
}

memory_target_t kstdout = {.line = 0, .ch = 0}, kstderr = {.line = 0, .ch = 0},
                kverb = {.line = 0, .ch = 0}, kdebug = {.line = 0, .ch = 0};

size_t pandos_kfprintf(memory_target_t *mem, const char *fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    size_t res = __pandos_printf(mem, memory_writer, fmt, varg);
    va_end(varg);
    return res;
}

void pandos_kclear(memory_target_t *mem)
{
    for (size_t i = 0; i < MEM_WRITER_LINES; i++)
        for (size_t j = 0; j < MEM_WRITER_LINE_LENGTH; j++)
            mem->buffer[i][j] = ' ';
    mem->line = 0;
    mem->ch = 0;
}
#endif
