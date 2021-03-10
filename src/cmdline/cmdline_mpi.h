/** @file cmdline_mpi.h
 *  @brief The header file for the command line option parser
 *  generated by GNU Gengetopt version 2.23
 *  http://www.gnu.org/software/gengetopt.
 *  DO NOT modify this file, since it can be overwritten
 *  @author GNU Gengetopt */

#ifndef CMDLINE_MPI_H
#define CMDLINE_MPI_H

/* If we use autoconf.  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h> /* for FILE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CMDLINE_PARSER_PACKAGE
/** @brief the program name (used for printing errors) */
#define CMDLINE_PARSER_PACKAGE "rbc_validator_mpi"
#endif

#ifndef CMDLINE_PARSER_PACKAGE_NAME
/** @brief the complete program name (used for help and version) */
#define CMDLINE_PARSER_PACKAGE_NAME "rbc_validator_mpi"
#endif

#ifndef CMDLINE_PARSER_VERSION
/** @brief the program version */
#define CMDLINE_PARSER_VERSION "v0.1.0"
#endif

enum enum_mode { mode__NULL = -1, mode_arg_none = 0, mode_arg_aes, mode_arg_chacha20, mode_arg_ecc };

/** @brief Where the command line options are stored */
struct gengetopt_args_info
{
  const char *help_help; /**< @brief Print help and exit help description.  */
  const char *version_help; /**< @brief Print version and exit help description.  */
  const char *usage_help; /**< @brief Give a short usage message help description.  */
  enum enum_mode mode_arg;	/**< @brief (REQUIRED) Choose between only seed iteration (none), AES256 (aes), ChaCha20 (chacha20), and ECC Secp256r1 (ecc)..  */
  char * mode_orig;	/**< @brief (REQUIRED) Choose between only seed iteration (none), AES256 (aes), ChaCha20 (chacha20), and ECC Secp256r1 (ecc). original value given at command line.  */
  const char *mode_help; /**< @brief (REQUIRED) Choose between only seed iteration (none), AES256 (aes), ChaCha20 (chacha20), and ECC Secp256r1 (ecc). help description.  */
  int mismatches_arg;	/**< @brief The largest # of bits of corruption to test against, inclusively. Defaults to -1. If negative, then the size of key in bits will be the limit. If in random or benchmark mode, then this will also be used to corrupt the random key by the same # of bits; for this reason, it must be set and non-negative when in random or benchmark mode. Cannot be larger than what --subkey-size is set to. (default='-1').  */
  char * mismatches_orig;	/**< @brief The largest # of bits of corruption to test against, inclusively. Defaults to -1. If negative, then the size of key in bits will be the limit. If in random or benchmark mode, then this will also be used to corrupt the random key by the same # of bits; for this reason, it must be set and non-negative when in random or benchmark mode. Cannot be larger than what --subkey-size is set to. original value given at command line.  */
  const char *mismatches_help; /**< @brief The largest # of bits of corruption to test against, inclusively. Defaults to -1. If negative, then the size of key in bits will be the limit. If in random or benchmark mode, then this will also be used to corrupt the random key by the same # of bits; for this reason, it must be set and non-negative when in random or benchmark mode. Cannot be larger than what --subkey-size is set to. help description.  */
  int subkey_arg;	/**< @brief How many of the first bits to corrupt and iterate over. Must be between 1 and 256. Defaults to 256. (default='256').  */
  char * subkey_orig;	/**< @brief How many of the first bits to corrupt and iterate over. Must be between 1 and 256. Defaults to 256. original value given at command line.  */
  const char *subkey_help; /**< @brief How many of the first bits to corrupt and iterate over. Must be between 1 and 256. Defaults to 256. help description.  */
  int random_flag;	/**< @brief Instead of using arguments, randomly generate HOST_SEED and CLIENT_*. This must be accompanied by --mismatches, since it is used to corrupt the random key by the same # of bits. --random and --benchmark cannot be used together. (default=off).  */
  const char *random_help; /**< @brief Instead of using arguments, randomly generate HOST_SEED and CLIENT_*. This must be accompanied by --mismatches, since it is used to corrupt the random key by the same # of bits. --random and --benchmark cannot be used together. help description.  */
  int benchmark_flag;	/**< @brief Instead of using arguments, strategically generate HOST_SEED and CLIENT_*. Specifically, generates a client seed that's always 50% of the way through a rank's workload, but randomly chooses the thread. --random and --benchmark cannot be used together. (default=off).  */
  const char *benchmark_help; /**< @brief Instead of using arguments, strategically generate HOST_SEED and CLIENT_*. Specifically, generates a client seed that's always 50% of the way through a rank's workload, but randomly chooses the thread. --random and --benchmark cannot be used together. help description.  */
  int all_flag;	/**< @brief Don't cut out early when key is found. (default=off).  */
  const char *all_help; /**< @brief Don't cut out early when key is found. help description.  */
  int count_flag;	/**< @brief Count the number of keys tested and show it as verbose output. (default=off).  */
  const char *count_help; /**< @brief Count the number of keys tested and show it as verbose output. help description.  */
  int fixed_flag;	/**< @brief Only test the given mismatch, instead of progressing from 0 to --mismatches. This is only valid when --mismatches is set and non-negative. (default=off).  */
  const char *fixed_help; /**< @brief Only test the given mismatch, instead of progressing from 0 to --mismatches. This is only valid when --mismatches is set and non-negative. help description.  */
  int verbose_flag;	/**< @brief Produces verbose output and time taken to stderr. (default=off).  */
  const char *verbose_help; /**< @brief Produces verbose output and time taken to stderr. help description.  */
  
