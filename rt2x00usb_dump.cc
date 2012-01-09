/*
 * Copyright (C) 2011 Stanislaw Gruszka <sgruszka@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Based on usbdump by Bert Vermeulen
 * Copyright (C) 2010 Bert Vermeulen <bert@biot.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>

#include <linux/types.h>

#include <map>

#define SYSBASE		"/sys/bus/usb/devices"
#define USBMON_DEVICE	"/dev/usbmon"
#define MAX_PACKETS	32

#define USB_DIR_IN	0x80
#define LINEBUF_LEN	16383

/* this was taken from usbmon.txt in the kernel documentation */
#define SETUP_LEN 8
struct usbmon_packet {
	uint64_t id;			/* 0: URB ID - from submission to callback */
	unsigned char type;		/* 8: Same as text; extensible. */
	unsigned char xfer_type;/* ISO (0), Intr, Control, Bulk (3) */
	unsigned char epnum;	/* Endpoint number and transfer direction */
	unsigned char devnum;	/* Device address */
	uint16_t busnum;		/* 12: Bus number */
	char flag_setup;		/* 14: Same as text */
	char flag_data;			/* 15: Same as text; Binary zero is OK. */
	int64_t ts_sec;			/* 16: gettimeofday */
	int32_t ts_usec;		/* 24: gettimeofday */
	int status;				/* 28: */
	unsigned int length;	/* 32: Length of data (submitted or actual) */
	unsigned int len_cap;	/* 36: Delivered length */
	union {					/* 40: */
		unsigned char setup[SETUP_LEN]; /* Only for Control S-type */
		struct iso_rec {	/* Only for ISO */
			int error_count;
			int numdesc;
		} iso;
	} s;
	int interval;			/* 48: Only for Interrupt and ISO */
	int start_frame;		/* 52: For ISO */
	unsigned int xfer_flags;/* 56: copy of URB's transfer_flags */
	unsigned int ndesc;		/* 60: Actual number of ISO descriptors */
};							/* 64 total length */

struct mon_mfetch_arg {
    uint32_t *offvec;		/* Vector of events fetched */
    uint32_t nfetch;		/* Number of events to fetch (out: fetched) */
    uint32_t nflush;		/* Number of events to flush */
};

#define MON_IOC_MAGIC		0x92
#define MON_IOCX_MFETCH		_IOWR(MON_IOC_MAGIC, 7, struct mon_mfetch_arg)
#define MON_IOCQ_URB_LEN	_IO(MON_IOC_MAGIC, 1)
#define MON_IOCQ_RING_SIZE	_IO(MON_IOC_MAGIC, 5)
#define MON_IOCH_MFLUSH		_IO(MON_IOC_MAGIC, 8)

#define XFER_TYPE_ISO		0
#define XFER_TYPE_INTERRUPT	1
#define XFER_TYPE_CONTROL	2
#define XFER_TYPE_BULK		3

struct usb_ctrlrequest {
        __u8 bRequestType;
        __u8 bRequest;
        __le16 wValue;
        __le16 wIndex;
        __le16 wLength;
} __attribute__ ((packed));

static inline bool is_read_cr(struct usb_ctrlrequest *cr)
{
	return (cr->bRequestType & 0x80);
}

#include "registers.cc"

unsigned char *get_data(struct usbmon_packet *hdr)
{
	return reinterpret_cast<unsigned char *>(hdr) + sizeof(struct usbmon_packet);
}

uint32_t decode_reg_val(unsigned char *buf)
{
	// Little endian
	return buf[3] << 24 | buf[2] << 16 | buf [1] << 8 | buf[0];
}

uint32_t get_reg_val(struct usbmon_packet *hdr, int nr = 0)
{
	unsigned char *buf = get_data(hdr);
	buf += nr * sizeof(uint32_t);
	return decode_reg_val(buf);
}

void print_reg_content(struct reg *reg, uint32_t val, int32_t include = 0xffffffff)
{
	for (int i = reg->n_fields - 1; i >= 0; i--) {
		struct field *f = &reg->fields[i];
		uint32_t mask = (0xffffffff << f->first) & (0xffffffff >> (31 - f->last));

		if (mask & include)
			printf(" %s: 0x%x", f->name, (val & mask) >> f->first);
	}	
}

void print_buf_reg(struct reg *reg, unsigned char *buf)
{
	uint32_t val = decode_reg_val(buf);

	printf("\t[%s:", reg->name);
	print_reg_content(reg, val);
	printf("]\n");
}

