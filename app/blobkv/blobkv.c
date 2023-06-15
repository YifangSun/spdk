
#include "spdk/event.h"
#include "spdk/vhost.h"
#include "spdk/bdev.h"
#include "spdk/thread.h"

static char *g_bdev_name = "Malloc0";

struct hello_context_t {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;
	char *buff;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
};

static void
hello_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}

static void
hello_start(void *arg1)
{
	struct hello_context_t *hello_context = arg1;
	uint32_t blk_size, buf_align;
	int rc = 0;
	hello_context->bdev = NULL;
	hello_context->bdev_desc = NULL;

	SPDK_NOTICELOG("Successfully started the application\n");

	/*
	 * There can be many bdevs configured, but this application will only use
	 * the one input by the user at runtime.
	 *
	 * Open the bdev by calling spdk_bdev_open_ext() with its name.
	 * The function will return a descriptor
	 */
	SPDK_NOTICELOG("Opening the bdev %s\n", hello_context->bdev_name);
	rc = spdk_bdev_open_ext(hello_context->bdev_name, true, hello_bdev_event_cb, NULL,
				&hello_context->bdev_desc);
	if (rc) {
		SPDK_ERRLOG("Could not open bdev: %s\n", hello_context->bdev_name);
		spdk_app_stop(-1);
		return;
	}

	/* A bdev pointer is valid while the bdev is opened. */
	hello_context->bdev = spdk_bdev_desc_get_bdev(hello_context->bdev_desc);


	SPDK_NOTICELOG("Opening io channel\n");
	/* Open I/O channel */
	hello_context->bdev_io_channel = spdk_bdev_get_io_channel(hello_context->bdev_desc);
	if (hello_context->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	/* Allocate memory for the write buffer.
	 * Initialize the write buffer with the string "Hello World!"
	 */
	blk_size = spdk_bdev_get_block_size(hello_context->bdev);
	buf_align = spdk_bdev_get_buf_align(hello_context->bdev);

	snprintf(hello_context->buff, blk_size, "%s", "Hello World!\n");


	// hello_write(hello_context);
  // spdk_put_io_channel(hello_context->bdev_io_channel);
  // spdk_bdev_close(hello_context->bdev_desc);
  // spdk_app_stop(-1);
  // return;
}

int
main(int argc, char *argv[])
{
	struct spdk_app_opts opts = {};
	int rc;
  struct hello_context_t hello_context = {};

	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "vhost";

  if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, NULL, NULL)) 
      != SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}
  hello_context.bdev_name = g_bdev_name;

	/* Blocks until the application is exiting */
	rc = spdk_app_start(&opts, hello_start, &hello_context);

  // spdk_dma_free(hello_context.buff);
	spdk_app_fini();
	return rc;
}