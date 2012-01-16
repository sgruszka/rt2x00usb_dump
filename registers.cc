#include <inttypes.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#endif

struct area {
	uint16_t begin;
	uint16_t end;
	const char *name;
	//void (*print_data)(uint16_t offset);
};

struct area areas_array[] =  {
	{ 0x0000, 0x17ff, "MAC REGISTERS" },
	{ 0x1800, 0x1fff, "WCID search table" },
	{ 0x2000, 0x2fff, "Unknown 1" },
	{ 0x3000, 0x3fff, "Firmware" },
	{ 0x4000, 0x5fff, "Security table/CIS/Beacon/NULL frame" },
	{ 0x6000, 0x67ff, "IV/EIV table" },
	{ 0x6800, 0x6bff, "WCID attribute table" },
	{ 0x6c00, 0x6fff, "Shared Key Table" },
	{ 0x7000, 0x700f, "Shared Key Mode" },
	{ 0x7010, 0x701f, "Shared Memory MCU - host" },
	{ 0x7020, 0xffff, "Unknown 2" },
};

struct area *get_area(uint16_t offset)
{
	const int n = ARRAY_SIZE(areas_array);

	for (int i = 0; i < n; i++) {
		if (areas_array[i].begin <= offset && offset <= areas_array[i].end)
			return &areas_array[i];
	}
	return NULL;
}

struct field {
	uint8_t last;
	uint8_t first;
	const char *name;
};

struct reg {
	uint16_t offset;
	const char *name;
	int n_fields;
	struct field fields[32];
};


struct reg regs_array[] = {
	{ 0x0200, "INT_STATUS", 0, { } },
	{ 0x0204, "INT_MASK", 0, { } },

	{ 0x0208, "WPDMA_GLO_CFG", 9, {
		{ 31, 16, "Reserved" },
		{ 15,  8, "HDR_SEG_LEN" },
		{  7,  7, "BIG_ENDIAN" },
		{  6,  6, "TX_WB_DDONE" },
		{  5,  4, "WPDMA_BT_SIZE" },
		{  3,  3, "RX_DMA_BUSY" },
		{  2,  2, "RX_DMA_EN" },
		{  1,  1, "TX_DMA_BUSY" },
		{  0,  0, "TX_DMA_EN" },

	}},

	{ 0x020C, "WPDMA_RST_IDX", 0, { } },
	{ 0x0210, "DELAY_INT_CFG", 0, { } },
	{ 0x0214, "WMM_AIFSN_CFG", 0, { } },
	{ 0x0218, "WMM_CWMIN_CFG", 0, { } },
	{ 0x021C, "WMM_CWMAX_CFG", 0, { } },
	{ 0x0220, "WMM_TXOP0_CFG", 0, { } },
	{ 0x0224, "WMM_TXOP1_CFG", 0, { } },
	{ 0x0228, "GPIO_CTRL", 0, { } },
	{ 0x022C, "MCU_CMD_REG", 0, { } },
	{ 0x0230, "TX_BASE_PTR0", 0, { } },
	{ 0x0234, "TX_MAX_CNT0", 0, { } },
	{ 0x0238, "TX_CTX_IDX0", 0, { } },
	{ 0x023C, "TX_DTX_IDX0", 0, { } },
	{ 0x0290, "RX_BASE_PTR", 0, { } },
	{ 0x0294, "RX_MAX_CNT", 0, { } },
	{ 0x0298, "RX_CALC_IDX", 0, { } },
	{ 0x029C, "FS_DRX_IDX", 0, { } },

	{ 0x02A0, "USB_DMA_CFG", 13, {
		{ 31, 31, "TX_BUSY" },
		{ 30, 30, "RX_BUSY" },
		{ 29, 24, "EPOUT_VLD" },
		{ 23, 23, "UDMA_TX_EN" },
		{ 22, 22, "UDMA_RX_EN" },
		{ 21, 21, "RX_AGG_EN" },
		{ 20, 20, "TXOP_HALT" },
		{ 19, 19, "TX_CLEAR" },
		{ 18, 17, "Reserved" },
		{ 16, 16, "PHY_WD_EN" },
		{ 15, 15, "PHY_MAN_RST" },
		{ 14,  8, "RX_AGG_LMT" },
		{  7,  0, "RX_AGG_TO" },
	}},

