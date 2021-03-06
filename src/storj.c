#include "storj.h"
#include "http.h"

static void json_request_worker(uv_work_t *work)
{
    json_request_t *req = work->data;
    int status_code = 0;
    req->response = fetch_json(req->http_options,
                               req->options, req->method, req->path, req->body,
                               req->auth, NULL, &status_code);

    req->status_code = status_code;
}

static uv_work_t *uv_work_new()
{
    uv_work_t *work = malloc(sizeof(uv_work_t));
    assert(work != NULL);
    return work;
}

static json_request_t *json_request_new(
    storj_http_options_t *http_options,
    storj_bridge_options_t *options,
    char *method,
    char *path,
    struct json_object *request_body,
    bool auth)
{
    json_request_t *req = malloc(sizeof(json_request_t));
    assert(req != NULL);

    req->http_options = http_options;
    req->options = options;
    req->method = method;
    req->path = path;
    req->body = request_body;
    req->auth = auth;

    return req;
}

static uv_work_t *json_request_work_new(
    storj_env_t *env,
    char *method,
    char *path,
    struct json_object *request_body,
    bool auth)
{
    uv_work_t *work = uv_work_new();
    work->data = json_request_new(env->http_options,
                                  env->bridge_options, method, path,
                                  request_body, auth);

    return work;
}

struct storj_env *storj_init_env(storj_bridge_options_t *options,
                                 storj_encrypt_options_t *encrypt_options,
                                 storj_http_options_t *http_options)
{
    uv_loop_t *loop = malloc(sizeof(uv_loop_t));
    if (uv_loop_init(loop)) {
        return NULL;
    }

    storj_env_t *env = malloc(sizeof(storj_env_t));
    env->bridge_options = options;
    env->loop = loop;
    env->encrypt_options = encrypt_options;
    env->http_options = http_options;

    return env;
}

char *storj_strerror(int error_code)
{
    switch(error_code) {
        case STORJ_BRIDGE_REQUEST_ERROR:
            return "Bridge request error";
        case STORJ_BRIDGE_AUTH_ERROR:
            return "Bridge request authorization error";
        case STORJ_BRIDGE_TOKEN_ERROR:
            return "Bridge request token error";
        case STORJ_BRIDGE_POINTER_ERROR:
            return "Bridge request pointer error";
        case STORJ_BRIDGE_REPOINTER_ERROR:
            return "Bridge request replace pointer error";
        case STORJ_BRIDGE_TIMEOUT_ERROR:
            return "Bridge request timeout error";
        case STORJ_BRIDGE_INTERNAL_ERROR:
            return "Bridge request internal error";
        case STORJ_BRIDGE_RATE_ERROR:
            return "Bridge transfer rate limit error";
        case STORJ_BRIDGE_BUCKET_NOTFOUND_ERROR:
            return "Bucket is not found";
        case STORJ_BRIDGE_FILE_NOTFOUND_ERROR:
            return "File is not found";
        case STORJ_BRIDGE_JSON_ERROR:
            return "Unexpected JSON response";
        case STORJ_FARMER_REQUEST_ERROR:
            return "Farmer request error";
        case STORJ_FARMER_EXHAUSTED_ERROR:
            return "Farmer exhausted error";
        case STORJ_FARMER_TIMEOUT_ERROR:
            return "Farmer request timeout error";
        case STORJ_FARMER_AUTH_ERROR:
            return "Farmer request authorization error";
        case STORJ_FARMER_INTEGRITY_ERROR:
            return "Farmer requst integrity error";
        case STORJ_FILE_INTEGRITY_ERROR:
            return "File integrity error";
        case STORJ_BRIDGE_FRAME_ERROR:
            return "Bridge frame request error";
        case STORJ_FILE_ENCRYPTION_ERROR:
            return "File Encryption error";
        case STORJ_TRANSFER_CANCELLED:
            return "File transfer cancelled";
        case STORJ_TRANSFER_OK:
            return "No errors";
        default:
            return "Unknown error";
    }
}

int storj_bridge_get_info(storj_env_t *env, uv_after_work_cb cb)
{
    uv_work_t *work = json_request_work_new(env,"GET", "/", NULL, false);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_get_buckets(storj_env_t *env, uv_after_work_cb cb)
{
    uv_work_t *work = json_request_work_new(env, "GET", "/buckets", NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_create_bucket(storj_env_t *env, char *name,
                               uv_after_work_cb cb)
{
    struct json_object *body = json_object_new_object();
    json_object *name_string = json_object_new_string(name);

    json_object_object_add(body, "name", name_string);

    uv_work_t *work = json_request_work_new(env, "POST", "/buckets", body, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_delete_bucket(storj_env_t *env, char *id, uv_after_work_cb cb)
{
    char *path = ne_concat("/buckets/", id, NULL);
    uv_work_t *work = json_request_work_new(env, "DELETE", path,
                                            NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_list_files(storj_env_t *env, char *id, uv_after_work_cb cb)
{
    char *path = ne_concat("/buckets/", id, "/files", NULL);
    uv_work_t *work = json_request_work_new(env, "GET", path, NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_create_bucket_token(storj_env_t *env,
                                     char *bucket_id,
                                     storj_bucket_op_t operation,
                                     uv_after_work_cb cb)
{
    struct json_object *body = json_object_new_object();
    json_object *op_string = json_object_new_string(BUCKET_OP[operation]);

    json_object_object_add(body, "operation", op_string);

    char *path = ne_concat("/buckets/", bucket_id, "/tokens", NULL);
    uv_work_t *work = json_request_work_new(env, "POST", path, body, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_get_file_pointers(storj_env_t *env,
                                   char *bucket_id,
                                   char *file_id,
                                   uv_after_work_cb cb)
{
    char *path = ne_concat("/buckets/", bucket_id, "/files/", file_id, NULL);
    uv_work_t *work = json_request_work_new(env, "GET", path, NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_delete_file(storj_env_t *env,
                             char *bucket_id,
                             char *file_id,
                             uv_after_work_cb cb)
{
    char *path = ne_concat("/buckets/", bucket_id, "/files/", file_id, NULL);
    uv_work_t *work = json_request_work_new(env, "DELETE", path, NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_create_frame(storj_env_t *env, uv_after_work_cb cb)
{
    uv_work_t *work = json_request_work_new(env, "POST", "/frames", NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_get_frames(storj_env_t *env, uv_after_work_cb cb)
{
    uv_work_t *work = json_request_work_new(env, "GET", "/frames", NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_get_frame(storj_env_t *env,
                           char *frame_id,
                           uv_after_work_cb cb)
{
    char *path = ne_concat("/frames/", frame_id, NULL);
    uv_work_t *work = json_request_work_new(env, "GET", path, NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);

}

int storj_bridge_delete_frame(storj_env_t *env, char *frame_id,
                              uv_after_work_cb cb)
{
    char *path = ne_concat("/frames/", frame_id, NULL);
    uv_work_t *work = json_request_work_new(env, "DELETE", path, NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}

int storj_bridge_get_file_info(storj_env_t *env,
                               char *bucket_id,
                               char *file_id,
                               uv_after_work_cb cb)
{
    char *path = ne_concat("/buckets/", bucket_id, "/files/",
                           file_id, "/info", NULL);

    uv_work_t *work = json_request_work_new(env, "GET", path, NULL, true);

    return uv_queue_work(env->loop, (uv_work_t*) work, json_request_worker, cb);
}
