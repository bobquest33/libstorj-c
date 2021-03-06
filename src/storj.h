/**
 * @file storj.h
 * @brief Storj library.
 *
 * Implements functionality to upload and download files from the Storj
 * distributed network.
 */

#ifndef STORJ_H
#define STORJ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

#ifdef _WIN32
#include <time.h>
#endif

#define ERROR 1
#define OK 0

// File transfer success
#define STORJ_TRANSFER_OK 0
#define STORJ_TRANSFER_CANCELLED 1

// Bridge related errors 1000 to 1999
#define STORJ_BRIDGE_REQUEST_ERROR 1000
#define STORJ_BRIDGE_AUTH_ERROR 1001
#define STORJ_BRIDGE_TOKEN_ERROR 1002
#define STORJ_BRIDGE_TIMEOUT_ERROR 1003
#define STORJ_BRIDGE_INTERNAL_ERROR 1004
#define STORJ_BRIDGE_RATE_ERROR 1005
#define STORJ_BRIDGE_BUCKET_NOTFOUND_ERROR 1006
#define STORJ_BRIDGE_FILE_NOTFOUND_ERROR 1007
#define STORJ_BRIDGE_JSON_ERROR 1008
#define STORJ_BRIDGE_FRAME_ERROR 1009
#define STORJ_BRIDGE_POINTER_ERROR 1010
#define STORJ_BRIDGE_REPOINTER_ERROR 1011

// Farmer related errors 2000 to 2999
#define STORJ_FARMER_REQUEST_ERROR 2000
#define STORJ_FARMER_TIMEOUT_ERROR 2001
#define STORJ_FARMER_AUTH_ERROR 2002
#define STORJ_FARMER_EXHAUSTED_ERROR 2003
#define STORJ_FARMER_INTEGRITY_ERROR 2004

// File related errors 3000 to 3999
#define STORJ_FILE_INTEGRITY_ERROR 3000
#define STORJ_FILE_WRITE_ERROR 3001
#define STORJ_FILE_ENCRYPTION_ERROR 3002

// Exchange report codes
#define STORJ_REPORT_SUCCESS 1000;
#define STORJ_REPORT_FAILURE 1100;

// Exchange report messages
#define STORJ_REPORT_FAILED_INTEGRITY "FAILED_INTEGRITY"
#define STORJ_REPORT_SHARD_DOWNLOADED "SHARD_DOWNLOADED"
#define STORJ_REPORT_SHARD_UPLOADED "SHARD_UPLOADED"
#define STORJ_REPORT_DOWNLOAD_ERROR "DOWNLOAD_ERROR"
#define STORJ_REPORT_UPLOAD_ERROR "TRANSFER_FAILED"

/** @brief Bridge configuration options
 *
 * Proto can be "http" or "https", and the user/pass are used for
 * basic authentication to a Storj bridge.
 */
typedef struct {
    char *proto;
    char *host;
    int port;
    char *user;
    char *pass;
} storj_bridge_options_t;

/** @brief File encryption options
 *
 * The mnemonic is a BIP39 secret code used for generating keys for file
 * encryption and decryption.
 */
typedef struct storj_encrypt_options {
    char *mnemonic;
} storj_encrypt_options_t;

typedef enum {
    STORJ_PROXY_SOCKSV4 = 0,
    STORJ_PROXY_SOCKSV4A,
    STORJ_PROXY_SOCKSV5
} storj_proxy_version_t;

/** @brief HTTP configuration options
 *
 * Settings for making HTTP requests
 */
typedef struct storj_http_options {
    char *user_agent;
    storj_proxy_version_t proxy_version;
    char *proxy_host;
    int proxy_port;
} storj_http_options_t;

/** @brief A structure for a Storj user environment.
 *
 * This is the highest level structure and holds many commonly used options
 * and the event loop for queuing work.
 */
typedef struct storj_env {
    storj_bridge_options_t *bridge_options;
    storj_encrypt_options_t *encrypt_options;
    storj_http_options_t *http_options;
    uv_loop_t *loop;
} storj_env_t;

