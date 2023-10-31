#include "argparse/libargparse.h"

#if __MOS_MINIMAL_LIBC__
#include <mos_stdio.h>
#else
#include <stdio.h>
#endif

#define ARGPARSE_MSG_INVALID "invalid option"
#define ARGPARSE_MSG_MISSING "option requires an argument"
#define ARGPARSE_MSG_TOOMANY "option takes no arguments"

static int argparse_error(argparse_state_t *state, const char *msg, const char *data)
{
    unsigned p = 0;
    const char *sep = " -- '";
    while (*msg)
        state->errmsg[p++] = *msg++;
    while (*sep)
        state->errmsg[p++] = *sep++;
    while (p < sizeof(state->errmsg) - 2 && *data)
        state->errmsg[p++] = *data++;
    state->errmsg[p++] = '\'';
    state->errmsg[p++] = '\0';
    return '?';
}

void argparse_init(argparse_state_t *state, const char *argv[])
{
    state->argv = argv;
    state->permute = 1;
    state->optind = argv[0] != 0;
    state->subopt = 0;
    state->optarg = 0;
    state->errmsg[0] = '\0';
}

static int argparse_is_dashdash(const char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] == '-' && arg[2] == '\0';
}

static int argparse_is_shortopt(const char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] != '-' && arg[1] != '\0';
}

static int argparse_is_longopt(const char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] == '-' && arg[2] != '\0';
}

static void argparse_permute(argparse_state_t *state, int index)
{
    const char *nonoption = state->argv[index];
    int i;
    for (i = index; i < state->optind - 1; i++)
        state->argv[i] = state->argv[i + 1];
    state->argv[state->optind - 1] = nonoption;
}

static int argparse_get_argtype(const char *optstring, char c)
{
    int count = ARGPARSE_NONE;
    if (c == ':')
        return -1;
    for (; *optstring && c != *optstring; optstring++)
        ;
    if (!*optstring)
        return -1;
    if (optstring[1] == ':')
        count += optstring[2] == ':' ? 2 : 1;
    return count;
}

int argparse(argparse_state_t *state, const char *optstring)
{
    int type;
    const char *next;
    const char *option = state->argv[state->optind];
    state->errmsg[0] = '\0';
    state->optopt = 0;
    state->optarg = 0;
    if (option == 0)
    {
        return -1;
    }
    else if (argparse_is_dashdash(option))
    {
        state->optind++; /* consume "--" */
        return -1;
    }
    else if (!argparse_is_shortopt(option))
    {
        if (state->permute)
        {
            int index = state->optind++;
            int r = argparse(state, optstring);
            argparse_permute(state, index);
            state->optind--;
            return r;
        }
        else
        {
            return -1;
        }
    }
    option += state->subopt + 1;
    state->optopt = option[0];
    type = argparse_get_argtype(optstring, option[0]);
    next = state->argv[state->optind + 1];
    switch (type)
    {
        case -1:
        {
            char str[2] = { 0, 0 };
            str[0] = option[0];
            state->optind++;
            return argparse_error(state, ARGPARSE_MSG_INVALID, str);
        }
        case ARGPARSE_NONE:
            if (option[1])
            {
                state->subopt++;
            }
            else
            {
                state->subopt = 0;
                state->optind++;
            }
            return option[0];
        case ARGPARSE_REQUIRED:
            state->subopt = 0;
            state->optind++;
            if (option[1])
            {
                state->optarg = option + 1;
            }
            else if (next != 0)
            {
                state->optarg = next;
                state->optind++;
            }
            else
            {
                char str[2] = { 0, 0 };
                str[0] = option[0];
                state->optarg = 0;
                return argparse_error(state, ARGPARSE_MSG_MISSING, str);
            }
            return option[0];
        case ARGPARSE_OPTIONAL:
            state->subopt = 0;
            state->optind++;
            if (option[1])
                state->optarg = option + 1;
            else
                state->optarg = 0;
            return option[0];
    }
    return 0;
}

const char *argparse_arg(argparse_state_t *options)
{
    const char *option = options->argv[options->optind];
    options->subopt = 0;
    if (option != 0)
        options->optind++;
    return option;
}

static int argparse_longopts_end(const argparse_arg_t *longopts, int i)
{
    return !longopts[i].full && !longopts[i].abbr;
}