	{ 0x02A4, "US_CYC_CNT", 6, {
	  	{ 31, 25, "Reserved" },
	  	{ 24, 24, "TEST_EN" },
	  	{ 23, 16, "TEST_SEL" },
	  	{ 15,  9, "Reserved" },
	  	{  8,  8, "BT_MODE_EN" },
	  	{  7,  0, "US_CYC_CNT" },
	}},

	{ 0x0400, "SYS_CTRL", 17, {
		{ 31, 17, "Reserved" },
		{ 16, 16, "HST_PM_SEL" },
		{ 15, 15, "Reserved" },
		{ 14, 14, "CAP_MODE" },
		{ 13, 13, "PME_OEN" },
		{ 12, 12, "CLKSELECT" },
		{ 11, 11, "PBF_CLKEN" },
		{ 10, 10, "MAC_CLKEN" },
		{  9,  9, "DMA_CLKEN" },
		{  8,  8, "Reserved" },
		{  7,  7, "MCU_READY" },
		{  6,  5, "Reseved" },
		{  4,  4, "ASY_RESET" },
		{  3,  3, "PBF_RESET" },
		{  2,  2, "MAC_RESET" },
		{  1,  1, "DMA_RESET" },
		{  0,  0, "MCU_RESET" },
	}},

	{ 0x0404, "HOST_CMD", 1, {
		{ 31, 0, "HOST_CMD"}, 
	}},

	{ 0x0408, "PBF_CFG", 17, {
		{ 31, 24, "Reserved" },
		{ 23, 21, "TX1Q_NUM" },
		{ 20, 16, "TX2Q_NUM" },
		{ 15, 15, "NULL0_MODE" },
		{ 14, 14, "NULL1_MODE" },
		{ 13, 13, "RX_DROP_MODE" },
		{ 12, 12, "TX0Q_MODE" },
		{ 11, 11, "TX1Q_MODE" },
		{ 10, 10, "TX2Q_MODE" },
		{  9,  9, "RX0Q_MODE" },
		{  8,  8, "HCCA_MODE" },
		{  7,  5, "Reserved" },
		{  4,  4, "TX0Q_EN" },
		{  3,  3, "TX1Q_EN" },
		{  2,  2, "TX2Q_EN" },
		{  1,  1, "RX0Q_EN" },
		{  0,  0, "Reserved" },
	}},

	{ 0x040C, "MAX_PCNT", 0, { } },
	{ 0x0410, "BUF_CTRL", 0, { } },
	{ 0x0414, "MCU_INT_STA", 0, { } },
	{ 0x0418, "MCU_INT_ENA", 0, { } },
	{ 0x041C, "TX0Q_IO", 0, { } },
	{ 0x0420, "TX1Q_IO", 0, { } },
	{ 0x0424, "TX2Q_IO", 0, { } },
	{ 0x0428, "RX0Q_IO", 0, { } },

	{ 0x042C, "BCN_OFFSET0", 4, {
		{ 31, 24, "BCN3_OFFSET" },
		{ 23, 16, "BCN2_OFFSET" },
		{ 15,  8, "BCN1_OFFSET" },
		{  7,  0, "BCN0_OFFSET" },
	}},
	
	{ 0x0430, "BCN_OFFSET1", 4, {
		{ 31, 24, "BCN7_OFFSET" },
		{ 23, 16, "BCN6_OFFSET" },
		{ 15,  8, "BCN5_OFFSET" },
		{  7,  0, "BCN4_OFFSET" },
	}},

	{ 0x0434, "TXRXQ_STA", 0, {
		{ 31, 24, "RX0Q_STA" },
		{ 23, 16, "TX2Q_STA" },
		{ 15,  8, "TX1Q_STA" },
		{  7,  0, "TX0Q_STA" },
	}},

	{ 0x0438, "TXRXQ_PCNT", 4, {
		{ 31, 24, "RX0Q_PCNT" },
		{ 23, 16, "TX2Q_PCNT" },
		{ 15,  8, "TX1Q_PCNT" },
		{  7,  0, "TX0Q_PCNT" },
	}},