/** @brief A structure for queueing json request work
 */
typedef struct {
    storj_http_options_t *http_options;
    storj_bridge_options_t *options;
    char *method;
    char *path;
    bool auth;
    struct json_object *body;
    struct json_object *response;
    int status_code;
} json_request_t;

typedef enum {
    BUCKET_PUSH,
    BUCKET_PULL
} storj_bucket_op_t;

static const char *BUCKET_OP[] = { "PUSH", "PULL" };

/** @brief A data structure that represents an exchange report
 *
 * These are sent at the end of an exchange with a farmer to report the
 * performance and reliability of farmers.
 */
typedef struct {
    char *data_hash;
    char *reporter_id;
    char *farmer_id;
    char *client_id;
    uint64_t start;
    uint64_t end;
    unsigned int code;
    char *message;
    unsigned int send_status;
    unsigned int send_count;
    uint32_t pointer_index;
} storj_exchange_report_t;

/** @brief A function signature for download/upload progress callback
 */
typedef void (*storj_progress_cb)(double progress,
                                  uint64_t bytes,
                                  uint64_t total_bytes);

/** @brief A function signature for a download complete callback
 */
typedef void (*storj_finished_download_cb)(int status, FILE *fd);

/** @brief A function signature for an upload complete callback
 */
typedef void (*storj_finished_upload_cb)(int error_status);

/** @brief A structure that represents a pointer to a shard
 *
 * A shard is an encrypted piece of a file, a pointer holds all necessary
 * information to retrieve a shard from a farmer, including the IP address
 * and port of the farmer, as well as a token indicating a transfer has been
 * authorized. Other necessary information such as the expected hash of the
 * data, and the index position in the file is also included.
 *
 * The data can be replaced with new farmer contact, in case of failure, and the
 * total number of replacements can be tracked.
 */
typedef struct {
    unsigned int replace_count;
    char *token;
    char *shard_hash;
    char *shard_data;
    uint32_t index;
    int status;
    uint64_t size;
    uint64_t downloaded_size;
    char *farmer_id;
    char *farmer_address;
    int farmer_port;
    storj_exchange_report_t *report;
    uv_work_t *work;
} storj_pointer_t;

/** @brief A structure for file upload options
 */
typedef struct {
    int file_concurrency;
    int shard_concurrency;
    char *bucket_id;
    char *file_path;
    char *key_pass;
    char *mnemonic;
} storj_upload_opts_t;

/** @brief A structure that keeps state between multiple worker threads,
 * and for referencing a download to apply actions to an in-progress download.
 *
 * After work has been completed in a thread, its after work callback will
 * update and modify the state and then queue the next set of work based on the
 * changes, and added to the event loop. The state is all managed within one
 * thread, the event loop thread, and any work that is performed in another
 * thread should not modify this structure directly, but should pass a
 * reference to it, so that once the work is complete the state can be updated.
 */
typedef struct {
    uint64_t total_bytes;
    storj_env_t *env;
    char *file_id;
    char *bucket_id;
    FILE *destination;
    storj_progress_cb progress_cb;
    storj_finished_download_cb finished_cb;
    bool finished;
    bool cancelled;
    uint64_t shard_size;
    uint32_t total_shards;
    uint32_t completed_shards;
    uint32_t resolving_shards;
    storj_pointer_t *pointers;
    char *excluded_farmer_ids;
    uint32_t total_pointers;
    bool pointers_completed;
    uint32_t pointer_fail_count;
    bool requesting_pointers;
    int error_status;
    bool writing;
    char *token;
    bool requesting_token;
    uint32_t token_fail_count;
    uint8_t *decrypt_key;
    uint8_t *decrypt_ctr;
    uint32_t pending_work_count;
} storj_download_state_t;

