//
// Created by cp723 on 2/7/2019.
//

#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(USE_MPI)
#include <mpi.h>
#else
#include <omp.h>
#endif

#include "crypto/cipher.h"
#include "crypto/ec.h"
#include "crypto/hash.h"
#include "perm.h"
#include "seed_iter.h"
#include "util.h"
#include "uuid.h"
#include "validator.h"

#if defined(USE_MPI)
#include "cmdline/cmdline_mpi.h"
#else
#include "cmdline/cmdline_omp.h"
#endif

enum StatusCode { SC_Found = 0, SC_NotFound = 1, SC_Failure = 2 };

// If using OpenMP, and using Clang 10+ or GCC 9+, support omp_pause_resource_all
#if !defined(USE_MPI) && \
        ((defined(__clang__) && __clang_major__ >= 10) || (!defined(__clang) && __GNUC__ >= 9))
#define OMP_DESTROY()                                             \
    if (omp_pause_resource_all(omp_pause_hard)) {                 \
        fprintf(stderr, "ERROR: omp_pause_resource_all failed."); \
    }
#else
#define OMP_DESTROY()
#endif

// By setting it to 0, we're assuming it'll be zeroified when arguments are first created
#define MODE_NONE 0
// Used with symmetric encryption
#define MODE_CIPHER 0b1
// Used with matching a public key
#define MODE_EC 0b10
// Used with matching a digest
#define MODE_HASH 0b100
// Used alongside MODE_HASH for a custom digest_size
#define MODE_XOF 0b1000

#define DEFAULT_XOF_SIZE 32

typedef struct Algo {
    const char* abbr_name;
    const char* full_name;
    int nid;
    int mode;
} Algo;

const Algo supportedAlgos[] = {
        {"none", "None", 0, MODE_NONE},
        // Cipher algorithms
        {"aes", "AES-256-ECB", NID_aes_256_ecb, MODE_CIPHER},
        {"chacha20", "ChaCha20", NID_chacha20, MODE_CIPHER},
        // EC algorithms
        {"ecc", "Secp256r1", NID_X9_62_prime256v1, MODE_EC},
        // Hashing algorithms
        {"md5", "MD5", NID_md5, MODE_HASH},
        {"sha1", "SHA1", NID_sha1, MODE_HASH},
        {"sha224", "SHA2-224", NID_sha224, MODE_HASH},
        {"sha256", "SHA2-256", NID_sha256, MODE_HASH},
        {"sha384", "SHA2-384", NID_sha384, MODE_HASH},
        {"sha512", "SHA2-512", NID_sha512, MODE_HASH},
        {"sha3-224", "SHA3-224", NID_sha3_224, MODE_HASH},
        {"sha3-256", "SHA3-256", NID_sha3_256, MODE_HASH},
        {"sha3-384", "SHA3-384", NID_sha3_384, MODE_HASH},
        {"sha3-512", "SHA3-512", NID_sha3_512, MODE_HASH},
        {"shake128", "SHAKE128", NID_shake128, MODE_HASH | MODE_XOF},
        {"shake256", "SHAKE256", NID_shake256, MODE_HASH | MODE_XOF},
        {"kang12", "KangarooTwelve", NID_kang12, MODE_HASH | MODE_XOF},
        {0},
};

struct Params {
    char *seed_hex, *client_crypto_hex, *uuid_hex, *iv_hex, *salt_hex;
};

const Algo* findAlgo(const char* abbr_name, const Algo* algos) {
    while (algos->abbr_name != NULL) {
        if (!strcmp(abbr_name, algos->abbr_name)) {
            return algos;
        }
        algos++;
    }

    return NULL;
}

int checkUsage(int argc, const struct gengetopt_args_info* args_info) {
    if (args_info->usage_given || argc < 2) {
        fprintf(stderr, "%s\n", gengetopt_args_info_usage);
        return 1;
    }

    if (args_info->inputs_num == 0) {
        if (!args_info->random_flag && !args_info->benchmark_flag) {
            fprintf(stderr, "%s\n", gengetopt_args_info_usage);
            return 1;
        }
    } else if (args_info->mode_given) {
        const Algo* algo = &(supportedAlgos[args_info->mode_arg]);

        if (algo->mode == MODE_NONE || args_info->random_flag || args_info->benchmark_flag) {
            fprintf(stderr, "%s\n", gengetopt_args_info_usage);
            return 1;
        }
    }

    return 0;
}

