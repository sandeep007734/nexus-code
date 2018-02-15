#include "internal.h"

// -------------------------- utilities -----------------------

void *
ocall_calloc(size_t size)
{
    void * ptr = calloc(1, size);
    if (ptr == NULL) {
        log_error("allocation error");
    }

    return ptr;
}

void
ocall_free(void * ptr)
{
    free(ptr);
}

void
ocall_print(char * str)
{
    printf("%s", str);
    fflush(stdout);
}



// ------------------- Buffer Management ---------------------------

uint8_t *
ocall_buffer_alloc(size_t size, struct nexus_uuid * uuid, void * backend_info)
{
    struct sgx_backend * sgx_backend = (struct sgx_backend *)backend_info;

    return buffer_manager_alloc(sgx_backend->buf_manager, size, uuid);
}

uint8_t *
ocall_buffer_get(struct nexus_uuid * uuid, size_t * p_buffer_size, void * backend_info)
{
    struct sgx_backend * sgx_backend = (struct sgx_backend *)backend_info;

    return buffer_manager_get(sgx_backend->buf_manager, uuid, p_buffer_size);
}

void
ocall_buffer_put(struct nexus_uuid * uuid, void * backend_info)
{
    struct sgx_backend * sgx_backend = (struct sgx_backend *)backend_info;

    buffer_manager_put(sgx_backend->buf_manager, uuid);
}


// ---------------- metadata management -----------------------

int
ocall_metadata_get(struct nexus_uuid      * metadata_uuid,
                   struct nexus_uuid_path * uuid_path,
                   void                   * backend_info)
{
    struct sgx_backend * sgx_backend = NULL;

    uint8_t * buffer_addr = NULL;
    size_t    buffer_size = NULL;

    int ret = -1;


    sgx_backend = (struct sgx_backend *)backend_info;

    // TODO add code for checking buffer_manager entry here

    ret = nexus_datastore_get_uuid(sgx_backend->volume->metadata_store,
                                   metadata_uuid,
                                   NULL,
                                   &buffer_addr,
                                   (uint32_t *)&buffer_size);

    if (ret) {
        log_error("nexus_datastore_get_uuid FAILED\n");
        return -1;
    }

    ret = buffer_manager_add(sgx_backend->buf_manager, buffer_addr, buffer_size, metadata_uuid);
    if (ret != 0) {
        nexus_free(buffer_addr);
        log_error("buffer_manager_add FAILED\n");
        return -1;
    }

    return 0;
}

int
ocall_metadata_set(struct nexus_uuid      * metadata_uuid,
                   struct nexus_uuid_path * uuid_path,
                   void                   * backend_info)
{
    struct sgx_backend * sgx_backend = NULL;

    uint8_t * buffer_addr = NULL;
    size_t    buffer_size = NULL;

    int ret = -1;


    sgx_backend = (struct sgx_backend *)backend_info;

    buffer_addr = buffer_manager_get(sgx_backend->buf_manager, metadata_uuid, &buffer_size);

    if (buffer_addr == NULL) {
        log_error("buffer_manager_get returned NULL\n");
        return -1;
    }

    ret = nexus_datastore_put_uuid(sgx_backend->volume->metadata_store,
                                   metadata_uuid,
                                   NULL,
                                   buffer_addr,
                                   buffer_size);

    if (ret) {
        log_error("nexus_datastore_put_uuid FAILED\n");
        return -1;
    }

    return 0;
}