	{ 0x043C, "PBF_DBG", 0, { } },
	{ 0x0440, "CAP_CTRL", 0, { } },

	// From rt2x00 driver
	{ 0x0500, "RF_CSR_CFG", 6, {
		{  31, 18, "Reserved" },
		{  17, 17, "BUSY" },
		{  16, 16, "WRITE" },
		{  15, 14, "Reserved" },
		{  13, 8, "REGNUM" },
		{  7,  0, "DATA" },
	}},

	{ 0x0580, "EFUSE_CTRL", 0, { } },
	{ 0x0590, "EFUSE_DATA0", 0, { } },
	{ 0x0594, "EFUSE_DATA1", 0, { } },
	{ 0x0598, "EFUSE_DATA2", 0, { } },
	{ 0x059c, "EFUSE_DATA3", 0, { } },
	{ 0x05d4, "LDO_CFG0", 0, { } },
	{ 0x05dc, "GPIO_SWITCH", 0, { } },

	{ 0x1000, "ASIC_VER_ID", 2, {
		{ 31, 16, "VER_ID" },
		{ 15,  0, "REV_ID" },
		
	}},
	
	{ 0x1004, "MAC_SYS_CTRL", 9, {
		{ 31,  8, "Reserved" },
		{  7,  7, "RX_TS_EN" },
		{  6,  6, "WLAN_HALT_EN" },
		{  5,  5, "PBF_LOOP_EN" },
		{  4,  4, "CONT_TX_TEST" },
		{  3,  3, "MAC_RX_EN" },
		{  2,  2, "MAC_TX_EN" },
		{  1,  1, "BBP_HRST" },
		{  0,  0, "MAC_SRST" },
	}},

	{ 0x1008, "MAC_ADDR_DW0", 4, {
		{ 31, 24, "MAC_ADDR_3" },
		{ 23, 16, "MAC_ADDR_2" },
		{ 15,  8, "MAC_ADDR_1" },
		{  7,  0, "MAC_ADDR_0" },

	}},

	{ 0x100C, "MAC_ADDR_DW1" , 3, {
		{ 31, 16, "Reserved" },
		{ 15,  8, "MAC_ADDR_5" },
		{  7,  0, "MAC_ADDR_4" },
	}},

	{ 0x1010, "MAC_BSSID_DW0", 4, {
		{ 31, 24, "BSSID_3" },
		{ 23, 16, "BSSID_2" },
		{ 15,  8, "BSSID_1" },
		{  7,  0, "BSSID_0" },
	}},

	{ 0x1014, "MAC_BSSID_DW1", 5, {
		{ 31, 21, "Reserved" },
		{ 20, 18, "MULTI_BCN_NUM" },
		{ 17, 16, "MULTI_BSSID_MODE" },
		{ 15,  8, "BSSID_5" },
		{  7,  0, "BSSID_4" },
	}},

	{ 0x1018, "MAX_LEN_CFG", 5, {
		{ 31, 20, "Reserved" },
		{ 19, 16, "MIN_MPDU_LEN" },
		{ 15, 14, "Reserved" },
		{ 13, 12, "MAX_PSDU_LEN" },
		{ 11,  0, "MAX_MPDU_LEN" },
	}},

	{ 0x101c, "BBP_CSR_CFG", 7, {
		{ 31, 20, "Reserved" },
		{ 19, 19, "BBP_RW_MODE" },
		{ 18, 18, "BBP_PAR_DUR" },
		{ 17, 17, "BBP_CSR_KICK" },
		{ 16, 16, "BBP_CSR_RW" },
		{ 15,  8, "BBP_ADDR" },
		{  7,  0, "BBP_DATA" },
	}},

	{ 0x1020, "RF_CSR_CFG0", 5, {
		{ 31, 31, "RF_REG_CTRL" },
		{ 30, 30, "RF_LE_SEL" },
		{ 29, 29, "RF_LE_STBY" }, 
		{ 28, 24, "RF_REG_WIDTH" },
		{ 23,  0, "RF_REG_0" },	
	}},