int validateArgs(const struct gengetopt_args_info* args_info) {
    // Manually enforce requirement since built-in required not used with --usage
    if (!args_info->mode_given) {
        fprintf(stderr, "%s: --mode option is required\n", CMDLINE_PARSER_PACKAGE);
        return 1;
    }

    if (args_info->mismatches_arg > SEED_SIZE * 8) {
        fprintf(stderr, "--mismatches cannot exceed the seed size of 256-bits.\n");
        return 1;
    }

    if (args_info->subkey_arg > SEED_SIZE * 8) {
        fprintf(stderr, "--subkey cannot exceed the seed size of 256-bits.\n");
        return 1;
    } else if (args_info->subkey_arg < 1) {
        fprintf(stderr, "--subkey must be at least 1.\n");
        return 1;
    }

#ifndef USE_MPI
    if (args_info->threads_arg > omp_get_thread_limit()) {
        fprintf(stderr, "--threads exceeds program thread limit.\n");
        return 1;
    }
#endif

    if (args_info->mismatches_arg < 0) {
        if (args_info->random_flag) {
            fprintf(stderr, "--mismatches must be set and non-negative when using --random.\n");
            return 1;
        }
        if (args_info->benchmark_flag) {
            fprintf(stderr, "--mismatches must be set and non-negative when using --benchmark.\n");
            return 1;
        }
        if (args_info->fixed_flag) {
            fprintf(stderr, "--mismatches must be set and non-negative when using --fixed.\n");
            return 1;
        }
    } else if (args_info->mismatches_arg > args_info->subkey_arg) {
        fprintf(stderr, "--mismatches cannot be set larger than --subkey.\n");
        return 1;
    }

    return 0;
}

int parse_params(struct Params* params, const struct gengetopt_args_info* args_info) {
    if (args_info->inputs_num < 1) {
        return 0;
    }

    if (strlen(args_info->inputs[0]) != SEED_SIZE * 2) {
        fprintf(stderr, "HOST_SEED must be %d byte(s) long.\n", SEED_SIZE);
        return 1;
    }

    params->seed_hex = args_info->inputs[0];

    const Algo* algo = findAlgo(args_info->mode_orig, supportedAlgos);

    if (algo->mode & MODE_CIPHER) {
        if (args_info->inputs_num < 3 || args_info->inputs_num > 4) {
            fprintf(stderr, "%s\n", gengetopt_args_info_usage);
            return 1;
        }

        const EVP_CIPHER* evp_cipher = EVP_get_cipherbynid(algo->nid);
        if (evp_cipher == NULL) {
            fprintf(stderr, "Not a valid EVP cipher nid.\n");
            return 1;
        }
        size_t block_len = EVP_CIPHER_block_size(evp_cipher);
        if (strlen(args_info->inputs[1]) % block_len * 2 != 0) {
            fprintf(stderr, "CLIENT_CIPHER not a multiple of the block size %zu bytes for %s\n",
                    block_len, algo->full_name);
            return 1;
        }

        params->client_crypto_hex = args_info->inputs[1];

        if (strlen(args_info->inputs[2]) != UUID_STR_LEN) {
            fprintf(stderr, "UUID not %d characters long.\n", UUID_STR_LEN);
            return 1;
        }

        params->uuid_hex = args_info->inputs[2];

        if (args_info->inputs_num == 4) {
            if (EVP_CIPHER_iv_length(evp_cipher) == 0) {
                fprintf(stderr, "The chosen cipher doesn't require an IV.\n");
                return 1;
            }
            if (strlen(args_info->inputs[3]) != EVP_CIPHER_iv_length(evp_cipher) * 2) {
                fprintf(stderr,
                        "Length of IV doesn't match the chosen cipher's required IV"
                        " length match\n");
                return 1;
            }

            params->iv_hex = args_info->inputs[3];
        }
    } else if (algo->mode & MODE_EC) {
        if (args_info->inputs_num != 2) {
            fprintf(stderr, "%s\n", gengetopt_args_info_usage);
            return 1;
        }

        EC_GROUP* group = EC_GROUP_new_by_curve_name(algo->nid);
        if (group == NULL) {
            fprintf(stderr, "EC_GROUP_new_by_curve_name failed.\n");
            return 1;
        }
        size_t order_len = (EC_GROUP_order_bits(group) + 7) / 8;
        size_t comp_len = order_len + 1;
        size_t uncomp_len = (order_len * 2) + 1;
        if (strlen(args_info->inputs[1]) != comp_len * 2 &&
            strlen(args_info->inputs[1]) != uncomp_len * 2) {
            fprintf(stderr, "CLIENT_PUB_KEY not %zu nor %zu bytes for %s\n", comp_len, uncomp_len,
                    algo->full_name);
            return 1;
        }
        EC_GROUP_free(group);

        params->client_crypto_hex = args_info->inputs[1];
    } else if (algo->mode & MODE_HASH) {
        if (args_info->inputs_num < 2 || args_info->inputs_num > 3) {
            fprintf(stderr, "%s\n", gengetopt_args_info_usage);
            return 1;
        }

        if (!(algo->mode & MODE_XOF)) {
            const EVP_MD* md = EVP_get_digestbynid(algo->nid);
            if (md == NULL) {
                fprintf(stderr,
                        "ERROR: EVP_get_digestbynid failed.\nOpenSSL Error:"
                        "%s\n",
                        ERR_error_string(ERR_get_error(), NULL));
                return 1;
            }
            size_t digest_size = EVP_MD_size(md);

            if (strlen(args_info->inputs[1]) != digest_size * 2) {
                fprintf(stderr, "CLIENT_DIGEST not equivalent to %zu bytes for %s\n", digest_size,
                        algo->full_name);
                return 1;
            }
        }

        params->client_crypto_hex = args_info->inputs[1];

        if (args_info->inputs_num > 2) {
            params->salt_hex = args_info->inputs[2];
        }
    }
    // MODE_NONE
    else if (args_info->inputs_num != 1) {
        fprintf(stderr, "%s\n", gengetopt_args_info_usage);
        return 1;
    }

    return 0;
}

