#include "argparse/libargparse.h"

#define ARGPARSE_MSG_INVALID "invalid option"
#define ARGPARSE_MSG_MISSING "option requires an argument"
#define ARGPARSE_MSG_TOOMANY "option takes no arguments"

static int argparse_error(argparse_state_t *options, const char *msg, const char *data)
{
    unsigned p = 0;
    const char *sep = " -- '";
    while (*msg)
        options->errmsg[p++] = *msg++;
    while (*sep)
        options->errmsg[p++] = *sep++;
    while (p < sizeof(options->errmsg) - 2 && *data)
        options->errmsg[p++] = *data++;
    options->errmsg[p++] = '\'';
    options->errmsg[p++] = '\0';
    return '?';
}

void argparse_init(argparse_state_t *options, const char **argv)
{
    options->argv = argv;
    options->permute = 1;
    options->optind = argv[0] != 0;
    options->subopt = 0;
    options->optarg = 0;
    options->errmsg[0] = '\0';
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

static void argparse_permute(argparse_state_t *options, int index)
{
    const char *nonoption = options->argv[index];
    int i;
    for (i = index; i < options->optind - 1; i++)
        options->argv[i] = options->argv[i + 1];
    options->argv[options->optind - 1] = nonoption;
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

int argparse(argparse_state_t *options, const char *optstring)
{
    int type;
    const char *next;
    const char *option = options->argv[options->optind];
    options->errmsg[0] = '\0';
    options->optopt = 0;
    options->optarg = 0;
    if (option == 0)
    {
        return -1;
    }
    else if (argparse_is_dashdash(option))
    {
        options->optind++; /* consume "--" */
        return -1;
    }
    else if (!argparse_is_shortopt(option))
    {
        if (options->permute)
        {
            int index = options->optind++;
            int r = argparse(options, optstring);
            argparse_permute(options, index);
            options->optind--;
            return r;
        }
        else
        {
            return -1;
        }
    }
    option += options->subopt + 1;
    options->optopt = option[0];
    type = argparse_get_argtype(optstring, option[0]);
    next = options->argv[options->optind + 1];
    switch (type)
    {
        case -1:
        {
            char str[2] = { 0, 0 };
            str[0] = option[0];
            options->optind++;
            return argparse_error(options, ARGPARSE_MSG_INVALID, str);
        }
        case ARGPARSE_NONE:
            if (option[1])
            {
                options->subopt++;
            }
            else
            {
                options->subopt = 0;
                options->optind++;
            }
            return option[0];
        case ARGPARSE_REQUIRED:
            options->subopt = 0;
            options->optind++;
            if (option[1])
            {
                options->optarg = option + 1;
            }
            else if (next != 0)
            {
                options->optarg = next;
                options->optind++;
            }
            else
            {
                char str[2] = { 0, 0 };
                str[0] = option[0];
                options->optarg = 0;
                return argparse_error(options, ARGPARSE_MSG_MISSING, str);
            }
            return option[0];
        case ARGPARSE_OPTIONAL:
            options->subopt = 0;
            options->optind++;
            if (option[1])
                options->optarg = option + 1;
            else
                options->optarg = 0;
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