	{ 0x1024, "RF_CSR_CFG1", 3, {
		{ 31, 25, "Reserved" },
		{ 24, 24, "RF_DUR" },
		{ 23,  0, "RF_REG_1" },
	}},

	{ 0x1028, "RF_CSR_CFG2", 2, {
		{ 31, 24, "Reserved" },
		{ 23,  0, "RF_REG_2" },
	}},
	
	{ 0x102C, "LED_CFG", 9, {
		{ 31, 31, "Reserved" },
		{ 30, 30, "LED_POL" },
		{ 29, 28, "Y_LED_MODE" },
		{ 27, 26, "G_LED_MODE" },
		{ 25, 24, "R_LED_MODE" },
		{ 23, 22, "Reserved" },
		{ 21, 16, "SLOW_BLK_TIME" },
		{ 15,  8, "LED_OFF_TIME" },
		{  7,  0, "LED_ON_TIME" },
	}},

	{ 0x1100, "XIFS_TIME_CFG", 6, {
		{ 31, 30, "Reserved" },
		{ 29, 29, "BB_RXEND_EN" },
		{ 28, 20, "EIFS_TIME" },
		{ 19, 16, "OFDM_XIFS_TIME" },
		{ 15,  8, "OFDM_SIFS_TIME" },
		{  7,  0, "CCK_SIFS_TIME" }
	}},

	{ 0x1104, "BKOFF_SLOT_CFG", 3, {
		{ 31, 12, "Reserved" },
		{ 11,  8, "CC_DELAY_TIME" },
		{  7,  0, "SLOT_TIME" },
	}},

	{ 0x1108, "NAV_TIME_CFG", 4, {
		{ 31, 31, "NAV_UPD" },
		{ 30, 16, "NAV_UPD_VAL" },
		{ 15, 15, "NAV_CLR_EN" },
		{ 14,  0, "NAV_TIMER" },
	}},

	{ 0x110C, "CH_TIME_CFG", 6, {
		{ 31, 5, "Reserved" },
		{ 4,  4, "EIFS_AS_CH_BUSY" },
		{ 3,  3, "NAV_AS_CH_BUSY" },
		{ 2,  2, "RX_AS_CH_BYSY" },
		{ 1,  1, "TX_AS_CH_BUSY" },
		{ 0,  0, "CH_STA_TIMER_EN" },
	}},

	{ 0x1110, "PBF_LIFE_TIMER", 1, {
		{ 31, 0, "PBF_LIFE_TIMER" },
	}},

	{ 0x1114, "BCN_TIME_CFG", 7, {
		{ 31, 24, "TSF_INS_COMP" },
		{ 23, 21, "Reserved" },
		{ 20, 20, "BCN_TX_EN" },
		{ 19, 19, "TBTT_TIMER_EN" },
		{ 18, 17, "TSF_SYNC_MODE" },
		{ 16, 16, "TSF_TIMER_EN" },
		{ 15,  0, "BCN_INTVAL" },
	}},

	{ 0x1118, "TSF_SYNC_CFG", 5, {
		{ 31, 24, "Reserved" },
		{ 23, 20, "BCN_CWMIN" },
		{ 19, 16, "BCN_AIFSN" },
		{ 15,  8, "BCN_EXP_WIN" },
		{  7,  0, "TBTT_ADJUST" }, 
	}},
	
	{ 0x111C, "TSF_TIMER_DW0", 1, {
		{ 31, 0, "TSF_TIMER_DW0" },
	}},

	{ 0x1120, "TSF_TIMER_DW1", 1, {
		{ 31, 0, "TSF_TIMER_DW1" },
	}},

	{ 0x1124, "TBTT_TIMER", 2, {
		{ 31, 17, "Reserved" },
		{ 16,  0, "TBTT_TIMER" },
	}},

	{ 0x1128, "INT_TIMER_CFG", 0, {
	}},
	{ 0x112C, "INT_TIMER_EN", 0, { } },
	{ 0x1130, "CH_IDLE_STA", 0, { } },

	{ 0x1200, "MAC_STATUS_REG", 0, { } },
	{ 0x1204, "PWR_PIN_CFG", 0, { } },