/**
 * @brief Initialize a Storj environment
 *
 * This will setup an event loop for queueing further actions, as well
 * as define necessary configuration options for communicating with Storj
 * bridge, and for encrypting/decrypting files.
 *
 * @param[in] options
 * @param[in] encrypt_options
 * @return A null value on error, otherwise a storj_env pointer.
 */
storj_env_t *storj_init_env(storj_bridge_options_t *options,
                            storj_encrypt_options_t *encrypt_options,
                            storj_http_options_t *http_options);

/**
 * @brief Get the error message for an error code
 *
 * This function will return a error message associated with a storj
 * error code.
 *
 * @param[in] error_code The storj error code integer
 * @return A char pointer with error message
 */
char *storj_strerror(int error_code);

/**
 * @brief Get Storj bridge API information.
 *
 * This function will get general information about the storj bridge api.
 * The network i/o is performed in a thread pool with a libuv loop, and the
 * response is available in the first argument to the callback function.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_get_info(storj_env_t *env, uv_after_work_cb cb);

/**
 * @brief List available buckets for a user.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_get_buckets(storj_env_t *env, uv_after_work_cb cb);

/**
 * @brief Create a bucket.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_create_bucket(storj_env_t *env,
                               char *name,
                               uv_after_work_cb cb);

/**
 * @brief Delete a bucket.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_delete_bucket(storj_env_t *env, char *id, uv_after_work_cb cb);

/**
 * @brief Get a list of all files in a bucket.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_list_files(storj_env_t *env, char *id, uv_after_work_cb cb);

/**
 * @brief Create a PUSH or PULL bucket token.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_create_bucket_token(storj_env_t *env,
                                     char *bucket_id,
                                     storj_bucket_op_t operation,
                                     uv_after_work_cb cb);

/**
 * @brief Get pointers with locations to file shards.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_get_file_pointers(storj_env_t *env,
                                   char *bucket_id,
                                   char *file_id,
                                   uv_after_work_cb cb);

/**
 * @brief Delete a file in a bucket.
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_delete_file(storj_env_t *env,
                             char *bucket_id,
                             char *file_id,
                             uv_after_work_cb cb);

/**
 * @brief Create a file frame
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_create_frame(storj_env_t *env, uv_after_work_cb cb);

/**
 * @brief List available file frames
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_get_frames(storj_env_t *env, uv_after_work_cb cb);

/**
 * @brief Get information for a file frame
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_get_frame(storj_env_t *env,
                           char *frame_id,
                           uv_after_work_cb cb);

/**
 * @brief Delete a file frame
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_delete_frame(storj_env_t *env,
                              char *frame_id,
                              uv_after_work_cb cb);

/**
 * @brief Get metadata for a file
 *
 * @param[in] env The storj environment struct
 * @param[in] cb A function called with response when complete
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_get_file_info(storj_env_t *env,
                               char *bucket_id,
                               char *file_id,
                               uv_after_work_cb cb);

int storj_bridge_store_file(storj_env_t *env,
                           storj_upload_opts_t *opts,
                           storj_progress_cb progress_cb,
                           storj_finished_upload_cb finished_cb);


/**
 * @brief Will cancel a download
 *
 * @param[in] state A pointer to the the download state
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_resolve_file_cancel(storj_download_state_t *state);

/**
 * @brief Download a file
 *
 * @param[in] env A pointer to environment
 * @param[in] state A pointer to the the download state
 * @param[in] bucket_id Character array of bucket id
 * @param[in] file_id Character array of file id
 * @param[in] destination File descriptor of the destination
 * @param[in] progress_cb Function called with progress updates
 * @param[in] finished_cb Function called when download finished
 * @return A non-zero error value on failure and 0 on success.
 */
int storj_bridge_resolve_file(storj_env_t *env,
                              storj_download_state_t *state,
                              char *bucket_id,
                              char *file_id,
                              FILE *destination,
                              storj_progress_cb progress_cb,
                              storj_finished_download_cb finished_cb);
#ifdef __cplusplus
}
#endif

#endif /* STORJ_H */
