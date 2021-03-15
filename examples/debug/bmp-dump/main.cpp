// Dump all bytes coming from a Black Magic Probe's "Trace Capture" endpoint.
// This reads "bulk" data packets from USB, and writes binary data to stdout.
// Inspired by Nick Downing's "bmp_traceswo", which filters on ITM channel 0.

#include <cstdio>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

constexpr auto VID = 0x1D50, PID = 0x6018;

libusb_device** devs;
libusb_device_handle* handle;

auto bmpScan () {
    struct libusb_device_descriptor desc;

    for (int i = 0; devs[i] != nullptr; ++i) {
        if (int e = libusb_get_device_descriptor(devs[i], &desc); e < 0)
            return e;

        // fprintf(stderr, "%04x:%04x @ bus %2d dev %2d\n",
        //                     desc.idVendor, desc.idProduct,
        //                     libusb_get_bus_number(devs[i]),
        //                     libusb_get_device_address(devs[i]));

        if (desc.idVendor == VID && desc.idProduct == PID) {
            if (auto e = libusb_open(devs[i], &handle); e < 0)
                return e;

            if (auto e = libusb_claim_interface(handle, 5); e < 0)
                return e;

            return i; // >= 0
        }
    }

    return (int) LIBUSB_ERROR_NO_DEVICE;
}

auto bmpCapture () {
    unsigned char buf [64];
    int num;

    while (true) {
        auto e = libusb_bulk_transfer(handle, 0x85, buf, sizeof (buf), &num, 0);
        if (e < 0)
            return e;
        write(1, buf, num); // ignore write errors
    }
}

int main () {
    fprintf(stderr, "[BMP] Looking for device\n");

    int e = libusb_init(0);
    if (e >= 0)
        while (true) {
            e = libusb_get_device_list(0, &devs);
            if (e < 0)
                break;

            e = bmpScan();

            libusb_free_device_list(devs, 1);

            if (e >= 0) {
                fprintf(stderr, "[BMP] Connected\n");
                e = bmpCapture();
                fprintf(stderr, "[BMP] Connection lost\n");
            }

            if (e < 0 && e != LIBUSB_ERROR_NO_DEVICE)
                break;

            sleep(1);
        }

    fprintf(stderr, "[BMP] %s\n", libusb_strerror((enum libusb_error) e));
    return 1;
}