	{ 0x1208,"AUTO_WAKEUP_CFG", 4, {
		{ 31, 16, "Reserved" },
		{ 15, 15, "AUTO_WAKEUP_EN" },
		{ 14,  8, "SLEEP_TBTT_NUM" },
		{  7,  0, "WAKEUP_LEAD_TIME" },
	}},

	{ 0x1300,"EDCA_AC0_CFG", 0, { } },
	{ 0x1304,"EDCA_AC1_CFG", 0, { } },
	{ 0x1308,"EDCA_AC2_CFG", 0, { } },
	{ 0x1310,"EDCA_TID_AC_MAP", 0, { } },

	{ 0x1314,"TX_PWR_CFG_0", 4, {
		{ 31, 24, "TX_PWR_OFDM_12" },
		{ 23, 16, "TX_PWR_OFDM_6" },
		{ 15,  8, "TX_PWR_CCK_5" },
		{  7,  0, "TX_PWR_CCK_1" },
	}},

	{ 0x1318,"TX_PWR_CFG_1", 4, {
		{ 31, 24, "TX_PWR_MCS_2" },
		{ 23, 16, "TX_PWR_MCS_0" },
		{ 15,  8, "TX_PWR_OFDM_48" },
		{  7,  0, "TX_PWR_OFDM_24" },
	}},

	{ 0x131C,"TX_PWR_CFG_2", 4, {
		{ 31, 24, "TX_PWR_MCS_10" },
		{ 23, 16, "TX_PWR_MCS_8" },
		{ 15,  8, "TX_PWR_MCS_6" },
		{  7,  0, "TX_PWR_MCS_4" },
	}},

	{ 0x1320,"TX_PWR_CFG_3", 4, {
		{ 31, 24, "Reserved" },
		{ 23, 16, "Reserved" },
		{ 15,  8, "TX_PWR_MCS_14" },
		{  7,  0, "TX_PWR_MCS_12" },
	}},

	{ 0x1324,"TX_PWR_CFG_4", 4, {
		{ 31, 24, "Reserved" },
		{ 23, 16, "Reserved" },
		{ 15,  8, "Reserved" },
		{  7,  0, "Reserved" },
	}},

	{ 0x1328,"TX_PIN_CFG", 21, {
		{ 31 ,20, "Reserved" },
		{ 19, 19, "TRSW_POL" },
		{ 18, 18, "TRSW_EN" },
		{ 17, 17, "RFTR_POL" },
		{ 16, 16, "RFTR_EN" },
		{ 15, 15, "LNA_PE_G1_POL" },
		{ 14, 14, "LNA_PE_A1_POL" },
		{ 13, 13, "LNA_PE_G0_POL" },
		{ 12, 12, "LNA_PE_A0_POL" },
		{ 11, 11, "LNA_PE_G1_EN" },
		{ 10, 10, "LNA_PE_A1_EN" },
		{  9,  9, "LNA_PE_G0_EN" },
		{  8,  8, "LNA_PE_A0_EN" },
		{  7,  7, "PA_PE_G1_POL" },
		{  6,  6, "PA_PE_A1_POL" },
		{  5,  5, "PA_PE_G0_POL" },
		{  4,  4, "PA_PE_A0_POL" },
		{  3,  3, "PA_PE_G1_EN" },
		{  2,  2, "PA_PE_A1_EN" },
		{  1,  1, "PA_PE_G0_EN" },
		{  0,  0, "PA_PE_A0_EN" },
	}},

	{ 0x132C,"TX_BAND_CFG", 4, {
		{ 31, 3, "Reserved" },
		{  2, 2, "5G_BAND_SEL_N" },
		{  1, 1, "5G_CAND_SEL_P" },
		{  0, 0, "TX_BAND_SEL" }, 
	}},