static void argparse_from_long(const argparse_arg_t *longopts, char *optstring)
{
    char *p = optstring;
    int i;
    for (i = 0; !argparse_longopts_end(longopts, i); i++)
    {
        if (longopts[i].abbr && longopts[i].abbr < 127)
        {
            int a;
            *p++ = longopts[i].abbr;
            for (a = 0; a < (int) longopts[i].argtype; a++)
                *p++ = ':';
        }
    }
    *p = '\0';
}

/* Unlike strcmp(), handles options containing "=". */
static int argparse_longopts_match(const char *longname, const char *option)
{
    const char *a = option, *n = longname;
    if (longname == 0)
        return 0;
    for (; *a && *n && *a != '='; a++, n++)
        if (*a != *n)
            return 0;
    return *n == '\0' && (*a == '\0' || *a == '=');
}

/* Return the part after "=", or NULL. */
static const char *argparse_longopts_arg(const char *option)
{
    for (; *option && *option != '='; option++)
        ;
    if (*option == '=')
        return option + 1;
    else
        return 0;
}

static int argparse_long_fallback(argparse_state_t *options, const argparse_arg_t *longopts, int *longindex)
{
    int result;
    char optstring[96 * 3 + 1]; /* 96 ASCII printable characters */
    argparse_from_long(longopts, optstring);
    result = argparse(options, optstring);
    if (longindex != 0)
    {
        *longindex = -1;
        if (result != -1)
        {
            int i;
            for (i = 0; !argparse_longopts_end(longopts, i); i++)
                if (longopts[i].abbr == options->optopt)
                    *longindex = i;
        }
    }
    return result;
}

int argparse_long(argparse_state_t *options, const argparse_arg_t *longopts, int *longindex)
{
    int i;
    const char *option = options->argv[options->optind];
    if (option == 0)
    {
        return -1;
    }
    else if (argparse_is_dashdash(option))
    {
        options->optind++; /* consume "--" */
        return -1;
    }
    else if (argparse_is_shortopt(option))
    {
        return argparse_long_fallback(options, longopts, longindex);
    }
    else if (!argparse_is_longopt(option))
    {
        if (options->permute)
        {
            int index = options->optind++;
            int r = argparse_long(options, longopts, longindex);
            argparse_permute(options, index);
            options->optind--;
            return r;
        }
        else
        {
            return -1;
        }
    }

    /* Parse as long option. */
    options->errmsg[0] = '\0';
    options->optopt = 0;
    options->optarg = 0;
    option += 2; /* skip "--" */
    options->optind++;
    for (i = 0; !argparse_longopts_end(longopts, i); i++)
    {
        const char *name = longopts[i].full;
        if (argparse_longopts_match(name, option))
        {
            const char *arg;
            if (longindex)
                *longindex = i;
            options->optopt = longopts[i].abbr;
            arg = argparse_longopts_arg(option);
            if (longopts[i].argtype == ARGPARSE_NONE && arg != 0)
            {
                return argparse_error(options, ARGPARSE_MSG_TOOMANY, name);
            }
            if (arg != 0)
            {
                options->optarg = arg;
            }
            else if (longopts[i].argtype == ARGPARSE_REQUIRED)
            {
                options->optarg = options->argv[options->optind];
                if (options->optarg == 0)
                    return argparse_error(options, ARGPARSE_MSG_MISSING, name);
                else
                    options->optind++;
            }
            return options->optopt;
        }
    }
    return argparse_error(options, ARGPARSE_MSG_INVALID, option);
}

void argparse_usage(argparse_state_t *options, const argparse_arg_t *args, const char *usage)
{
    fprintf(stderr, "Usage: %s, %s\n", options->argv[0], usage);
    static const int help_indent = 24;

    for (int i = 0; !argparse_longopts_end(args, i); i++)
    {
        const char *name = args[i].full;
        const char abbr = args[i].abbr;
        const char *help = args[i].help ? args[i].help : "";
        const char *arg = args[i].argtype == ARGPARSE_NONE ? "" : args[i].argtype == ARGPARSE_REQUIRED ? " ARG" : " [ARG]";

        int printed = 0;
        if (abbr)
            printed += fprintf(stderr, "  -%c", abbr);

        if (name)
            printed += fprintf(stderr, ", --%s%s", name, arg);
        else
            printed++;

        if (printed < help_indent)
            fprintf(stderr, "%*s", help_indent - printed, "");
        else
            fprintf(stderr, "\n%*s", help_indent, "");

        fprintf(stderr, "%s\n", help);
    }

    fprintf(stderr, "\n");
}