enum Content { Full, UpperHalf, LowerHalf };

void print_reg(struct reg *reg, uint32_t val, bool read, Content content)
{
	const char *dir1 = read ? "<-" : "->";
	const char *dir2 = read ? "[READ :" : "[WRITE:";
	uint32_t include;

	switch (content) {
	case Full:
		printf("0x%08x %s %s\t %s", val, dir1, reg->name, dir2);
		include = 0xffffffff;
		break;
	case UpperHalf:
		printf("0x%04x %s %s (16 MSB)\t %s", val, dir1, reg->name, dir2);
		include = 0xffff0000;
		val <<= 16; // Tweak to do not break fields matching
		break;
	case LowerHalf:
		printf("0x%04x %s %s (16 LSB)\t %s", val, dir1, reg->name, dir2);
		include = 0x0000ffff;
		break;
	}	

	print_reg_content(reg, val, include);
	printf("]\n");
}

void process_bbp_register_rw(struct usb_ctrlrequest *cr, struct usbmon_packet *shdr, struct usbmon_packet *hdr)
{

}

void process_register_rw(struct usb_ctrlrequest *cr, struct usbmon_packet *shdr, struct usbmon_packet *hdr)
{
	struct reg *reg = get_reg(cr->wIndex);
	// We can write or read halves of two consequitive registers at once
	struct reg *reg1 = get_reg(cr->wIndex - 2);
	struct reg *reg2 = get_reg(cr->wIndex + 2);

	if (is_read_cr(cr)) {
		// Read
		assert(shdr->len_cap == 0);

		if (hdr->len_cap != 4) {
			printf("CTRL: READ %d BYTES FROM REGISTER 0x04%x\n", hdr->len_cap, cr->wIndex);
			return; 
		}
	
		uint32_t reg_val = get_reg_val(hdr);

		if (reg)
			print_reg(reg, reg_val, true, Full);
		else {
			if (reg1 && reg2) {
				print_reg(reg1, reg_val & 0xffff, true, UpperHalf);
				print_reg(reg2, reg_val >> 16, true, LowerHalf);
			 } else {
				// Unknown register
				printf("0x%08x <- REG 0x%04x\n", reg_val, cr->wIndex);
			}
		}
	} else {
		// Write
		assert(hdr->len_cap == 0);
		
		if (shdr->len_cap != 4 && shdr->len_cap != 0) {
			printf("CTRL: WRITE %d BYTES TO REGISTER 0x%04x\n", shdr->len_cap, cr->wIndex);
			return;
		}

		if (shdr->len_cap == 4) {
			uint32_t reg_val = get_reg_val(shdr);

			if (reg)
				print_reg(reg, reg_val, false, Full);
			else {
				if (reg1 && reg2) {
					print_reg(reg1, reg_val & 0xffff, false, UpperHalf);
					print_reg(reg2, reg_val >> 16, false, LowerHalf);
				} else {
					// Unknown register
					printf("0x%08x -> REG 0x%04x\n", reg_val, cr->wIndex);
				}
			}
		} else if (shdr->len_cap == 0) {
			// Using wValue as data
			if (reg)
				print_reg(reg, cr->wValue, false, LowerHalf);
			else if (reg1)
				print_reg(reg1, cr->wValue, false, UpperHalf);
			else 
				printf("0x%04x -> REG 0x%04x\n", cr->wValue, cr->wIndex);
		}
	}

}

void process_control_packet(struct usbmon_packet *shdr, struct usbmon_packet *hdr)
{
	struct usb_ctrlrequest *cr = reinterpret_cast<struct usb_ctrlrequest *>(shdr->s.setup);

	if (!(cr->bRequestType & 0x40)) {
		// Not vendor request
		// FIXME: print data and length
		printf("CTRL: %02x %02x Value: %04x Index %04x Lenght %04x\n",
		       cr->bRequestType, cr->bRequest, cr->wValue, cr->wIndex, cr->wLength);
		return;
	}

	// FIXME: check urb statuses
	
	if (cr->wIndex > 0x17ff) {
		// Not registers area
		struct area *area = get_area(cr->wIndex);
		const char *name = area ? area->name : "Unknown area";

		// FIXME: len_cap == 0
		if (is_read_cr(cr))
			// Read
			printf("CTRL: READ %d BYTES FROM 0x%04x (%s)\n", hdr->len_cap, cr->wIndex, name);
		else
			// Write
			printf("CTRL: WRITE %d BYTES TO 0x%04x (%s)\n", shdr->len_cap, cr->wIndex, name);

		return;
	}

	if (cr->wIndex == 0x101c || cr->wIndex == 0x101e)
		// BBP registers are threated specially
		process_bbp_register_rw(cr, shdr, hdr);
	else 
		process_register_rw(cr, shdr, hdr);
}

