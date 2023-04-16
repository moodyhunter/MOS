/* Argparse --- portable, reentrant, embeddable, getopt-like option parser
 *
 * This is free and unencumbered software released into the public domain.
 *
 * The POSIX getopt() option parser has three fatal flaws. These flaws
 * are solved by Argparse.
 *
 * 1) Parser state is stored entirely in global variables, some of
 * which are static and inaccessible. This means only one thread can
 * use getopt(). It also means it's not possible to recursively parse
 * nested sub-arguments while in the middle of argument parsing.
 * Argparse fixes this by storing all state on a local struct.
 *
 * 2) The POSIX standard provides no way to properly reset the parser.
 * This means for portable code that getopt() is only good for one
 * run, over one argv with one option string. It also means subcommand
 * options cannot be processed with getopt(). Most implementations
 * provide a method to reset the parser, but it's not portable.
 * Argparse provides an argparse_arg() function for stepping over
 * subcommands and continuing parsing of options with another option
 * string. The Argparse struct itself can be passed around to
 * subcommand handlers for additional subcommand option parsing. A
 * full reset can be achieved by with an additional argparse_init().
 *
 * 3) Error messages are printed to stderr. This can be disabled with
 * opterr, but the messages themselves are still inaccessible.
 * Argparse solves this by writing an error message in its errmsg
 * field. The downside to Argparse is that this error message will
 * always be in English rather than the current locale.
 *
 * Argparse should be familiar with anyone accustomed to getopt(), and
 * it could be a nearly drop-in replacement. The option string is the
 * same and the fields have the same names as the getopt() global
 * variables (optarg, optind, optopt).
 *
 * Argparse also supports GNU-style long options with argparse_long().
 * The interface is slightly different and simpler than getopt_long().
 *
 * By default, argv is permuted as it is parsed, moving non-option
 * arguments to the end. This can be disabled by setting the `permute`
 * field to 0 after initialization.
 */

#pragma once

#include "mos/moslib_global.h"

typedef struct
{
    const char **argv;
    int permute;
    int optind;
    int optopt;
    const char *optarg;
    char errmsg[64];
    int subopt;
} argparse_state_t;

typedef enum
{
    ARGPARSE_NONE,     // no argument
    ARGPARSE_REQUIRED, // an argument is required
    ARGPARSE_OPTIONAL  // an argument is optional
} argparse_argtype;

typedef struct
{
    const char *full;
    char abbr;
    argparse_argtype argtype;
    const char *help;
} argparse_arg_t;

/**
 * Initializes the parser state.
 */
MOSAPI void argparse_init(argparse_state_t *options, const char *argv[]);

/**
 * Read the next option in the argv array.
 * @param optstring a getopt()-formatted option string.
 * @return the next option character, -1 for done, or '?' for error
 *
 * Just like getopt(), a character followed by no colons means no
 * argument. One colon means the option has a required argument. Two
 * colons means the option takes an optional argument.
 */
MOSAPI int argparse(argparse_state_t *options, const char *optstring);

/**
 * Handles GNU-style long options in addition to getopt() options.
 * This works a lot like GNU's getopt_long(). The last option in
 * longopts must be all zeros, marking the end of the array. The
 * longindex argument may be NULL.
 */
MOSAPI int argparse_long(argparse_state_t *options, const argparse_arg_t *longopts, int *longindex);

/**
 * Used for stepping over non-option arguments.
 * @return the next non-option argument, or NULL for no more arguments
 *
 * Argument parsing can continue with argparse() after using this
 * function. That would be used to parse the options for the
 * subcommand returned by argparse_arg(). This function allows you to
 * ignore the value of optind.
 */
MOSAPI const char *argparse_arg(argparse_state_t *options);

/**
 * @brief Print usage information
 *
 * @param options argparse_state_t
 * @param args argparse_arg_t
 * @param usage program usage string
 * @return MOSAPI
 */
MOSAPI void argparse_usage(argparse_state_t *options, const argparse_arg_t *args, const char *usage);