  unsigned int help_given ;	/**< @brief Whether help was given.  */
  unsigned int version_given ;	/**< @brief Whether version was given.  */
  unsigned int usage_given ;	/**< @brief Whether usage was given.  */
  unsigned int mode_given ;	/**< @brief Whether mode was given.  */
  unsigned int mismatches_given ;	/**< @brief Whether mismatches was given.  */
  unsigned int subkey_given ;	/**< @brief Whether subkey was given.  */
  unsigned int random_given ;	/**< @brief Whether random was given.  */
  unsigned int benchmark_given ;	/**< @brief Whether benchmark was given.  */
  unsigned int all_given ;	/**< @brief Whether all was given.  */
  unsigned int count_given ;	/**< @brief Whether count was given.  */
  unsigned int fixed_given ;	/**< @brief Whether fixed was given.  */
  unsigned int verbose_given ;	/**< @brief Whether verbose was given.  */

  char **inputs ; /**< @brief unnamed options (options without names) */
  unsigned inputs_num ; /**< @brief unnamed options number */
  int Benchmark_mode_counter; /**< @brief Counter for mode Benchmark */
  int Random_mode_counter; /**< @brief Counter for mode Random */
} ;

/** @brief The additional parameters to pass to parser functions */
struct cmdline_parser_params
{
  int override; /**< @brief whether to override possibly already present options (default 0) */
  int initialize; /**< @brief whether to initialize the option structure gengetopt_args_info (default 1) */
  int check_required; /**< @brief whether to check that all required options were provided (default 1) */
  int check_ambiguity; /**< @brief whether to check for options already specified in the option structure gengetopt_args_info (default 0) */
  int print_errors; /**< @brief whether getopt_long should print an error message for a bad option (default 1) */
} ;

/** @brief the purpose string of the program */
extern const char *gengetopt_args_info_purpose;
/** @brief the usage string of the program */
extern const char *gengetopt_args_info_usage;
/** @brief the description string of the program */
extern const char *gengetopt_args_info_description;
/** @brief all the lines making the help output */
extern const char *gengetopt_args_info_help[];

/**
 * The command line parser
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser (int argc, char **argv,
  struct gengetopt_args_info *args_info);

/**
 * The command line parser (version with additional parameters - deprecated)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param override whether to override possibly already present options
 * @param initialize whether to initialize the option structure my_args_info
 * @param check_required whether to check that all required options were provided
 * @return 0 if everything went fine, NON 0 if an error took place
 * @deprecated use cmdline_parser_ext() instead
 */
int cmdline_parser2 (int argc, char **argv,
  struct gengetopt_args_info *args_info,
  int override, int initialize, int check_required);

/**
 * The command line parser (version with additional parameters)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param params additional parameters for the parser
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_ext (int argc, char **argv,
  struct gengetopt_args_info *args_info,
  struct cmdline_parser_params *params);

/**
 * Save the contents of the option struct into an already open FILE stream.
 * @param outfile the stream where to dump options
 * @param args_info the option struct to dump
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_dump(FILE *outfile,
  struct gengetopt_args_info *args_info);

/**
 * Save the contents of the option struct into a (text) file.
 * This file can be read by the config file parser (if generated by gengetopt)
 * @param filename the file where to save
 * @param args_info the option struct to save
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_file_save(const char *filename,
  struct gengetopt_args_info *args_info);

/**
 * Print the help
 */
void cmdline_parser_print_help(void);
/**
 * Print the version
 */
void cmdline_parser_print_version(void);

/**
 * Initializes all the fields a cmdline_parser_params structure 
 * to their default values
 * @param params the structure to initialize
 */
void cmdline_parser_params_init(struct cmdline_parser_params *params);

/**
 * Allocates dynamically a cmdline_parser_params structure and initializes
 * all its fields to their default values
 * @return the created and initialized cmdline_parser_params structure
 */
struct cmdline_parser_params *cmdline_parser_params_create(void);

/**
 * Initializes the passed gengetopt_args_info structure's fields
 * (also set default values for options that have a default)
 * @param args_info the structure to initialize
 */
void cmdline_parser_init (struct gengetopt_args_info *args_info);
/**
 * Deallocates the string fields of the gengetopt_args_info structure
 * (but does not deallocate the structure itself)
 * @param args_info the structure to deallocate
 */
void cmdline_parser_free (struct gengetopt_args_info *args_info);

/**
 * Checks that all the required options were specified
 * @param args_info the structure to check
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @return
 */
int cmdline_parser_required (struct gengetopt_args_info *args_info,
  const char *prog_name);

extern const char *cmdline_parser_mode_values[];  /**< @brief Possible values for mode. */


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CMDLINE_MPI_H */