void print_rxinfo(unsigned char *buf, int len)
{
	int frame_nr = 0;

	while (len > 24) { // FIXME: why 24
		uint32_t rxinfo_val = decode_reg_val(buf);
		int frame_len = rxinfo_val & 0xffff;

		printf("  READ FRAME%d (%d BYTES)\n", frame_nr++, frame_len);

		print_buf_reg(&rxinfo, buf + 0);
		print_buf_reg(&rxwi_w0, buf + 4);
		print_buf_reg(&rxwi_w1, buf + 8);
		print_buf_reg(&rxwi_w2, buf + 12);
		print_buf_reg(&rxwi_w3, buf + 16);

		frame_len += 4; // RXINFO size
		assert(frame_len <= len - 4);

		print_buf_reg(&rxd, buf + frame_len);

		frame_len += 4; // RXD size
		len -= frame_len;
		buf += frame_len;
	}
}
void print_txinfo(unsigned char *buf, int len)
{
	int frame_nr = 0;

	while (len > 20) { // FIXME: why 16
		uint32_t txinfo_val = decode_reg_val(buf);
		int frame_len = txinfo_val & 0xffff;

		printf("   WRITE FRAME%d (%d BYTES)\n", frame_nr++, frame_len);

		print_buf_reg(&txinfo, buf);
		print_buf_reg(&txwi_w0, buf + 4);
		print_buf_reg(&txwi_w1, buf + 8);

		frame_len += 4; // TXINFO size
		len -= frame_len;
		buf += frame_len;
	}
}

void process_bulk_packet(struct usbmon_packet *shdr, struct usbmon_packet *hdr)
{
	const int ep = hdr->epnum & 0x7f;

	if (hdr->epnum & USB_DIR_IN) {
		// Read
		unsigned char *buf = get_data(hdr);
		const int len = hdr->len_cap;

		printf("BULK%d <- READ %d BYTES\n", ep, len);

		if (0) {
			for (int i = 0; i < len; i++)
				printf("%02x ", buf[i]);
			printf("\n");
		}
		
		print_rxinfo(buf, len);
	} else {
		// Write
		unsigned char *buf = get_data(shdr);
		const int len = shdr->len_cap;

		printf("BULK%d -> WRITE %d BYTES\n", ep, len);

		if (0) {	
			for (int i = 0; i < len; i++)
				printf("%02x ", buf[i]);
			printf("\n");
		}

		print_txinfo(buf, len);	
	}
}

void process_packet(struct usbmon_packet *hdr)
{
	static std::map<uint64_t, char *> pkts_map;

	if (hdr->type == 'S') {
		const int len = sizeof(struct usbmon_packet) + hdr->len_cap;
		char *tmp = new char[len];
		memcpy(tmp, hdr, len);
		pkts_map[hdr->id] = tmp;
		return;
	}

	assert(hdr->type == 'C');

	std::map<uint64_t, char *>::iterator it = pkts_map.find(hdr->id);
	if (it == pkts_map.end()) // not yet mapped
		return;
	char *buf = it->second;
	struct usbmon_packet *shdr = reinterpret_cast<struct usbmon_packet *>(buf);

	assert(shdr->id == hdr->id);
	assert(shdr->epnum == hdr->epnum);
	assert(shdr->xfer_type == hdr->xfer_type);

	if (shdr->xfer_type == 2)
		process_control_packet(shdr, hdr);
	else if (shdr->xfer_type == 3)
		process_bulk_packet(shdr, hdr);
	else {
		const char *xfer_name[] = {
			" iso",
			"intr",
			"ctrl",
			"bulk"
		};
		const char *dir = (hdr->epnum & USB_DIR_IN) ? "<-" : "->";
		const int ep = hdr->epnum & 0x7f;

		printf("%p %s %s %d\n", (void *) shdr->id, xfer_name[shdr->xfer_type], dir, ep);
	}

	delete[] buf;
	pkts_map.erase(it);

	return;
}

