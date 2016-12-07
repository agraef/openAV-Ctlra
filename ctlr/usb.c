#include <errno.h>
#include <stdio.h>

#include "ctlr.h"
#include "devices.h"
#include "devices/device_impl.h"

static int ctlr_libusb_initialized;

int ctlr_dev_impl_usb_open(int vid, int pid, struct ctlr_dev_t *ctlr_dev,
			   uint32_t num_skip)
{
	int ret;

	if(!ctlr_libusb_initialized) {
		ret = libusb_init (NULL);
		if (ret < 0) {
			printf("failed to initialise libusb: %s\n", libusb_error_name(ret));
			return -EINVAL;
		}
	}

	libusb_device **devs;
	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8];

	int cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return 0;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			printf("failed to get device descriptor");
			return -1;
		}
#if 0
		printf("%04x:%04x (bus %d, device %d)",
		       desc.idVendor, desc.idProduct,
		       libusb_get_bus_number(dev),
		       libusb_get_device_address(dev));

		r = libusb_get_port_numbers(dev, path, sizeof(path));
		if (r > 0) {
			printf(" path: %d", path[0]);
			for (j = 1; j < r; j++)
				printf(".%d", path[j]);
		}
		printf("\n");
#endif

		if(desc.idVendor == vid &&
		    desc.idProduct == pid) {
			break;
		}
	}

	libusb_free_device_list(devs, 1);

	/* now that we've found the device, open the handle */
	ret = libusb_open(dev, &ctlr_dev->usb_handle);
	if(ret != LIBUSB_SUCCESS) {
		printf("Error in claiming interface\n");
		goto fail;
	}
	printf("got device OK\n");
	ctlr_dev->usb_device = dev;
	return 0;
fail:
	return -ENODEV;
}