	{ 0x1330,"TX_SW_CFG0", 0, { } },
	{ 0x1334,"TX_SW_CFG1", 0, { } },
	{ 0x1338,"TX_SW_CFG2", 0, { } },
	{ 0x133C,"TXOP_THRES_CFG", 0, { } },
	{ 0x1344,"TX_RTS_CFG", 0, { } },
	{ 0x1348,"TX_TIMEOUT_CFG", 0, { } },
	{ 0x134C,"TX_RTY_CFG", 0, { } },
	{ 0x1354,"HT_FBK_CFG0", 0, { } },
	{ 0x1358,"HT_FBK_CFG1", 0, { } },
	{ 0x135C,"LG_FBK_CFG0", 0, { } },
	{ 0x1360,"LG_FBK_CFG1", 0, { } },
	{ 0x1364,"CCK_PROT_CFG", 0, { } },
	{ 0x1368,"OFDM_PROT_CFG", 0, { } },
	{ 0x136C,"MM20_PROT_CFG", 0, { } },
	{ 0x1370,"MM40_PROT_CFG", 0, { } },
	{ 0x1374,"GF20_PROT_CFG", 0, { } },
	{ 0x1378,"GF40_PROT_CFG", 0, { } },
	{ 0x137C,"EXP_CTS_TIME", 0, { } },
	{ 0x1380,"EXP_ACK_TIME", 0, { } },

	{ 0x1400,"RX_FILTR_CFG", 18, {
		{ 31, 17, "Reserved" },
		{ 16, 16, "DROP_CTRL_RSV" },
		{ 15, 15, "DROP_BAR" },
		{ 14, 14, "DROP_BA" },
		{ 13, 13, "DROP_PSPOLL" },
		{ 12, 12, "DROP_RTS" },
		{ 11, 11, "DROP_CTS" },
		{ 10, 10, "DROP_ACK" },
		{  9,  9, "DROP_CFEND" },
		{  8,  8, "DROP_CFACK" },
		{  7,  7, "DROP_DUPL" },
		{  6,  6, "DROP_BC" },
		{  5,  5, "DROP_MC" },
		{  4,  4, "DROP_VER_ERR" },
		{  3,  3, "DROP_NOT_MYBSS" },
		{  2,  2, "DROP_UC_NOME" },
		{  1,  1, "DROP_PHY_ERR" },
		{  0,  0, "DROP_CRC_ERR" },
	}},

	{ 0x1404,"AUTO_RSP_CFG", 9, {
		{ 31, 8, "Reserved" },
		{  7, 7, "CTRL_PWR_BIT" },
		{  6, 6, "BAC_ACK_POLICY" },
		{  5, 5, "Reserved" },
		{  4, 4, "CCK_SHORT_EN" },
		{  3, 3, "CTS_40M_REF" },
		{  2, 2, "CTS_40M_MODE" },
		{  1, 1, "BAC_ACKPOLICY_EN" },
		{  0, 0, "AUTO_RSP_EN" },
	}},

	{ 0x1408,"LEGACY_BASIC_RATE", 2, {
		{ 31, 12, "Reserved" },
		{ 11,  0, "LEGACY_BASIC_RATE" },
	}},
	
	{ 0x140C,"HT_BASIC_RATE", 1, {
		{ 31, 0, "Reserved" }, // Hmmm
	}},

	{ 0x1410,"HT_CTRL_CFG", 0, { } },
	{ 0x1414,"SIFS_COST_CFG", 0, { } },
	{ 0x1418,"RX_PARSER_CFG", 0, { } },
	{ 0x1500,"TX_SEC_CNT0", 0, { } },
	{ 0x1504,"RX_SEC_CNT0", 0, { } },
	{ 0x1508,"CCMP_FC_MUTE", 0, { } },
	{ 0x1600,"TXOP_HLDR_ADDR0", 0, { } },
	{ 0x1604,"TXOP_HLDR_ADDR1", 0, { } },
	{ 0x1608,"TXOP_HLDR_ET", 0, { } },
	{ 0x160C,"QOS_CFPOLL_RA_DW0", 0, { } },
	{ 0x1610,"QOS_CFPOLL_A1_DW1", 0, { } },
	{ 0x1614,"QOS_CFPOLL_QC", 0, { } },

	{ 0x1700,"RX_STA_CNT0", 2, {
		{ 31, 16, "PHY_ERRCNT" },
		{ 15,  0, "CRC_ERRCNT" },
	}},

	{ 0x1704,"RX_STA_CNT1", 2, {
		{ 31, 16, "PLPC_ERRCNT" },
		{ 15,  0, "CCA_ERRCNT" },
	}},

