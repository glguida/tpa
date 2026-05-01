#ifndef TPA_MSG_EDGE_STORAGE_CONFIG_H
#define TPA_MSG_EDGE_STORAGE_CONFIG_H

unsigned char tpa_msg_edge_storage_ch0_buf[64] __attribute__((aligned(64)));

#define TPA_EDGE_CH_0_NRBUF 1u
#define TPA_EDGE_CH_0_BUF0 tpa_msg_edge_storage_ch0_buf
#define TPA_EDGE_CH_0_BUF1 0

#endif
