

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tl866iiplus.h"

#define TL866IIPLUS_BEGIN_TRANS 0x03
#define TL866IIPLUS_END_TRANS 0x04
#define TL866IIPLUS_READID 0x05
#define TL866IIPLUS_READ_USER 0x06
#define TL866IIPLUS_WRITE_USER 0x07
#define TL866IIPLUS_READ_CFG 0x08
#define TL866IIPLUS_WRITE_CFG 0x09
#define TL866IIPLUS_WRITE_CODE 0x0C
#define TL866IIPLUS_READ_CODE 0x0D
#define TL866IIPLUS_ERASE 0x0E
#define TL866IIPLUS_READ_DATA 0x10
#define TL866IIPLUS_WRITE_DATA 0x11
#define TL866IIPLUS_WRITE_LOCK 0x14
#define TL866IIPLUS_READ_LOCK 0x15
#define TL866IIPLUS_PROTECT_OFF 0x18
#define TL866IIPLUS_PROTECT_ON 0x19

#define TL866IIPLUS_UNLOCK_TSOP48 0x38
#define TL866IIPLUS_REQUEST_STATUS 0x39

static void msg_init(minipro_handle_t *handle, uint8_t command, uint8_t *buf,
                     size_t length) {
  assert(length >= 8);

  memset(buf, 0x00, length);
  buf[0] = command;
  if (handle->device) {
    buf[1] = handle->device->protocol_id;
    buf[2] = handle->device->variant;
  }
  buf[3] = handle->icsp;
}

static void payload_transfer_cb(struct libusb_transfer *transfer) {
  int *completed = transfer->user_data;
  if (completed != NULL) {
    *completed = 1;
  }
}

static int payload_transfer(minipro_handle_t *handle, uint8_t direction,
                            uint8_t *ep2_buffer, size_t ep2_length,
                            uint8_t *ep3_buffer, size_t ep3_length) {
  struct libusb_transfer *ep2_urb;
  struct libusb_transfer *ep3_urb;
  int ret;
  int32_t ep2_completed = 0;
  int32_t ep3_completed = 0;

  ep2_urb = libusb_alloc_transfer(0);
  ep3_urb = libusb_alloc_transfer(0);
  if (ep2_urb == NULL || ep3_urb == NULL) {
    fprintf(stderr, "Out of memory");
    return -1;
  }

  libusb_fill_bulk_transfer(ep2_urb, handle->usb_handle, (0x02 | direction),
                            ep2_buffer, ep2_length, payload_transfer_cb,
                            &ep2_completed, 5000);
  libusb_fill_bulk_transfer(ep3_urb, handle->usb_handle, (0x03 | direction),
                            ep3_buffer, ep3_length, payload_transfer_cb,
                            &ep3_completed, 5000);

  ret = libusb_submit_transfer(ep2_urb);
  if (ret < 0) {
    minipro_close(handle);
    fprintf(stderr, "IO error: submit_transfer: %s\n", libusb_error_name(ret));
    return -1;
  }
  ret = libusb_submit_transfer(ep3_urb);
  if (ret < 0) {
    minipro_close(handle);
    fprintf(stderr, "IO error: submit_transfer: %s\n", libusb_error_name(ret));
    return -1;
  }

  while (!ep2_completed) {
    ret = libusb_handle_events_completed(handle->ctx, &ep2_completed);
    if (ret < 0) {
      if (ret == LIBUSB_ERROR_INTERRUPTED) continue;
      libusb_cancel_transfer(ep2_urb);
      libusb_cancel_transfer(ep3_urb);
      continue;
    }
  }
  while (!ep3_completed) {
    ret = libusb_handle_events_completed(handle->ctx, &ep3_completed);
    if (ret < 0) {
      if (ret == LIBUSB_ERROR_INTERRUPTED) continue;
      libusb_cancel_transfer(ep2_urb);
      libusb_cancel_transfer(ep3_urb);
      continue;
    }
  }

  if (ep2_urb->status != 0 || ep3_urb->status != 0) {
    minipro_close(handle);
    fprintf(stderr, "IO Error: Async transfer failed.");
    return -1;
  }

  libusb_free_transfer(ep2_urb);
  libusb_free_transfer(ep3_urb);
  return 0;
}

static int write_payload(minipro_handle_t *handle, uint8_t *buffer,
                         size_t length) {
  uint8_t *data;
  int ret;
  size_t ep_length = length / 2;
  size_t blocks = length / 64;

  data = malloc(length);

  if (data == NULL) {
    fprintf(stderr, "Out of memory");
    return -1;
  }

  for (int i = 0; i < blocks; ++i) {
    uint8_t *ep_buf;
    if (i % 2 == 0) {
      ep_buf = data;
    } else {
      ep_buf = data + ep_length;
    }
    memcpy(ep_buf + ((i / 2) * 64), buffer + (i * 64), 64);
  }

  ret = payload_transfer(handle, LIBUSB_ENDPOINT_OUT, data, ep_length,
                         data + ep_length, ep_length);

  free(data);
  return ret;
}