void sniff(int bus, int address)
{
	struct mon_mfetch_arg mfetch;
	struct usbmon_packet *hdr;
	int kbuf_len, fd, nflush;
	char *mbuf, path[64];
	uint32_t vec[MAX_PACKETS];

	snprintf(path, 63, "%s%d", USBMON_DEVICE, bus);
	if ((fd = open(path, O_RDONLY)) == -1) {
		printf("unable to open %s: %s\n", path, strerror(errno));
		return;
	}

	if ((kbuf_len = ioctl(fd, MON_IOCQ_RING_SIZE)) <= 0) {
		printf("failed to determine kernel USB buffer size: %s\n", strerror(errno));
		return;
	}

	mbuf = static_cast<char* >(mmap(NULL, kbuf_len, PROT_READ, MAP_SHARED, fd, 0));
	if (mbuf == MAP_FAILED) {
		printf("unable to mmap %d bytes: %s\n", kbuf_len, strerror(errno));
		return;
	}

	nflush = 0;
	while (1) {
		mfetch.offvec = vec;
		mfetch.nfetch = MAX_PACKETS;
		mfetch.nflush = nflush;
		ioctl(fd, MON_IOCX_MFETCH, &mfetch);
		nflush = mfetch.nfetch;
		for (unsigned int i = 0; i < mfetch.nfetch; i++) {
			hdr = (struct usbmon_packet *) &mbuf[vec[i]];
			if (hdr->type == '@')
				/* filler packet */
				continue;
			if (hdr->busnum != bus || hdr->devnum != address)
				/* some other device */
				continue;
			process_packet(hdr);
		}
	}

	munmap(mbuf, kbuf_len);
	ioctl(fd, MON_IOCH_MFLUSH, mfetch.nfetch);
	close(fd);
}

int check_device(struct dirent *de, char *vidpid, int *bus, int *address)
{
	int fd;
	char path[128], vid[5], buf[5];

	/* vendor */
	memcpy(vid, vidpid, 4);
	vid[4] = '\0';
	sprintf(path, "%s/%s/idVendor", SYSBASE, de->d_name);
	if ((fd = open(path, O_RDONLY)) == -1)
		return 0;
	buf[4] = '\0';
	if ((read(fd, buf, 4) != 4) || strncmp(vid, buf, 4)) {
		close(fd);
		return 0;
	}

	/* product */
	sprintf(path, "%s/%s/idProduct", SYSBASE, de->d_name);
	if ((fd = open(path, O_RDONLY)) == -1)
		return 0;
	buf[4] = '\0';
	if ((read(fd, buf, 4) != 4) || strncmp(vidpid+5, buf, 4)) {
		close(fd);
		return 0;
	}

	/* bus */
	sprintf(path, "%s/%s/busnum", SYSBASE, de->d_name);
	if ((fd = open(path, O_RDONLY)) == -1)
		return 0;
	memset(buf, 0, 5);
	read(fd, buf, 4);
	*bus = strtol(buf, NULL, 10);

	/* address */
	sprintf(path, "%s/%s/devnum", SYSBASE, de->d_name);
	if ((fd = open(path, O_RDONLY)) == -1)
		return 0;
	memset(buf, 0, 5);
	read(fd, buf, 4);
	*address = strtol(buf, NULL, 10);

	return 1;
}

int find_device(char *vidpid, int *bus, int *address)
{
	DIR *dir;
	struct dirent *de;
	int found;

	if (!(dir = opendir(SYSBASE)))
		return 0;

	found = 0;
	while ((de = readdir(dir))) {
		if (check_device(de, vidpid, bus, address)) {
			found = 1;
			break;
		}
	}
	closedir(dir);

	return found;
}

void usage(void)
{
	printf("usage: rt2x00_usbdump [-d <vid:pid>]\n");
}

int main(int argc, char **argv)
{
	int opt, bus, address;
	char *device;

	// FIXME: device autorecognize
	device = NULL;
	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
		case 'd':
			if (strlen(optarg) != 9 || strspn(optarg, "01234567890abcdef:") != 9)
				printf("invalid format for device id (aaaa:bbbb)\n");
			else
				device = optarg;
			break;
		}
	}

	if (!device) {
		usage();
		return 1;
	}

	if (find_device(device, &bus, &address))
		sniff(bus, address);
	return 0;
}