int parse_hex_handler(unsigned char* buffer, const char* hex) {
    int status = parseHex(buffer, hex);

    if (status == 1) {
        fprintf(stderr, "ERROR: CIPHER had non-hexadecimal characters.\n");
    } else if (status == 2) {
        fprintf(stderr, "ERROR: CIPHER did not have even length.\n");
    }

    return status != 0;
}

/// OpenMP implementation
/// \return Returns a 0 on successfully finding a match, a 1 when unable to find a match,
/// and a 2 when a general error has occurred.
int main(int argc, char* argv[]) {
    int my_rank;

#ifdef USE_MPI
    int nprocs;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
#else
    int core_count;
#endif

    struct Params params;
    struct gengetopt_args_info args_info;

    unsigned char host_seed[SEED_SIZE];
    unsigned char client_seed[SEED_SIZE];

    const EVP_CIPHER* evp_cipher;
    unsigned char client_cipher[EVP_MAX_BLOCK_LENGTH];
    unsigned char uuid[UUID_SIZE];
    unsigned char iv[EVP_MAX_IV_LENGTH];
    EC_GROUP* ec_group;
    EC_POINT* client_ec_point;
    unsigned char* client_digest;
    size_t digest_size;
    const EVP_MD* md;
    unsigned char* salt = NULL;
    size_t salt_size = 0;

    int mismatch, ending_mismatch;
    int random_flag, benchmark_flag;
    int all_flag, count_flag, verbose_flag;
    int subseed_length;
    const Algo* algo;

    double start_time, duration, key_rate;
    long long int validated_keys = 0;
    int found, subfound;

    memset(&params, 0, sizeof(params));

    // Parse arguments
    if (cmdline_parser(argc, argv, &args_info)) {
#ifdef USE_MPI
        MPI_Finalize();
#else
        OMP_DESTROY()
#endif

        return SC_Failure;
    }

    if (checkUsage(argc, &args_info)) {
#ifdef USE_MPI
        MPI_Finalize();
#else
        OMP_DESTROY()
#endif

        return EXIT_SUCCESS;
    }

    if (validateArgs(&args_info) || parse_params(&params, &args_info)) {
#ifdef USE_MPI
        MPI_Finalize();
#else
        OMP_DESTROY()
#endif

        return SC_Failure;
    }

    algo = findAlgo(args_info.mode_orig, supportedAlgos);
    random_flag = args_info.random_flag;
    benchmark_flag = args_info.benchmark_flag;
    all_flag = args_info.all_flag;
    count_flag = args_info.count_flag;
    verbose_flag = args_info.verbose_flag;
    subseed_length = args_info.subkey_arg;

    mismatch = 0;
    ending_mismatch = args_info.subkey_arg;

    // If --fixed option was set, set the validation range to only use the --mismatches value.
    if (args_info.fixed_flag) {
        mismatch = args_info.mismatches_arg;
        ending_mismatch = args_info.mismatches_arg;
    }
    // If --mismatches is set and non-negative, set the ending_mismatch to its value.
    else if (args_info.mismatches_arg >= 0) {
        ending_mismatch = args_info.mismatches_arg;
    }

#ifndef USE_MPI
    if (args_info.threads_arg > 0) {
        omp_set_num_threads(args_info.threads_arg);
    }

    // omp_get_num_threads() must be called in a parallel region, but
    // ensure that only one thread calls it
#pragma omp parallel default(none) shared(core_count)
#pragma omp single
    core_count = omp_get_num_threads();
#endif

    // Memory alloc/init
    if (algo->mode & MODE_CIPHER) {
        evp_cipher = EVP_get_cipherbynid(algo->nid);
    } else if (algo->mode & MODE_EC) {
        if ((ec_group = EC_GROUP_new_by_curve_name(algo->nid)) == NULL) {
            fprintf(stderr, "ERROR: EC_GROUP_new_by_curve_name failed.\nOpenSSL Error: %s\n",
                    ERR_error_string(ERR_get_error(), NULL));

            OMP_DESTROY()

            return SC_Failure;
        }

        if ((client_ec_point = EC_POINT_new(ec_group)) == NULL) {
            fprintf(stderr, "ERROR: EC_POINT_new failed.\nOpenSSL Error: %s\n",
                    ERR_error_string(ERR_get_error(), NULL));

            EC_GROUP_free(ec_group);

            OMP_DESTROY()

            return SC_Failure;
        }
    } else if (algo->mode & MODE_HASH) {
        if (algo->nid != NID_kang12 && (md = EVP_get_digestbynid(algo->nid)) == NULL) {
            fprintf(stderr, "ERROR: EVP_get_digestbynid failed.\nOpenSSL Error: %s\n",
                    ERR_error_string(ERR_get_error(), NULL));

            // No need to deallocate an EVP_MD

            OMP_DESTROY()

            return SC_Failure;
        }

        if (algo->mode & MODE_XOF) {
            if (random_flag || benchmark_flag) {
                digest_size = DEFAULT_XOF_SIZE;
            } else {
                digest_size = (strlen(params.client_crypto_hex) + 1) / 2;
            }
        } else {
            digest_size = EVP_MD_size(md);
        }

        if ((client_digest = malloc(digest_size)) == NULL) {
            perror("ERROR");

            OMP_DESTROY()

            return SC_Failure;
        }

        if (params.salt_hex != NULL) {
            salt_size = (strlen(params.salt_hex) + 1) / 2;

            if ((salt = malloc(salt_size)) == NULL) {
                perror("ERROR");

                free(client_digest);
                OMP_DESTROY()

                return SC_Failure;
            }
        }
    }

    if (random_flag || benchmark_flag) {
#ifdef USE_MPI
        if (my_rank == 0) {
#endif
            gmp_randstate_t randstate;

            // Set the gmp prng algorithm and set a seed based on the current time
            gmp_randinit_default(randstate);
            gmp_randseed_ui(randstate, (unsigned long)time(NULL));

            getRandomSeed(host_seed, SEED_SIZE, randstate);
            getRandomCorruptedSeed(client_seed, host_seed, args_info.mismatches_arg, SEED_SIZE,
                                   subseed_length, randstate, benchmark_flag,
#ifdef USE_MPI
                                   nprocs);
#else
                               core_count);
#endif

            if (algo->mode & MODE_CIPHER) {
                size_t iv_length = EVP_CIPHER_iv_length(evp_cipher);

                if (iv_length > 0) {
                    getRandomSeed(iv, iv_length, randstate);
                }

                getRandomSeed(uuid, AES_BLOCK_SIZE, randstate);

                if (evpEncrypt(client_cipher, NULL, evp_cipher, client_seed, uuid, UUID_SIZE, iv)) {
                    fprintf(stderr, "ERROR: Initial encryption failed.\nOpenSSL Error: %s\n",
                            ERR_error_string(ERR_get_error(), NULL));

                    OMP_DESTROY()

                    return SC_Failure;
                }
            } else if (algo->mode & MODE_EC) {
                if (getEcPublicKey(client_ec_point, NULL, ec_group, client_seed, SEED_SIZE)) {
                    EC_POINT_free(client_ec_point);
                    EC_GROUP_free(ec_group);

                    OMP_DESTROY()

                    return SC_Failure;
                }
            } else if (algo->mode & MODE_HASH) {
                int hash_status;

                // No EVP variant exists for KangarooTwelve
                if (algo->nid == NID_kang12) {
                    hash_status =
                            kang12Hash(client_digest, digest_size, client_seed, SEED_SIZE, NULL, 0);
                }
                // No need to use the faster variants since this isn't a time critical step
                else {
                    hash_status =
                            evpHash(client_digest, algo->mode & MODE_XOF ? &digest_size : NULL,
                                    NULL, md, client_seed, SEED_SIZE, NULL, 0);
                }

                if (hash_status) {
                    if (salt_size > 0) {
                        free(salt);
                    }
                    free(client_digest);

                    OMP_DESTROY()

                    return SC_Failure;
                }
            }

            // Clear GMP PRNG
            gmp_randclear(randstate);
#ifdef USE_MPI
        }

        // Broadcast all of the relevant variable to every rank
        MPI_Bcast(host_seed, SEED_SIZE, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        MPI_Bcast(client_seed, SEED_SIZE, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

        if (algo->mode & MODE_CIPHER) {
            MPI_Bcast(client_cipher, AES_BLOCK_SIZE, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
            MPI_Bcast(uuid, UUID_SIZE, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        } else if (algo->mode & MODE_EC) {
            unsigned char client_public_key[100];
            int len;

            if (my_rank == 0) {
                if ((len = EC_POINT_point2oct(ec_group, client_ec_point,
                                              POINT_CONVERSION_COMPRESSED, client_public_key,
                                              sizeof(client_public_key), NULL)) == 0) {
                    fprintf(stderr, "ERROR: EC_POINT_point2oct failed.\nOpenSSL Error: %s\n",
                            ERR_error_string(ERR_get_error(), NULL));

                    EC_POINT_free(client_ec_point);
                    EC_GROUP_free(ec_group);

                    return SC_Failure;
                }
            }

            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(client_public_key, len, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

            EC_POINT_oct2point(ec_group, client_ec_point, client_public_key, len, NULL);
        } else if (algo->mode & MODE_HASH) {
            MPI_Bcast(client_digest, digest_size, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        }
#endif
    } else {
        int parse_status = parse_hex_handler(host_seed, params.seed_hex);

        if (!parse_status) {
            if (algo->mode & MODE_CIPHER) {
                parse_status = parse_hex_handler(client_cipher, params.client_crypto_hex);

                if (!parse_status && uuid_parse(uuid, params.uuid_hex)) {
                    fprintf(stderr, "ERROR: UUID not in canonical form.\n");
                    parse_status = 1;
                }

                if (!parse_status && params.iv_hex != NULL) {
                    parse_status = parse_hex_handler(iv, params.iv_hex);
                }
            } else if (algo->mode & MODE_EC) {
                if (EC_POINT_hex2point(ec_group, params.client_crypto_hex, client_ec_point, NULL) ==
                    NULL) {
                    fprintf(stderr, "ERROR: EC_POINT_hex2point failed.\nOpenSSL Error: %s\n",
                            ERR_error_string(ERR_get_error(), NULL));
                    parse_status = 1;
                }
            }
        }

        if (parse_status) {
            if (algo->mode & MODE_EC) {
                EC_POINT_free(client_ec_point);
                EC_GROUP_free(ec_group);
            }
            OMP_DESTROY()

            return SC_Failure;
        } else if (algo->mode & MODE_HASH) {
            switch (parseHex(client_digest, params.client_crypto_hex)) {
                case 1:
                    fprintf(stderr, "ERROR: CLIENT_DIGEST had non-hexadecimal characters.\n");

                    free(salt);
                    free(client_digest);
                    OMP_DESTROY()

                    return SC_Failure;
                case 2:
                    fprintf(stderr, "ERROR: CLIENT_DIGEST did not have even length.\n");

                    free(salt);
                    free(client_digest);
                    OMP_DESTROY()

                    return SC_Failure;
                default:
                    break;
            }

            if (params.salt_hex != NULL) {
                switch (parseHex(salt, params.salt_hex)) {
                    case 1:
                        fprintf(stderr, "ERROR: SALT had non-hexadecimal characters.\n");

                        free(salt);
                        free(client_digest);
                        OMP_DESTROY()

                        return SC_Failure;
                    case 2:
                        fprintf(stderr, "ERROR: SALT did not have even length.\n");

                        free(salt);
                        free(client_digest);
                        OMP_DESTROY()

                        return SC_Failure;
                    default:
                        break;
                }
            }
        }
    }

    if (verbose_flag
#ifdef USE_MPI
        && my_rank == 0
#endif
    ) {
        fprintf(stderr, "INFO: Using HOST_SEED:                  ");
        fprintHex(stderr, host_seed, SEED_SIZE);
        fprintf(stderr, "\n");

        if (random_flag || benchmark_flag) {
            fprintf(stderr, "INFO: Using CLIENT_SEED (%d mismatches): ", args_info.mismatches_arg);
            fprintHex(stderr, client_seed, SEED_SIZE);
            fprintf(stderr, "\n");
        }

        if (algo->mode & MODE_CIPHER) {
            char uuid_str[UUID_STR_LEN + 1];

            fprintf(stderr, "INFO: Using %s CLIENT_CIPHER: %*s", algo->full_name,
                    (int)strlen(algo->full_name) - 4, "");
            fprintHex(stderr, client_cipher, AES_BLOCK_SIZE);
            fprintf(stderr, "\n");

            // Convert the uuid to a string for printing
            fprintf(stderr, "INFO: Using UUID:                       ");
            uuid_unparse(uuid_str, uuid);
            fprintf(stderr, "%s\n", uuid_str);

            if (EVP_CIPHER_iv_length(evp_cipher) > 0) {
                fprintf(stderr, "INFO: Using IV:                         ");
                fprintHex(stderr, iv, EVP_CIPHER_iv_length(evp_cipher));
                fprintf(stderr, "\n");
            }
        } else if (algo->mode & MODE_EC) {
            if (random_flag || benchmark_flag) {
                fprintf(stderr, "INFO: Using %s HOST_PUB_KEY:%*s", algo->full_name,
                        (int)strlen(algo->full_name) - 4, "");
                if (fprintfEcPoint(stderr, ec_group, client_ec_point, POINT_CONVERSION_COMPRESSED,
                                   NULL)) {
                    fprintf(stderr, "ERROR: fprintfEcPoint failed.\n");

                    EC_POINT_free(client_ec_point);
                    EC_GROUP_free(ec_group);

                    OMP_DESTROY()

                    return SC_Failure;
                }
                fprintf(stderr, "\n");
            }

            fprintf(stderr, "INFO: Using %s CLIENT_PUB_KEY:%*s", algo->full_name,
                    (int)strlen(algo->full_name) - 6, "");
            if (fprintfEcPoint(stderr, ec_group, client_ec_point, POINT_CONVERSION_COMPRESSED,
                               NULL)) {
                fprintf(stderr, "ERROR: fprintfEcPoint failed.\n");

                EC_POINT_free(client_ec_point);
                EC_GROUP_free(ec_group);

                OMP_DESTROY()

                return SC_Failure;
            }
            fprintf(stderr, "\n");
        } else if (algo->mode & MODE_HASH) {
            fprintf(stderr, "INFO: Using %s ", algo->full_name);
            if (algo->mode & MODE_XOF) {
                fprintf(stderr, "(%zu bytes) ", digest_size);
            }
            fprintf(stderr, "CLIENT_DIGEST: ");
            fprintHex(stderr, client_digest, digest_size);
            fprintf(stderr, "\n");

            if (salt_size > 0) {
                fprintf(stderr, "INFO: Using %s SALT:      %*s", algo->full_name,
                        (int)strlen(algo->full_name), "");
                fprintHex(stderr, salt, salt_size);
                fprintf(stderr, "\n");
            }
        }

        fflush(stderr);
    }

    found = 0;

#ifdef USE_MPI
    start_time = MPI_Wtime();
#else
    start_time = omp_get_wtime();
#endif

    // clang-format off
    for (; mismatch <= ending_mismatch && !found; mismatch++) {
        if (verbose_flag
#ifdef USE_MPI
            && my_rank == 0
#endif
        ) {
            fprintf(stderr, "INFO: Checking a hamming distance of %d...\n", mismatch);
            fflush(stderr);
        }

#ifndef USE_MPI
#pragma omp parallel default(none)                                                           \
        shared(found, host_seed, client_seed, evp_cipher, client_cipher, iv, uuid, ec_group, \
               client_ec_point, md, client_digest, digest_size, salt, salt_size, mismatch,   \
               validated_keys, algo, subseed_length, all_flag, count_flag,                   \
               verbose_flag) private(subfound, my_rank)
        {
        long long int sub_validated_keys = 0;
        my_rank = omp_get_thread_num();
#endif

        size_t max_count;
        mpz_t key_count, first_perm, last_perm;

        int (*crypto_func)(const unsigned char*, void*) = NULL;
        int (*crypto_cmp)(void*) = NULL;

        void* v_args = NULL;

        subfound = 0;

        if (algo->mode & MODE_CIPHER) {
#ifndef ALWAYS_EVP_AES
            // Use a custom implementation for improved speed
            if (algo->nid == NID_aes_256_ecb) {
                crypto_func = CryptoFunc_aes256;
                crypto_cmp = CryptoCmp_aes256;
            } else {
#endif
                crypto_func = CryptoFunc_cipher;
                crypto_cmp = CryptoCmp_cipher;
#ifndef ALWAYS_EVP_AES
            }
#endif

            v_args = CipherValidator_create(evp_cipher, client_cipher, uuid, UUID_SIZE,
                                            EVP_CIPHER_iv_length(evp_cipher) > 0 ? iv : NULL);
        } else if (algo->mode & MODE_EC) {
            crypto_func = CryptoFunc_ec;
            crypto_cmp = CryptoCmp_ec;
            v_args = EcValidator_create(ec_group, client_ec_point);
        } else if (algo->mode & MODE_HASH) {
            if (algo->nid == NID_kang12) {
                crypto_func = CryptoFunc_kang12;
                crypto_cmp = CryptoCmp_kang12;
                v_args = Kang12Validator_create(client_digest, digest_size, salt, salt_size);
            } else {
                crypto_func = CryptoFunc_hash;
                crypto_cmp = CryptoCmp_hash;
                v_args = HashValidator_create(md, client_digest, digest_size, salt, salt_size);
            }
        }

        mpz_inits(key_count, first_perm, last_perm, NULL);

        mpz_bin_uiui(key_count, subseed_length, mismatch);

        // Only have this rank run if it's within range of possible keys
        if (mpz_cmp_ui(key_count, (unsigned long)my_rank) > 0 && subfound >= 0) {
            // Set the count of pairs to the range of possible keys if there are more ranks
            // than possible keys
#ifdef USE_MPI
            max_count = nprocs;
            if (mpz_cmp_ui(key_count, nprocs) < 0) {
#else
            max_count = omp_get_num_threads();
            if (mpz_cmp_ui(key_count, omp_get_num_threads()) < 0) {
#endif
                max_count = mpz_get_ui(key_count);
            }

                getPermPair(first_perm, last_perm, (size_t)my_rank, max_count, mismatch,
                            subseed_length);

#ifdef USE_MPI
            subfound = findMatchingSeed(client_seed, host_seed, first_perm, last_perm, all_flag,
                                        count_flag ? &validated_keys : NULL, &found, verbose_flag,
                                        my_rank, max_count, crypto_func, crypto_cmp, v_args);
#else
            subfound = findMatchingSeed(client_seed, host_seed, first_perm, last_perm, all_flag,
                                        count_flag ? &sub_validated_keys : NULL, &found,
                                        crypto_func, crypto_cmp, v_args);
#endif
        }

        mpz_clears(key_count, first_perm, last_perm, NULL);
        if (algo->mode & MODE_CIPHER) {
            CipherValidator_destroy(v_args);
        } else if (algo->mode & MODE_EC) {
            EcValidator_destroy(v_args);
        } else if (algo->mode & MODE_HASH) {
            if (algo->nid == NID_kang12) {
                Kang12Validator_destroy(v_args);
            } else {
                HashValidator_destroy(v_args);
            }
        }

#ifdef USE_MPI
        if (subfound < 0) {
            // Cleanup
            if (algo->mode & MODE_EC) {
                EC_POINT_free(client_ec_point);
                EC_GROUP_free(ec_group);
            } else if (algo->mode & MODE_HASH) {
                if (salt_size > 0) {
                    free(salt);
                }
                free(client_digest);
            }
        }
#else
#pragma omp critical
        {
            // If the result is positive set the "global" found to 1. Will cause the other
            // threads to prematurely stop.
            if (subfound > 0) {
                // If it isn't already found nor is there an error found,
                if (!found) {
                    found = 1;
                }
            }
            // If the result is negative, set a flag that an error has occurred, and stop the other
            // threads. Will cause the other threads to prematurely stop.
            else if (subfound < 0) {
                found = -1;
            }

            validated_keys += sub_validated_keys;
        }
        }
#endif
    }

    if (algo->mode & MODE_EC) {
        EC_POINT_free(client_ec_point);
        EC_GROUP_free(ec_group);
    } else if (algo->mode & MODE_HASH) {
        if (salt_size > 0) {
            free(salt);
        }
        free(client_digest);
    }

#ifdef USE_MPI
    if ((mismatch <= ending_mismatch) && !(all_flag) && subfound == 0 && !found) {
        fprintf(stderr, "Rank %d Bleh\n", my_rank);
        MPI_Recv(&found, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    duration = MPI_Wtime() - start_time;

    fprintf(stderr, "INFO Rank %d: Clock time: %f s\n", my_rank, duration);

    if (my_rank == 0) {
        MPI_Reduce(MPI_IN_PLACE, &duration, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    } else {
        MPI_Reduce(&duration, &duration, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }

    if (my_rank == 0 && verbose_flag) {
        fprintf(stderr, "INFO: Max Clock time: %f s\n", duration);
    }

    if (count_flag) {
        if (my_rank == 0) {
            MPI_Reduce(MPI_IN_PLACE, &validated_keys, 1, MPI_LONG_LONG_INT, MPI_SUM, 0,
                       MPI_COMM_WORLD);

            // Divide validated_keys by duration
            key_rate = (double)validated_keys / duration;

            fprintf(stderr, "INFO: Keys searched: %lld\n", validated_keys);
            fprintf(stderr, "INFO: Keys per second: %.9g\n", key_rate);
        } else {
            MPI_Reduce(&validated_keys, &validated_keys, 1, MPI_LONG_LONG_INT, MPI_SUM, 0,
                       MPI_COMM_WORLD);
        }
    }

    if (subfound) {
        fprintHex(stdout, client_seed, SEED_SIZE);
        printf("\n");
    }

    // Cleanup
    MPI_Finalize();

    return EXIT_SUCCESS;
#else
    // Check if an error occurred in one of the threads.
    if (found < 0) {
        OMP_DESTROY()

        return SC_Failure;
    }

    duration = omp_get_wtime() - start_time;

    if (verbose_flag) {
        fprintf(stderr, "INFO: Clock time: %f s\n", duration);
        fprintf(stderr, "INFO: Found: %d\n", found);
    }

    if (count_flag) {
        // Divide validated_keys by duration
        key_rate = (double)validated_keys / duration;

        fprintf(stderr, "INFO: Keys searched: %lld\n", validated_keys);
        fprintf(stderr, "INFO: Keys per second: %.9g\n", key_rate);
    }

    if (found > 0) {
        fprintHex(stdout, client_seed, SEED_SIZE);
        printf("\n");
    }

    OMP_DESTROY()

    return found || algo->mode == MODE_NONE ? SC_Found : SC_NotFound;
#endif
}
    // clang-format on