static int read_payload(minipro_handle_t *handle, uint8_t *buffer,
                        size_t length) {
  uint8_t *data;
  int ret;
  size_t ep_length = length / 2;
  size_t blocks = length / 64;

  data = malloc(length);

  if (data == NULL) {
    fprintf(stderr, "Out of memory");
    return -1;
  }

  ret = payload_transfer(handle, LIBUSB_ENDPOINT_IN, data, ep_length,
                         data + ep_length, ep_length);

  for (int i = 0; i < blocks; ++i) {
    uint8_t *ep_buf;
    if (i % 2 == 0) {
      ep_buf = data;
    } else {
      ep_buf = data + ep_length;
    }
    memcpy(buffer + (i * 64), ep_buf + ((i / 2) * 64), 64);
  }

  free(data);
  return ret;
}

int tl866iiplus_begin_transaction(minipro_handle_t *handle) {
  uint8_t msg[64];
  msg_init(handle, TL866IIPLUS_BEGIN_TRANS, msg, sizeof(msg));
  format_int(&(msg[40]), handle->device->package_details, 4, MP_LITTLE_ENDIAN);
  format_int(&(msg[16]), handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
  format_int(&(msg[14]), handle->device->data_memory2_size, 2,
             MP_LITTLE_ENDIAN);
  format_int(&(msg[12]), handle->device->opts3, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[10]), handle->device->opts2, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[8]), handle->device->data_memory_size, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[5]), handle->device->opts1, 2, MP_LITTLE_ENDIAN);
  return msg_send(handle, msg, sizeof(msg));
}

int tl866iiplus_end_transaction(minipro_handle_t *handle) {
  uint8_t msg[8];
  msg_init(handle, TL866IIPLUS_END_TRANS, msg, sizeof(msg));
  return msg_send(handle, msg, sizeof(msg));
}

int tl866iiplus_read_block(minipro_handle_t *handle, uint8_t type,
                           uint32_t addr, uint8_t *buf, size_t len) {
  uint8_t msg[64];

  if (type == MP_CODE) {
    type = TL866IIPLUS_READ_CODE;
  } else if (type == MP_DATA) {
    type = TL866IIPLUS_READ_DATA;
  } else {
    fprintf(stderr, "Unknown type for read_block (%d)\n", type);
    return -1;
  }

  msg_init(handle, type, msg, sizeof(msg));
  format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[4]), addr, 4, MP_LITTLE_ENDIAN);
  if (msg_send(handle, msg, 8)) return -1;
  if (len < 64) {
    return msg_recv(handle, buf, len);
  } else {
    return read_payload(handle, buf, len);
  }
}

int tl866iiplus_write_block(minipro_handle_t *handle, uint8_t type,
                            uint32_t addr, uint8_t *buf, size_t len) {
  uint8_t msg[72];

  if (type == MP_CODE) {
    type = TL866IIPLUS_WRITE_CODE;
  } else if (type == MP_DATA) {
    type = TL866IIPLUS_WRITE_DATA;
  } else {
    fprintf(stderr, "Unknown type for write_block (%d)\n", type);
    return -1;
  }

  msg_init(handle, type, msg, sizeof(msg));
  format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
  format_int(&(msg[4]), addr, 4, MP_LITTLE_ENDIAN);
  if (len < 64) {
    memcpy(&(msg[8]), buf, len);
    if (msg_send(handle, msg, 8 + len)) return -1;
  } else {
    if (msg_send(handle, msg, 8)) return -1;
    if (write_payload(handle, buf, len)) return -1;
  }
  return 0;
}

int tl866iiplus_read_fuses(minipro_handle_t *handle, uint8_t type,
                           size_t length, uint8_t items_count,
                           uint8_t *buffer) {
  uint8_t msg[16];

  if (type == MP_FUSE_USER) {
    type = TL866IIPLUS_READ_USER;
  } else if (type == MP_FUSE_CFG) {
    type = TL866IIPLUS_READ_CFG;
  } else if (type == MP_FUSE_LOCK) {
    type = TL866IIPLUS_READ_LOCK;
  } else {
    fprintf(stderr, "Unknown type for read_fuses (%d)\n", type);
    return -1;
  }

  msg_init(handle, type, msg, 8);
  format_int(&(msg[2]), length / WORD_SIZE(handle->device), 2,
             MP_LITTLE_ENDIAN);
  if (msg_send(handle, msg, 8)) return -1;
  if (msg_recv(handle, msg, 8 + length)) return -1;
  memcpy(buffer, &(msg[8]), length);
  return 0;
}

