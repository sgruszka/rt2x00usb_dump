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

void print_data(struct usbmon_packet *hdr)
{
	unsigned char *data = get_data(hdr);

	printf(" [DATA:");
	for (unsigned int i = 0; i < hdr->len_cap; i++)
		printf(" %02x", data[i]);
	printf("]\n");
}

uint32_t get_reg_val(struct usbmon_packet *hdr, int nr = 0)
{
	unsigned char *buf = get_data(hdr);
	buf += nr * sizeof(uint32_t);
	return decode_reg_val(buf);
}

void process_special_register_rw(struct usb_ctrlrequest *cr, struct usbmon_packet *shdr, struct usbmon_packet *hdr, struct special_reg *reg)
{
	const uint32_t DATA_MASK = 0x000000ff;
	const uint32_t ADDR_MASK = reg->ADDR_MASK;
	const uint32_t RW_BIT	 = 0x00010000; // 0 - Write, 1 - Read
	const uint32_t KICK_BIT	 = 0x00020000;

	if (is_read_cr(cr)) {
		// Read
		assert(hdr->len_cap == 4);
		uint32_t reg_val = get_reg_val(hdr);

		if (reg->state == KICK_READ) {
			bool busy = reg_val & KICK_BIT;
			if (busy)
				return;

#if 0
			assert(reg->cur_addr == ((reg_val & ADDR_MASK) >> 8));
#else
			if (reg->cur_addr != ((reg_val & ADDR_MASK) >> 8))
				printf("WARN cur_addr %02x addr %02x reg_val %08x\n", reg->cur_addr, (reg_val & ADDR_MASK) >> 8, reg_val);
#endif
			reg->cur_data = reg_val & DATA_MASK;

			print_special_reg(reg, true);

			reg->state = CHECKING_STATUS;
		} else
			assert(reg->state == CHECKING_STATUS);
	} else {
		// Write
		if (shdr->len_cap == 4) {
			uint32_t reg_val = get_reg_val(shdr);

			reg->cur_data = reg_val & DATA_MASK;
			reg->cur_addr = (reg_val & ADDR_MASK) >> 8;

			bool do_read = reg_val & RW_BIT;
			if (reg->write_is_1)
				do_read = !do_read;
			bool do_kick = reg_val & KICK_BIT;

			assert(do_kick == true);

			if (do_read)
				reg->state = KICK_READ;
			else {
				print_special_reg(reg, false);
				reg->state = CHECKING_STATUS;
			}
		} else if (shdr->len_cap == 0) {
			if (cr->wIndex == reg->addr) {
				reg->cur_data = cr->wValue & DATA_MASK;
				reg->cur_addr = (cr->wValue & ADDR_MASK) >> 8;

				reg->state = SET_ADDR_DATA;
			} else {
#if 0
				assert(reg->state == SET_ADDR_DATA);
#else
				if (reg->state != SET_ADDR_DATA) {
					reg->state = CHECKING_STATUS;
					return;
				}
#endif

				// UpperHalf (cr->wIndex == 0x101e)
				bool do_read = cr->wValue & (RW_BIT >> 16);
				if (reg->write_is_1)
					do_read = !do_read;
				bool do_kick = cr->wValue & (KICK_BIT >> 16);

				assert(do_kick == true);

				if (do_read == false) {
					print_special_reg(reg, false);
					reg->state = CHECKING_STATUS;
				} else
					reg->state = KICK_READ;
			}
		} else
			assert(false);
	}
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
			print_data(hdr);
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
			print_data(shdr);
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

static bool process_mcu_request(struct usb_ctrlrequest *cr, struct usbmon_packet *shdr, struct usbmon_packet *hdr)
{
	const uint16_t H2M_MAILBOX_CSR = 0x7010;
	const uint32_t OWNER_BIT = 0x00000001;
	const uint32_t HOST_CMD = 0x404;

	static int state = 0;
	static uint8_t command;
	static uint8_t owner;
	static uint8_t token;
	static uint8_t arg0;
	static uint8_t arg1;

	switch (state) {
	case 0:
		// Check busy or not
		if (cr->wIndex == H2M_MAILBOX_CSR) {
			if (is_read_cr(cr)) {
				assert(hdr->len_cap == 4);

				uint32_t reg_val = get_reg_val(hdr);

				if (!(reg_val & OWNER_BIT)) // not busy
					state = 1;
			} else {
				// For some MCU command driver skip "check busy" part
				state = 1;
				goto state_1;
			}
		} else {
			return false;
		}
		break;
	case 1:
state_1:
		if (cr->wIndex != H2M_MAILBOX_CSR)
			return false;

		if (is_read_cr(cr))
			goto stop_processing;

		if (hdr->len_cap == 4) {
			uint32_t reg_val = get_reg_val(hdr);
			arg0 = reg_val & 0x000000ff;
			arg1 = (reg_val & 0x0000ff00) >> 8;
			token = (reg_val & 0x00ff0000) >> 16;
			owner = (reg_val & 0xff000000) >> 24;
			state = 3;
		} else {
			// Write 16 LSB to MAILBOX_CSR
			assert(cr->wIndex == H2M_MAILBOX_CSR);
			arg0 = cr->wValue & 0x00ff;
			arg1 = (cr->wValue & 0xff00) >> 8;
			state = 2;
		}
		break;
	case 2:
		// Write 16 MSB to MAILBOX_CSR
		if (cr->wIndex != H2M_MAILBOX_CSR + 2)
			goto stop_processing;
		assert(!is_read_cr(cr));

		token = cr->wValue & 0x00ff;
		owner = (cr->wValue & 0xff00) >> 8;
		state = 3;
		break;
	case 3:
		if (cr->wIndex != HOST_CMD)
			goto stop_processing;
		assert(!is_read_cr(cr));

		if (hdr->len_cap == 4) {
			uint32_t reg_val = get_reg_val(hdr);
			command = reg_val & 0xff;
			goto print;
		} else {
			command = cr->wValue & 0xff;
			state = 4;
		}
		break;
	case 4:
		if (cr->wIndex != HOST_CMD + 2)
			goto stop_processing;
		assert(!is_read_cr(cr));
print:
		printf("MCU COMMAND %02x Token %02x arg0 %02x arg1 %02x\n", command, token, arg0, arg1);
		state = 0;
		break;
	}

	// Packet was processed here
	return true;

stop_processing:
	printf("Warning: fail to parse MCU command at state %d\n", state);
	state = 0;
	return false;
}

static bool process_h2m_bbp(struct usb_ctrlrequest *cr, struct usbmon_packet *shdr, struct usbmon_packet *hdr)
{
	const uint16_t H2M_BBP_AGENT = 0x7028;
	const uint16_t H2M_MAILBOX_CSR = 0x7010;
	const uint32_t KICK_BIT	 = 0x00020000;

	static int state = 0;
	static bool is_read;
	static uint8_t cur_addr;
	static uint8_t cur_data;

	switch (state) {
	case 0:
		// Check busy or not H2M_BBP_AGENT processing
		if (is_read_cr(cr) && cr->wIndex == H2M_BBP_AGENT) {
			assert(hdr->len_cap == 4);

			uint32_t reg_val = get_reg_val(hdr);

			if (!(reg_val & KICK_BIT)) // not busy
				state = 1;
		} else
			return false;
		break;
	case 1:
		if (cr->wIndex != 0x7028)
			return false;

		if (!is_read_cr(cr))
			break;
		// Write 16 LSB to H2
		assert(cr->wIndex == 0x7028);
		cur_addr = (cr->wValue & 0xff00) >> 8;
		cur_data = cr->wValue & 0x00ff;
		state = 2;
		break;
	case 2:
		if (cr->wIndex != 0x702a)
			return false;

		assert(!is_read_cr(cr));
		is_read = (cr->wValue & 0x1) ? true : false;
		state = 3;
		break;
	case 3:
		// Ignore most of MCU processing
#if 0
		if (cr->wIndex == 0x7010) && is_read_cr(cr))
			break;
		if (cr->wIndex == 0x7010 && cr->wValue == 0x0000)
			break;
		if (cr->wIndex == 0x7012 && cr->wValue == 0x0000)
			break;
		if (cr->wIndex == 0x0404 && cr->wValue == 0x0080)
			break;
#endif
		if (cr->wIndex == 0x7010 || cr->wIndex == 0x7012 || cr->wIndex == 0x404)
			break;

		// End of MCU processing
		if (cr->wIndex == 0x0406 && cr->wValue == 0x0000) {
			if (is_read)
				state = 4;
			else {
				printf("0x%02x -> BBP REG%u\t[WRITE]\n", cur_data, cur_addr);
				state = 0;
			}
		} else
			return false;

		break;
	case 4:
		if (cr->wIndex != H2M_BBP_AGENT)
			return false;

		assert(is_read_cr(cr));
		assert(hdr->len_cap == 4);

		cur_data = cr->wValue & 0x00ff;

		printf("0x%02x <- BBP REG%u\t[READ]\n", cur_data, cur_addr);

		state = 0;
		break;
	/* FIXME: why this does not compile ?
	default:
		assert(false);
		break;
	*/
	}

	// Packet was processed here
	return true;
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

	if (process_mcu_request(cr, shdr, hdr))
		; //goto out;
	if (process_h2m_bbp(cr, shdr, hdr))
		goto out;

	if (cr->wIndex > 0x17ff) {
		// Not registers area
		struct area *area = get_area(cr->wIndex);
		const char *name = area ? area->name : "Unknown area";

		if (is_read_cr(cr)) {
			// Read
			printf("CTRL: READ %d BYTES FROM 0x%04x (%s)\n", hdr->len_cap, cr->wIndex, name);
			print_data(hdr);
		} else {
			// Write
			if (shdr->len_cap == 0) {
				assert(cr->wLength == 0);
				printf("CTRL: WRITE VALUE 0x%04x TO 0x%04x (%s)\n", cr->wValue, cr->wIndex, name);
			} else {
				printf("CTRL: WRITE %d BYTES TO 0x%04x (%s)\n", shdr->len_cap, cr->wIndex, name);
				print_data(shdr);
			}
		}

		goto out;
	}

	// BBP and RF registers are indirectly addressed, print only valuable data
	if (cr->wIndex == reg_bbp.addr || cr->wIndex == reg_bbp.addr + 2)
		process_special_register_rw(cr, shdr, hdr, &reg_bbp);
	else if (cr->wIndex == reg_rf.addr || cr->wIndex == reg_rf.addr + 2)
		process_special_register_rw(cr, shdr, hdr, &reg_rf);
	else
		process_register_rw(cr, shdr, hdr);

out:
	// Seems when asserion fail lines are not printed in order, flush should fix that
	fflush(stdout);

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
#if 0
	assert(shdr->epnum == hdr->epnum);
#else
	if (shdr->epnum != hdr->epnum)
		printf("WARN EP missmash shdr->epnum %02x hdr->epnum %02x\n", shdr->epnum, hdr->epnum);
#endif
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

	regs_array_self_test();

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