	{ 0x1708,"RX_STA_CNT2", 2, {
		{ 31, 16, "RX_OVFL_CNT" },
		{ 15,  0, "RX_DUPL_CNT" },
	}},
	
	{ 0x170C,"TX_STA_CNT0", 2, {
		{ 31, 16, "TX_BCN_CNT" },
		{ 15,  0, "TX_FAIL_CNT" },
	}},

	{ 0x1710,"TX_STA_CNT1", 2, {
		{ 31, 16, "TX_RTY_CNT" },
		{ 15,  0, "TX_SUCC_CNT" },
	}},

	{ 0x1714,"TX_STA_CNT2", 2, {
		{ 31, 16, "TX_UDFL_CNT" },
		{ 15,  0, "TX_ZERO_CNT" }, 	
	}},

	{ 0x1718,"TX_STAT_FIFO", 7, {
		{ 31, 16, "TXQ_RATE" },
		{ 15,  8, "TXQ_WCID" },
		{  7,  7, "TXQ_ACKREQ" },
		{  6,  6, "TXQ_AGG" },
		{  5,  5, "TXQ_OK" },
		{  4,  1, "TXQ_PID" },
		{  0,  0, "TXQ_VLD" }, 
	}},

	{ 0x171c, "TX_NAG_AGG_CNT", 2, {
		{ 31, 16, "TX_AGG_CNT" },
		{ 15,  0, "TX_NAG_CNT" },
	}},

	{ 0x1720,"TX_AGG_CNT0", 2, {
		{ 31, 16, "TX_AGG_2_CNT" },
		{ 15,  0, "TX_AGG_1_CNT" },
	}},

	{ 0x1724, "TX_AGG_CNT1", 2, {
		{ 31, 16, "TX_AGG_4_CNT" },
		{ 15,  0, "TX_AGG_3_CNT" },
	}},

	{ 0x1728, "TX_AGG_CNT2", 2, {
		{ 31, 16, "TX_AGG_6_CNT" },
		{ 15,  0, "TX_AGG_5_CNT" },
	}},

	{ 0x172C, "TX_AGG_CNT3", 2, {
		{ 31, 16, "TX_AGG_8_CNT" },
		{ 15,  0, "TX_AGG_7_CNT" },
	}},

	{ 0x1730, "TX_AGG_CNT4", 2, {
		{ 31, 16, "TX_AGG_10_CNT" },
		{ 15,  0, "TX_AGG_9_CNT" },
	}},

	{ 0x1734, "TX_AGG_CNT5", 2, {
		{ 31, 16, "TX_AGG_12_CNT" },
		{ 15,  0, "TX_AGG_11_CNT" },
	}},

	{ 0x1738, "TX_AGG_CNT6", 2, {
		{ 31, 16, "TX_AGG_14_CNT" },
		{ 15,  0, "TX_AGG_13_CNT" },
	}},

	{ 0x173C, "TX_AGG_CNT7", 2, {
		{ 31, 16, "TX_AGG_16_CNT" },
		{ 15,  0, "TX_AGG_15_CNT" },
	}},

	{ 0x1740, "MPDU_DENSITY_CNT", 0, { } },
};

// Not true registers, included in usb data
struct reg txinfo = {
	0, "TXINFO", 8, {
		{ 31, 31, "USB_DMA_TX_BURST" },
		{ 30, 30, "USB_DMA_NEXT_VALID" },
		{ 29, 28, "Reserved" },
		{ 27, 27, "SW_USE_LAST_ROUND" },
		{ 26, 25, "QSEL" },
		{ 24, 24, "WIV" },
		{ 23, 16, "Reserved" },
		{ 15,  0, "TX_PKT_LEN" },
	}
};

struct reg txwi_w0 = {
	0, "TXWI_W0", 15, {
		{ 31, 30, "PHYMODE" },
		{ 29, 28, "Reserved" },
		{ 27, 27, "IFS" },
		{ 26, 25, "STBC" },
		{ 24, 24, "SHORT_GI" },
		{ 23, 23, "BW" },
		{ 22, 16, "MCS" },
		{ 15, 10, "Reserved" },
		{  9,  8, "TXOP" },
		{  7,  5, "MPDU_DENSITY" },
		{  4,  4, "AMPDU" },
		{  3,  3, "TS" },
		{  2,  2, "CFACK" },
		{  1,  1, "MIMO_PS"},
		{  0,  0, "FRAG" },
	},
};