int tl866iiplus_write_fuses(minipro_handle_t *handle, uint8_t type,
                            size_t length, uint8_t items_count,
                            uint8_t *buffer) {
  uint8_t msg[16];

  if (type == MP_FUSE_USER) {
    type = TL866IIPLUS_WRITE_USER;
  } else if (type == MP_FUSE_CFG) {
    type = TL866IIPLUS_WRITE_CFG;
  } else if (type == MP_FUSE_LOCK) {
    type = TL866IIPLUS_WRITE_LOCK;
  } else {
    fprintf(stderr, "Unknown type for write_fuses (%d)\n", type);
  }

  msg_init(handle, type, msg, 8);
  format_int(&(msg[2]), length / WORD_SIZE(handle->device), 2,
             MP_LITTLE_ENDIAN);
  memcpy(&(msg[8]), buffer, length);
  return (msg_send(handle, msg, 8 + length));
}

int tl866iiplus_get_chip_id(minipro_handle_t *handle, uint8_t *type,
                            uint32_t *device_id) {
  uint8_t msg[8], format;
  msg_init(handle, TL866IIPLUS_READID, msg, sizeof(msg));
  if (msg_send(handle, msg, sizeof(msg))) return -1;
  if (msg_recv(handle, msg, 6)) return -1;
  *type = msg[0];  // The Chip ID type (1-5)
  format = (*type == MP_ID_TYPE3 || *type == MP_ID_TYPE4 ? MP_LITTLE_ENDIAN
                                                         : MP_BIG_ENDIAN);
  msg[1] &= 0x03;  // The length byte is always 1-4 but never know, truncate to
                   // max. 4 bytes.
  *device_id = (msg[1] ? load_int(&(msg[2]), msg[1], format)
                       : 0);  // Check for positive length.
  return 0;
}

int tl866iiplus_protect_off(minipro_handle_t *handle) {
  uint8_t msg[8];
  msg_init(handle, TL866IIPLUS_PROTECT_OFF, msg, sizeof(msg));
  return msg_send(handle, msg, sizeof(msg));
}

int tl866iiplus_protect_on(minipro_handle_t *handle) {
  uint8_t msg[8];
  msg_init(handle, TL866IIPLUS_PROTECT_ON, msg, sizeof(msg));
  return msg_send(handle, msg, sizeof(msg));
}

int tl866iiplus_erase(minipro_handle_t *handle) {
  // fprintf(stderr, "Erase\n");
  uint8_t msg[64];
  msg_init(handle, TL866IIPLUS_ERASE, msg, sizeof(msg));
  format_int(&(msg[2]), 0x03, 2, MP_LITTLE_ENDIAN);
  /* There's no "write unlock". This is how many fuses the controller have
   * or 1 if the device is something else.
   */
  switch (handle->device->protocol_id) {
    case PLD_PROTOCOL2_16V8:
    case PLD_PROTOCOL2_20V8:
    case PLD_PROTOCOL2_22V10:
      break;
    default:

      if (((fuse_decl_t *)handle->device->config) == NULL ||
          ((fuse_decl_t *)handle->device->config)->num_fuses == 0)
        msg[2] = 1;
      else {
        msg[2] = ((fuse_decl_t *)handle->device->config)->erase_num_fuses;
      }
  }
  if (msg_send(handle, msg, 15)) return -1;
  memset(msg, 0x00, sizeof(msg));
  if (msg_recv(handle, msg, sizeof(msg))) return -1;
  return 0;

}

int tl866iiplus_get_ovc_status(minipro_handle_t *handle,
                               minipro_status_t *status, uint8_t *ovc) {
  uint8_t msg[32];
  msg_init(handle, TL866IIPLUS_REQUEST_STATUS, msg, sizeof(msg));
  if (msg_send(handle, msg, 8)) return -1;
  if (msg_recv(handle, msg, sizeof(msg))) return -1;
  if (status)  // Check for null
    {
      // This is verify while writing feature.
      status->error = msg[0];
      status->address = load_int(&msg[8], 4, MP_LITTLE_ENDIAN);
      status->c1 = load_int(&msg[2], 2, MP_LITTLE_ENDIAN);
      status->c2 = load_int(&msg[4], 2, MP_LITTLE_ENDIAN);
    }
  *ovc = msg[12];  // return the ovc status
  return 0;
}

int tl866iiplus_unlock_tsop48(minipro_handle_t *handle, uint8_t *status) {
  uint8_t msg[48];
  uint16_t i, crc = 0;

  msg_init(handle, TL866IIPLUS_UNLOCK_TSOP48, msg, sizeof(msg));

  srandom(time(NULL));
  for (i = 8; i < 16; i++) {
    msg[i] = (uint8_t)random();
    // Calculate the crc16
    crc = (crc >> 8) | (crc << 8);
    crc ^= msg[i];
    crc ^= (crc & 0xFF) >> 4;
    crc ^= (crc << 12);
    crc ^= (crc & 0xFF) << 5;
  }
  msg[16] = msg[10];
  msg[17] = msg[12];
  msg[10] = (uint8_t)crc;
  msg[12] = (uint8_t)(crc >> 8);
  if (msg_send(handle, msg, sizeof(msg))) return -1;
  if (msg_recv(handle, msg, 8)) return -1;
  *status = msg[1];
  return 0;
}
