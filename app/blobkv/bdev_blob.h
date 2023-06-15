#ifndef BDEVKV_H
#define BDEVKV_H

#include "spdk/bdev.h"
#include "spdk/blob_bdev.h"
#include "spdk/bdev_module.h"
#include "spdk/uuid.h"


#define BDEV_BLOB_NAME_MAX	64

/* Default size of blobstore cluster */
#define BDEV_BLOB_OPTS_CLUSTER_SZ (4 * 1024 * 1024)

/* UUID + '_' + blobid (20 characters for uint64_t).
 * Null terminator is already included in SPDK_UUID_STRING_LEN. */
#define BDEV_BLOB_UNIQUE_ID_MAX (SPDK_UUID_STRING_LEN + 1 + 20)

typedef void (*spdk_delete_bdev_blob_complete)(void *cb_arg, int bdeverrno);

struct bdev_blob {
	struct spdk_blob_store  *bs;
	struct spdk_blob		*blob;
	spdk_blob_id			blob_id;
	char				unique_id[SPDK_LVOL_UNIQUE_ID_MAX];
	char				name[SPDK_LVOL_NAME_MAX];
	struct spdk_uuid		uuid;
	char				uuid_str[SPDK_UUID_STRING_LEN];
	bool				thin_provision;
	struct spdk_bdev		*bdev;
	int				ref_count;
	bool				action_in_progress;
	enum blob_clear_method		clear_method;
	TAILQ_ENTRY(bdev_blob) link;
};

int bdev_blob_create(struct spdk_bdev **bdev, const char *name,
		uint64_t pool_id,
		const char *image_name,
		uint64_t image_size,
		uint32_t block_size,
        uint64_t object_size,
		const char *etcd_address);

void bdev_blob_delete(struct spdk_bdev *bdev, spdk_delete_bdev_blob_complete cb_fn,
		     void *cb_arg);

int bdev_blob_resize(struct spdk_bdev *bdev, const uint64_t new_size_in_mb);

extern struct spdk_bdev_module bdev_blob_if;

#endif /* BDEVKV_H */