struct reg txwi_w1 = {
	0, "TXWI_W1", 6, {
		{ 31, 28, "PACKET_ID" },
		{ 27, 16, "MPDU_TOTAL_BYTE_COUNT" },
		{ 15,  8, "WCID" },
		{  7,  2, "BA_WIN_SIZE" },
		{  1,  1, "NSEQ" },
		{  0,  0, "ACK" },
	},
};

struct reg rxinfo = {
	0, "RXINFO", 2, {
		{ 31, 16, "Reserved" },
		{ 15,  0, "RX_PKT_LEN" },
	},
};

struct reg rxwi_w0 = {
	0, "RXWI_W0", 6, {
		{ 31, 28, "TID" }, 
		{ 27, 16, "MPDU_TOTAL_BYTE_COUNT" },
		{ 15, 13, "UDF" },
		{ 12, 10, "BSSID" },
		{  9,  8, "KEY_INDEX" },
		{  7,  0, "WCID" },
	},
};

struct reg rxwi_w1 = {
	0, "RXWI_W1", 7, {
		{ 31, 30, "PHYMODE" },
		{ 29, 27, "Reserved" },
		{ 26, 25, "STBC" },
		{ 24, 24, "SHORT_GI" },
		{ 23, 23, "BW" },
		{ 22, 16, "MCS" },
		{ 15,  3, "SEQUENCE" },
		{  2,  0, "FRAG" }, 
	},
};

struct reg rxwi_w2 = {
	0, "RXWI_W2", 4, {
		{ 31, 24, "Reserved" },
		{ 23, 16, "RSSI2" },
		{ 15,  8, "RSSI1" },
		{  7,  0, "RSSI0" },
	},
};

struct reg rxwi_w3 = {
	0, "RXWI_W3", 3, {
		{ 31, 16, "Reserved" },
		{ 15,  8, "SNR0" },
		{  7,  0, "SNR1" },
	},
};

struct reg rxd = {
	0, "RXD", 20, {
		{ 31, 20, "PLCP_SIGNAL" },
		{ 19, 19, "LAST_AMPDU" },
		{ 18, 18, "CIPHER_ALG" },
		{ 17, 17, "PLCP_RSSI" },
		{ 16, 16, "DECRYPTED" },
		{ 15, 15, "AMPDU" },
		{ 14, 14, "L2PAD" },
		{ 13, 13, "RSSI" },
		{ 12, 12, "HTC" },
		{ 11, 11, "AMSDU" },
		{ 10,  9, "CIPHER_ERROR" },
		{  8,  8, "CRC_ERROR" },
		{  7,  7, "MY_BSS" },
		{  6,  6, "BROADCAST" },
		{  5,  5, "MULTICAST" },
		{  4,  4, "UNICAST_TO_ME" },
		{  3,  3, "FRAG" },
		{  2,  2, "NULLDATA" },
		{  1,  1, "DATA" },
		{  0,  0, "BA" },
	},
};

struct reg *get_reg(uint16_t offset)
{
	const int n = ARRAY_SIZE(regs_array);

	for (int i = 0; i < n; i++) {
		if (regs_array[i].offset == offset)
			return &regs_array[i];
	}
	return NULL;
}

void regs_array_self_test(void)
{
	const int n = ARRAY_SIZE(regs_array);
	const bool print = false;

	for (int i = 0; i < n; i++) {
		if (print)
			printf("%s:\n", regs_array[i].name);
		int nf  = regs_array[i].n_fields;
		if (nf == 0)
			continue;

		int prev_first = 32;
		for (int j = 0; j < nf; j++) {
			struct field *f = &regs_array[i].fields[j];

			assert(f->name != NULL);
			if (print) {
				printf("\t%s:\n", regs_array[i].fields[j].name);
				fflush(stdout);
			}
			assert(f->last + 1 == prev_first);
			prev_first = f->first;
		}
		assert(prev_first == 0);
	}
}
