/*
 * Copyright (c) 2024 MUXen <jn.coueron@muxen.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT himax_hx8379

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dsi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hx8379, CONFIG_DISPLAY_LOG_LEVEL);

struct hx8379_config {
	const struct device *mipi_dsi;
	const struct gpio_dt_spec reset_gpio;
	const struct gpio_dt_spec bl_gpio;
	uint8_t num_of_lanes;
	uint8_t pixel_format;
	uint16_t panel_width;
	uint16_t panel_height;
	uint8_t channel;
};

/**
 * @name Display timings
 * @{
 */

/** Horizontal low pulse width */
#define HX8379_HSYNC 2U
/** Horizontal front porch. */
#define HX8379_HFP 1U
/** Horizontal back porch. */
#define HX8379_HBP 1U
/** Vertical low pulse width. */
#define HX8379_VSYNC 1U
/** Vertical front porch. */
#define HX8379_VFP 50U
/** Vertical back porch. */
#define HX8379_VBP 12U

/* MIPI DCS commands specific to this display driver */
#define HX8379_SETMIPI 0xBA
#define HX8379_MIPI_LPTX_BTA_READ BIT(6)
#define HX8379_MIPI_LP_CD_DIS BIT(5)
#define HX8379_MIPI_TA_6TL 0x3
#define HX8379_MIPI_DPHYCMD_LPRX_8NS 0x40
#define HX8379_MIPI_DPHYCMD_LPRX_66mV 0x20
#define HX8379_MIPI_DPHYCMD_LPTX_SRLIM 0x8
#define HX8379_MIPI_DPHYCMD_LDO_1_55V 0x60
#define HX8379_MIPI_DPHYCMD_HSRX_7X 0x8
#define HX8379_MIPI_DPHYCMD_HSRX_100OHM 0x2
#define HX8379_MIPI_DPHYCMD_LPCD_1X 0x1

#define HX8379_SET_ADDRESS 0x36
#define HX8379_FLIP_HORIZONTAL BIT(1)
#define HX8379_FLIP_VERTICAL BIT(0)

#define HX8379_SETPOWER 0xB1
#define HX8379_POWER_AP_2_0UA 0x4
#define HX8379_POWER_APF_EN 0x40
#define HX8379_POWER_VRHP_5_8V 0x1C
#define HX8379_POWER_VRHN_5_8V 0x1C
#define HX8379_POWER_BTP_6_45V 0x17
#define HX8379_POWER_EN_VSP_CLAMP 0x20
#define HX8379_POWER_BTN_6_45V 0x17
#define HX8379_POWER_XDK_X2_5 0x40
#define HX8379_POWER_XDKN_X3 0x80
#define HX8379_POWER_EN_VSN_CLAMP 0x10
#define HX8379_POWER_VCLS_3_1V 0xC0
#define HX8379_POWER_PMTU 0x10
#define HX8379_POWER_VGH_RATIO_3VSPVSN 0xC0
#define HX8379_POWER_VGHS_13_2V 0x22
#define HX8379_POWER_VGL_RATIO_1VSNVSP 0x40
#define HX8379_POWER_VGLS_11_2V 0x18
#define HX8379_POWER_EN_NVREF 0x80
#define HX8379_POWER_VGH_17_6V 0x38
#define HX8379_POWER_VGL_17_6V 0x38
#define HX8379_POWER_CLK_OPT5_VCL_HSYNC_RST 0x80
#define HX8379_POWER_FS0_DIV_8 0x02
#define HX8379_POWER_FS1_DIV_160 0x40
#define HX8379_POWER_FS2_DIV_160 0x04
#define HX8379_POWER_FS3_DIV_128 0x30
#define HX8379_POWER_FS4_DIV_128 0x03
#define HX8379_POWER_FS5_DIV_128 0x30

#define HX8379_SETDISP 0xB2
#define HX8379_DISP_ZZ_LR 0x80
#define HX8379_DISP_NL_480 0x14
#define HX8379_DISP_BP_14 0xC
#define HX8379_DISP_FP_50 0x30
#define HX8379_DISP_SAP_4 0x20
#define HX8379_DISP_RTN_160 0x50

#define HX8379_SETCYC 0xB4
#define HX8379_CYC_SPON_1 0x01
#define HX8379_CYC_SPOFF_170 0xAA
#define HX8379_CYC_CON_1 0x01
#define HX8379_CYC_COFF_175 0xAF
#define HX8379_CYC_CON1_1 0x01
#define HX8379_CYC_COFF1_175 0xAF
#define HX8379_CYC_EQON1_64 0x10
#define HX8379_CYC_EQON2_936 0xEA
#define HX8379_CYC_SON_112 0x1C
#define HX8379_CYC_SOFF_936 0xEA

#define HX8379_SETPANEL 0xCC
#define HX8379_BGR_PANEL BIT(0)
#define HX8379_REV_PANEL BIT(1)
#define HX8379_GS_PANEL BIT(2)
#define HX8379_SS_PANEL BIT(3)

#define HX8379_SETGIP0 0xD3
#define HX8379_GIP0_EQ_OPT_BOTH 0x0
#define HX8379_GIP0_EQ_HSYNC_NORMAL 0x0
#define HX8379_GIP0_EQ_VSEL_VSSA 0x0
#define HX8379_SHP_START_4 0x40
#define HX8379_SCP_WIDTH_7X_HSYNC 0x7
#define HX8379_CHR0_12X_HSYNC 0xA
#define HX8379_CHR1_18X_HSYNC 0x10

#define HX8379_SETGIP1 0xD5
#define HX8379_SETGIP2 0xD6

#define HX8379_SETGIP0 0xD3
#define HX8379_GIP0_VGLO_SEL BIT(1)
#define HX8379_GIP0_LVGL_SEL BIT(0)
#define HX8379_GIP0_EQ_DELAY_56 0x07

#define HX8379_SETGAMMA 0xE0
#define HX8379_GAMMA_VRP0 0x00
#define HX8379_GAMMA_VRP1 0x16

#define HX8379_SETVCOM 0xB6
#define HX8379_VCMC_F_0_74V 0x2C
#define HX8379_VCMC_B_0_74V 0x92


#define HX8379_SETBANK 0xBD
#define HX8379_SETDGCLUT 0xC1

#define HX8379_SET_TEAR 0x35
#define HX8379_TEAR_VBLANK 0x0

#define HX8379_SETEXTC 0xB9
#define HX8379_EXTC1_MAGIC 0xFF
#define HX8379_EXTC2_MAGIC 0x83
#define HX8379_EXTC3_MAGIC 0x79


const uint8_t enable_extension[] = {
	HX8379_SETEXTC,
	HX8379_EXTC1_MAGIC,
	HX8379_EXTC2_MAGIC,
	HX8379_EXTC3_MAGIC,
};

const uint8_t address_config[] = {
	HX8379_SET_ADDRESS,
	HX8379_FLIP_HORIZONTAL
};

const uint8_t power_config[] = {
	/* CMD B1 */  HX8379_SETPOWER,
	/* P01 44 */ (HX8379_POWER_APF_EN | HX8379_POWER_AP_2_0UA),
	/* P02 1C */  HX8379_POWER_VRHP_5_8V,
	/* P03 1C */  HX8379_POWER_VRHN_5_8V,
	/* P04 37 */ (HX8379_POWER_EN_VSP_CLAMP | HX8379_POWER_BTP_6_45V),
	/* P05 57 */ (HX8379_POWER_XDK_X2_5 | HX8379_POWER_BTN_6_45V),
	/* P06 90 */ (HX8379_POWER_XDKN_X3 | HX8379_POWER_EN_VSN_CLAMP),
	/* P07 D0 */ (HX8379_POWER_VCLS_3_1V | HX8379_POWER_PMTU),
	/* P08 E2 */ (HX8379_POWER_VGH_RATIO_3VSPVSN | HX8379_POWER_VGHS_13_2V),
	/* P09 58 */ (HX8379_POWER_VGL_RATIO_1VSNVSP | HX8379_POWER_VGLS_11_2V),
	/* P10 80 */  HX8379_POWER_EN_NVREF,
	/* P11 38 */  HX8379_POWER_VGH_17_6V,
	/* P12 38 */  HX8379_POWER_VGL_17_6V,
	/* P13 F8 */  HX8379_POWER_CLK_OPT5_VCL_HSYNC_RST,
	/* P14 33 */ (HX8379_POWER_FS5_DIV_128 | HX8379_POWER_FS4_DIV_128),
	/* P15 34 */ (HX8379_POWER_FS3_DIV_128 | HX8379_POWER_FS2_DIV_160),
	/* P16 42 */ (HX8379_POWER_FS1_DIV_160 | HX8379_POWER_FS0_DIV_8),
};

const uint8_t line_config[] = {
	/* CMD B2 */ HX8379_SETDISP,
	/* P01 80 */ HX8379_DISP_ZZ_LR,
	/* P02 14 */ HX8379_DISP_NL_480,
	/* P03 0C */ HX8379_DISP_BP_14,
	/* P04 30 */ HX8379_DISP_FP_50,
	/* P05 20 */ HX8379_DISP_SAP_4,
	/* P06 50 */ HX8379_DISP_RTN_160,
	/* P07 11 */ 0x11,
	/* P08 42 */ 0x42,
	/* P09 1D */ 0x1D,
};

const uint8_t cycle_config[] = {
	/* CMD B4 */ HX8379_SETCYC,
	/* P01 01 */ HX8379_CYC_SPON_1,
	/* P02 AA */ HX8379_CYC_SPOFF_170,
	/* P03 01 */ HX8379_CYC_CON_1,
	/* P04 AF */ HX8379_CYC_COFF_175,
	/* P05 01 */ HX8379_CYC_CON1_1,
	/* P06 AF */ HX8379_CYC_COFF1_175,
	/* P07 10 */ HX8379_CYC_EQON1_64,
	/* P08 EA */ HX8379_CYC_EQON2_936,
	/* P09 1C */ HX8379_CYC_SON_112,
	/* P10 EA */ HX8379_CYC_SOFF_936,
};

const uint8_t hx8379_cmd1[] = {0xC7, 0x00, 0x00, 0x00, 0xC0};

const uint8_t panel_config[] = {
	/* CMD CC */ HX8379_SETPANEL,
	/* P01 02 */ HX8379_REV_PANEL
};

const uint8_t hx8379_cmd3[] = {0xD2, 0x77};

const uint8_t gip0_config[] = {
	/* CMD D3 */ HX8379_SETGIP0,
	/* P01 00 */ 0x00,
	/* P02 00 */ HX8379_GIP0_EQ_DELAY_56,
	/* P03 00 */ 0x00,
	/* P04 00 */ 0x00,
	/* P05 00 */ 0x00,
	/* P06 00 */ 0x08, /* USER_GIP_GATE */
	/* P07 00 */ 0x08, /* USER_GIP_GATE1 */
	/* P08 00 */ 0x32,
	/* P09 00 */ 0x10,
	/* P10 00 */ 0x01, /* SHR0 */
	/* P11 00 */ 0x00,
	/* P12 00 */ 0x01,
	/* P13 00 */ 0x03,
	/* P14 00 */ 0x72,
	/* P15 00 */ 0x03,
	/* P16 00 */ 0x72,
	/* P17 00 */ 0x00,
	/* P18 00 */ 0x08,
	/* P19 00 */ 0x00,
	/* P20 00 */ 0x08,
	/* P21 00 */ 0x33,
	/* P22 00 */ 0x33,
	/* P23 00 */ 0x05,
	/* P24 00 */ 0x05,
	/* P25 00 */ 0x37,
	/* P26 00 */ 0x05,
	/* P27 00 */ 0x05,
	/* P28 00 */ 0x37,
	/* P29 00 */ 0x0A,
	/* P30 00 */ 0x00,
	/* P31 00 */ 0x00,
	/* P32 00 */ 0x00,
	/* P33 00 */ 0x0A,
	/* P34 00 */ 0x00,
	/* P35 00 */ 0x01,
	/* P36 00 */ 0x00,
	/* P37 00 */ 0x0E,
};

const uint8_t gip1_config[] = {
	/* CMD D5 */ HX8379_SETGIP1,
	/* P01 18 */ 0x18,
	/* P02 18 */ 0x18,
	/* P03 18 */ 0x18,
	/* P04 18 */ 0x18,
	/* P05 18 */ 0x18,
	/* P06 18 */ 0x18,
	/* P07 18 */ 0x18,
	/* P08 18 */ 0x18,
	/* P09 19 */ 0x19,
	/* P10 19 */ 0x19,
	/* P11 18 */ 0x18,
	/* P12 18 */ 0x18,
	/* P13 18 */ 0x18,
	/* P14 18 */ 0x18,
	/* P15 19 */ 0x19,
	/* P16 19 */ 0x19,
	/* P17 01 */ 0x01,
	/* P18 00 */ 0x00,
	/* P19 03 */ 0x03,
	/* P20 02 */ 0x02,
	/* P21 05 */ 0x05,
	/* P22 04 */ 0x04,
	/* P23 07 */ 0x07,
	/* P24 06 */ 0x06,
	/* P25 23 */ 0x23,
	/* P26 22 */ 0x22,
	/* P27 21 */ 0x21,
	/* P28 20 */ 0x20,
	/* P29 18 */ 0x18,
	/* P30 18 */ 0x18,
	/* P31 18 */ 0x18,
	/* P32 18 */ 0x18,
	/* P33 00 */ 0x00,
	/* P34 00 */ 0x00,
};

const uint8_t gip2_config[] = {
	/* CMD D6 */ HX8379_SETGIP2,
	/* P01 18 */ 0x18,
	/* P02 18 */ 0x18,
	/* P03 18 */ 0x18,
	/* P04 18 */ 0x18,
	/* P05 18 */ 0x18,
	/* P06 18 */ 0x18,
	/* P07 18 */ 0x18,
	/* P08 18 */ 0x18,
	/* P09 19 */ 0x19,
	/* P10 19 */ 0x19,
	/* P11 18 */ 0x18,
	/* P12 18 */ 0x18,
	/* P13 19 */ 0x19,
	/* P14 19 */ 0x19,
	/* P15 18 */ 0x18,
	/* P16 18 */ 0x18,
	/* P17 06 */ 0x06,
	/* P18 07 */ 0x07,
	/* P19 04 */ 0x04,
	/* P20 05 */ 0x05,
	/* P21 02 */ 0x02,
	/* P22 03 */ 0x03,
	/* P23 00 */ 0x00,
	/* P24 01 */ 0x01,
	/* P25 20 */ 0x20,
	/* P26 21 */ 0x21,
	/* P27 22 */ 0x22,
	/* P28 23 */ 0x23,
	/* P29 18 */ 0x18,
	/* P30 18 */ 0x18,
	/* P31 18 */ 0x18,
	/* P32 18 */ 0x18,
};

const uint8_t gamma_config[] = {
	/* CMD E0 */ HX8379_SETGAMMA,
	/* P01 00 */ HX8379_GAMMA_VRP0,
	/* P01 16 */ HX8379_GAMMA_VRP1,
	 0x1B, /* VRP2 */
	 0x30, /* VRP3 */
	 0x36, /* VRP4 */
	 0x3F, /* VRP5 */
	 0x24, /* PRP0 */
	 0x40, /* PRP1 */
	 0x09, /* PKP0 */
	 0x0D, /* PKP1 */
	 0x0F, /* PKP2 */
	 0x18, /* PKP3 */
	 0x0E, /* PKP4 */
	 0x11, /* PKP5 */
	 0x12, /* PKP6 */
	 0x11, /* VMP7 */
	 0x14, /* PKP8 */
	 0x07, /* PKP9 */
	 0x12, /* PKP10 */
	 0x13, /* PKP11 */
	 0x18, /* PKP12 */
	 0x00, /* VRN0 */
	 0x17, /* VRN1 */
	 0x1C, /* VRN2 */
	 0x30, /* VRN3 */
	 0x36, /* VRN4 */
	 0x3F, /* VRN5 */
	 0x24, /* PRN0 */
	 0x40, /* PRN1 */
	 0x09, /* PKN0 */
	 0x0C, /* PKN1 */
	 0x0F, /* PKN2 */
	 0x18, /* PKN3 */
	 0x0E, /* PKN4 */
	 0x11, /* PKN5 */
	 0x14, /* PKN6 */
	 0x11, /* PKN7 */
	 0x12, /* PKN8 */
	 0x07, /* PKN9 */
	 0x12, /* PKN10 */
	 0x14, /* PKN11 */
	 0x18, /* PKN12 */
};

const uint8_t vcom_config[] = {
	/* CMD B6 */ HX8379_SETVCOM,
	/* P01 2C */ HX8379_VCMC_F_0_74V,
	/* P02 2C */ HX8379_VCMC_B_0_74V,
	/* P03 00 */ 00,
};

const uint8_t hx8379_bank2[] = {
	/* CMD C1 */ HX8379_SETDGCLUT,
	0x00, /* B_GAMMA0[5:2]  */
	0x09, /* B_GAMMA1[5:2]  */
	0x0F, /* B_GAMMA2[6:2]  */
	0x18, /* B_GAMMA3[7:2]  */
	0x21, /* B_GAMMA4[7:2]  */
	0x2A, /* B_GAMMA5[8:2]  */
	0x34, /* B_GAMMA6[8:2]  */
	0x3C, /* B_GAMMA7[8:2]  */
	0x45, /* B_GAMMA8[8:2]  */
	0x4C, /* B_GAMMA9[8:2]  */
	0x56, /* B_GAMMA10[8:2] */
	0x5E, /* B_GAMMA11[8:2] */
	0x66, /* B_GAMMA12[8:2] */
	0x6E, /* B_GAMMA13[8:2] */
	0x76, /* B_GAMMA14[9:2] */
	0x7E, /* B_GAMMA15[9:2] */
	0x87, /* B_GAMMA16[9:2] */
	0x8E, /* B_GAMMA17[9:2] */
	0x95, /* B_GAMMA18[9:2] */
	0x9D, /* B_GAMMA19[9:2] */
	0xA6, /* B_GAMMA20[9:2] */
	0xAF, /* B_GAMMA21[9:2] */
	0xB7, /* B_GAMMA22[9:2] */
	0xBD, /* B_GAMMA23[8:2] */
	0xC5, /* B_GAMMA24[8:2] */
	0xCE, /* B_GAMMA25[8:2] */
	0xD5, /* B_GAMMA26[8:2] */
	0xDF, /* B_GAMMA27[8:2] */
	0xE7, /* B_GAMMA28[8:2] */
	0xEE, /* B_GAMMA29[8:2] */
	0xF4, /* B_GAMMA30[8:2] */
	0xFA, /* B_GAMMA31[8:2] */
	0xFF, /* B_GAMMA32[8:2] */
	0x0C, /* B_GAMMA0[1:0]  B_GAMMA1[1:0]  B_GAMMA2[1:0]  B_GAMMA3[1:0]  */
	0x31, /* B_GAMMA4[1:0]  B_GAMMA5[1:0]  B_GAMMA6[1:0]  B_GAMMA7[1:0]  */
	0x83, /* B_GAMMA8[1:0]  B_GAMMA9[1:0]  B_GAMMA10[1:0] B_GAMMA11[1:0] */
	0x3C, /* B_GAMMA12[1:0] B_GAMMA13[1:0] B_GAMMA14[1:0] B_GAMMA15[1:0] */
	0x5B, /* B_GAMMA16[1:0] B_GAMMA17[1:0] B_GAMMA18[1:0] B_GAMMA19[1:0] */
	0x56, /* B_GAMMA20[1:0] B_GAMMA21[1:0] B_GAMMA22[1:0] B_GAMMA23[1:0] */
	0x1E, /* B_GAMMA24[1:0] B_GAMMA25[1:0] B_GAMMA26[1:0] B_GAMMA27[1:0] */
	0x5A, /* B_GAMMA28[1:0] B_GAMMA29[1:0] B_GAMMA30[1:0] B_GAMMA31[1:0] */
	0xFF, /* B_GAMMA32[1:0] */
};

const uint8_t hx8379_bank1[] = {
	/* CMD C1 */ HX8379_SETDGCLUT,
	0x00, /* G_GAMMA0[5:2] */
	0x08, /* G_GAMMA1[5:2] */
	0x0F, /* G_GAMMA2[6:2] */
	0x16, /* G_GAMMA3[7:2] */
	0x1F, /* G_GAMMA4[7:2] */
	0x28, /* G_GAMMA5[8:2] */
	0x31, /* G_GAMMA6[8:2] */
	0x39, /* G_GAMMA7[8:2] */
	0x41, /* G_GAMMA8[8:2] */
	0x48, /* G_GAMMA9[8:2] */
	0x51, /* G_GAMMA10[8:2] */
	0x59, /* G_GAMMA11[8:2] */
	0x60, /* G_GAMMA12[8:2] */
	0x68, /* G_GAMMA13[8:2] */
	0x70, /* G_GAMMA14[9:2] */
	0x78, /* G_GAMMA15[9:2] */
	0x7F, /* G_GAMMA16[9:2] */
	0x87, /* G_GAMMA17[9:2] */
	0x8D, /* G_GAMMA18[9:2] */
	0x94, /* G_GAMMA19[9:2] */
	0x9C, /* G_GAMMA20[9:2] */
	0xA3, /* G_GAMMA21[9:2] */
	0xAB, /* G_GAMMA22[9:2] */
	0xB3, /* G_GAMMA23[8:2] */
	0xB9, /* G_GAMMA24[8:2] */
	0xC1, /* G_GAMMA25[8:2] */
	0xC8, /* G_GAMMA26[8:2] */
	0xD0, /* G_GAMMA27[8:2] */
	0xD8, /* G_GAMMA28[8:2] */
	0xE0, /* G_GAMMA29[8:2] */
	0xE8, /* G_GAMMA30[8:2] */
	0xEE, /* G_GAMMA31[8:2] */
	0xF5, /* G_GAMMA32[8:2] */
	0x3B, /* G_GAMMA0[1:0]  G_GAMMA1[1:0]  G_GAMMA2[1:0]  G_GAMMA3[1:0]  */
	0x1A, /* G_GAMMA4[1:0]  G_GAMMA5[1:0]  G_GAMMA6[1:0]  G_GAMMA7[1:0]  */
	0xB6, /* G_GAMMA8[1:0]  G_GAMMA9[1:0]  G_GAMMA10[1:0] G_GAMMA11[1:0] */
	0xA0, /* G_GAMMA12[1:0] G_GAMMA13[1:0] G_GAMMA14[1:0] G_GAMMA15[1:0] */
	0x07, /* G_GAMMA16[1:0] G_GAMMA17[1:0] G_GAMMA18[1:0] G_GAMMA19[1:0] */
	0x45, /* G_GAMMA20[1:0] G_GAMMA21[1:0] G_GAMMA22[1:0] G_GAMMA23[1:0] */
	0xC5, /* G_GAMMA24[1:0] G_GAMMA25[1:0] G_GAMMA26[1:0] G_GAMMA27[1:0] */
	0x37, /* G_GAMMA28[1:0] G_GAMMA29[1:0] G_GAMMA30[1:0] G_GAMMA31[1:0] */
	0x00, /* G_GAMMA32[1:0] */
};

const uint8_t hx8379_bank0[] = {
	/* CMD C1 */ HX8379_SETDGCLUT,
	0x01, /* DGC_EN  (Enable the Digital Gamma Curve function)*/
	0x00, /* R_GAMMA0[5:2] */
	0x07, /* R_GAMMA1[5:2] */
	0x0F, /* R_GAMMA2[6:2] */
	0x16, /* R_GAMMA3[7:2] */
	0x1F, /* R_GAMMA4[7:2] */
	0x27, /* R_GAMMA5[8:2] */
	0x30, /* R_GAMMA6[8:2] */
	0x38, /* R_GAMMA7[8:2] */
	0x40, /* R_GAMMA8[8:2] */
	0x47, /* R_GAMMA9[8:2] */
	0x4E, /* R_GAMMA10[8:2] */
	0x56, /* R_GAMMA11[8:2] */
	0x5D, /* R_GAMMA12[8:2] */
	0x65, /* R_GAMMA13[8:2] */
	0x6D, /* R_GAMMA14[9:2] */
	0x74, /* R_GAMMA15[9:2] */
	0x7D, /* R_GAMMA16[9:2] */
	0x84, /* R_GAMMA17[9:2] */
	0x8A, /* R_GAMMA18[9:2] */
	0x90, /* R_GAMMA19[9:2] */
	0x99, /* R_GAMMA20[9:2] */
	0xA1, /* R_GAMMA21[9:2] */
	0xA9, /* R_GAMMA22[9:2] */
	0xB0, /* R_GAMMA23[8:2] */
	0xB6, /* R_GAMMA24[8:2] */
	0xBD, /* R_GAMMA25[8:2] */
	0xC4, /* R_GAMMA26[8:2] */
	0xCD, /* R_GAMMA27[8:2] */
	0xD4, /* R_GAMMA28[8:2] */
	0xDD, /* R_GAMMA29[8:2] */
	0xE5, /* R_GAMMA30[8:2] */
	0xEC, /* R_GAMMA31[8:2] */
	0xF3, /* R_GAMMA32[8:2] */
	0x36, /* R_GAMMA0[1:0]  R_GAMMA1[1:0]  R_GAMMA2[1:0]  R_GAMMA3[1:0]  */
	0x07, /* R_GAMMA4[1:0]  R_GAMMA5[1:0]  R_GAMMA6[1:0]  R_GAMMA7[1:0]  */
	0x1C, /* R_GAMMA8[1:0]  R_GAMMA9[1:0]  R_GAMMA10[1:0] R_GAMMA11[1:0] */
	0xC0, /* R_GAMMA12[1:0] R_GAMMA13[1:0] R_GAMMA14[1:0] R_GAMMA15[1:0] */
	0x1B, /* R_GAMMA16[1:0] R_GAMMA17[1:0] R_GAMMA18[1:0] R_GAMMA19[1:0] */
	0x01, /* R_GAMMA20[1:0] R_GAMMA21[1:0] R_GAMMA22[1:0] R_GAMMA23[1:0] */
	0xF1, /* R_GAMMA24[1:0] R_GAMMA25[1:0] R_GAMMA26[1:0] R_GAMMA27[1:0] */
	0x34, /* R_GAMMA28[1:0] R_GAMMA29[1:0] R_GAMMA30[1:0] R_GAMMA31[1:0] */
	0x00, /* R_GAMMA32[1:0] */
};

const uint8_t tear_config[] = {HX8379_SET_TEAR, HX8379_TEAR_VBLANK};

static int hx8379_write(const struct device *dev, const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	LOG_WRN("Write not supported, use LCD controller display driver");
	return 0;
}

static int hx8379_blanking_off(const struct device *dev)
{
	const struct hx8379_config *config = dev->config;

	if (config->bl_gpio.port != NULL) {
		return gpio_pin_set_dt(&config->bl_gpio, 1);
	} else {
		return -ENOTSUP;
	}

	return mipi_dsi_dcs_write(config->mipi_dsi, config->channel, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
}

static int hx8379_blanking_on(const struct device *dev)
{
	const struct hx8379_config *config = dev->config;

	if (config->bl_gpio.port != NULL) {
		return gpio_pin_set_dt(&config->bl_gpio, 0);
	} else {
		return -ENOTSUP;
	}

	return mipi_dsi_dcs_write(config->mipi_dsi, config->channel, MIPI_DCS_SET_DISPLAY_OFF, NULL, 0);
}

static int hx8379_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pixel_format)
{
	const struct hx8379_config *config = dev->config;

	if (pixel_format == config->pixel_format) {
		return 0;
	}
	LOG_WRN("Pixel format change not implemented");
	return -ENOTSUP;
}

static int hx8379_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	const struct hx8379_config *config = dev->config;
	uint8_t param[2] = {0};

	/* Note- this simply flips the scan direction of the display
	 * driver. Can be useful if your application needs the display
	 * flipped on the X or Y axis
	 */
	param[0] = HX8379_SET_ADDRESS;
	switch (orientation) {
	case DISPLAY_ORIENTATION_NORMAL:
		/* Default orientation for this display flips image on x axis */
		param[1] = HX8379_FLIP_HORIZONTAL;
		break;
	case DISPLAY_ORIENTATION_ROTATED_90:
		param[1] = HX8379_FLIP_VERTICAL;
		break;
	case DISPLAY_ORIENTATION_ROTATED_180:
		param[1] = 0;
		break;
	case DISPLAY_ORIENTATION_ROTATED_270:
		param[1] = HX8379_FLIP_HORIZONTAL | HX8379_FLIP_VERTICAL;
		break;
	default:
		return -ENOTSUP;
	}
	return mipi_dsi_generic_write(config->mipi_dsi, config->channel, param, 2);
}

static void hx8379_get_capabilities(const struct device *dev,
				     struct display_capabilities *capabilities)
{
	const struct hx8379_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->panel_width;
	capabilities->y_resolution = config->panel_height;
	capabilities->supported_pixel_formats = config->pixel_format;
	capabilities->current_pixel_format = config->pixel_format;
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static const struct display_driver_api hx8379_api = {
	.blanking_on = hx8379_blanking_on,
	.blanking_off = hx8379_blanking_off,
	.write = hx8379_write,
	.get_capabilities = hx8379_get_capabilities,
	.set_pixel_format = hx8379_set_pixel_format,
	.set_orientation = hx8379_set_orientation,
};

static int hx8379_init(const struct device *dev)
{
	const struct hx8379_config *config = dev->config;
	int ret;
	struct mipi_dsi_device mdev;
	uint8_t param[2];

	mdev.data_lanes = config->num_of_lanes;
	mdev.pixfmt = config->pixel_format;
	
	/* HX8379 runs in video mode */
	mdev.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_LPM;

	mdev.timings.hactive = config->panel_width;
	mdev.timings.hbp = HX8379_HBP;
	mdev.timings.hfp = HX8379_HFP;
	mdev.timings.hsync = HX8379_HSYNC;
	mdev.timings.vactive = config->panel_height + 1;
	mdev.timings.vbp = HX8379_VBP;
	mdev.timings.vfp = HX8379_VFP;
	mdev.timings.vsync = HX8379_VSYNC;

	if (config->reset_gpio.port) {
		if (!gpio_is_ready_dt(&config->reset_gpio)) {
			LOG_ERR("Reset GPIO device is not ready!");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("Reset display failed! (%d)", ret);
			return ret;
		}
		k_msleep(11);
		ret = gpio_pin_set_dt(&config->reset_gpio, 1);
		if (ret < 0) {
			LOG_ERR("Enable display failed! (%d)", ret);
			return ret;
		}
		k_msleep(120);
	}

	ret = mipi_dsi_attach(config->mipi_dsi, config->channel, &mdev);
	if (ret < 0) {
		LOG_ERR("Could not attach to MIPI-DSI host");
		return ret;
	}

	/* Enable extended commands */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			enable_extension, sizeof(enable_extension));
	if (ret < 0) {
		return ret;
	}

	/* Set voltage and current targets */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			power_config, sizeof(power_config));
	if (ret < 0) {
		return ret;
	}

	/* Setup display line count and front/back porch size */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			line_config, sizeof(line_config));
	if (ret < 0) {
		return ret;
	}

	/* Setup display cycle counts (in counts of TCON CLK) */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			cycle_config, sizeof(cycle_config));
	if (ret < 0) {
		return ret;
	}

	/* This command is not documented in datasheet, but is included
	 * in the display initialization done by stm32Cube SDK
	 */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			hx8379_cmd1, sizeof(hx8379_cmd1));
	if (ret < 0) {
		return ret;
	}
	
	/* Set panel related register */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			panel_config, sizeof(panel_config));
	if (ret < 0) {
		return ret;
	}
	
	/* This command is not documented in datasheet, but is included
	 * in the display initialization done by stm32Cube SDK
	 */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			hx8379_cmd3, sizeof(hx8379_cmd3));
	if (ret < 0) {
		return ret;
	}

	/* Set group delay values */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			gip0_config, sizeof(gip0_config));
	if (ret < 0) {
		return ret;
	}


	/* Set group clock selections */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			gip1_config, sizeof(gip1_config));
	if (ret < 0) {
		return ret;
	}

	/* Set group clock selections for GS mode */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			gip2_config, sizeof(gip2_config));
	if (ret < 0) {
		return ret;
	}

	/* Set manufacturer supplied gamma values */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			gamma_config, sizeof(gamma_config));
	if (ret < 0) {
		return ret;
	}

	/* Delay for a moment before setting VCOM. It is not clear
	 * from the datasheet why this is required, but without this
	 * delay the panel stops responding to additional commands
	 */
	k_msleep(1);
	/* Set VCOM voltage config */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			vcom_config, sizeof(vcom_config));
	if (ret < 0) {
		return ret;
	}

	/* Write values to R/G/B Digital Gamma Curve Look-Up Table (Set DGC LUT) */
	param[0] = HX8379_SETBANK;
	param[1] = 0x2;
	
	/* Select bank 2 */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			param, sizeof(param));
	if (ret < 0) {
		return ret;
	}
	/* write bank 2 */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			hx8379_bank2, sizeof(hx8379_bank2));
	if (ret < 0) {
		return ret;
	}
	/* Select bank 0 */
	param[1] = 0x0;
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			param, sizeof(param));
	if (ret < 0) {
		return ret;
	}
	/* Select bank 1 */
	param[1] = 0x1;
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			param, sizeof(param));
	if (ret < 0) {
		return ret;
	}
	/* write bank 1 */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			hx8379_bank1, sizeof(hx8379_bank1));
	if (ret < 0) {
		return ret;
	}
	/* Select bank 0 */
	param[1] = 0x0;
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			param, sizeof(param));
	if (ret < 0) {
		return ret;
	}
	/* write bank 0 */
	ret = mipi_dsi_generic_write(config->mipi_dsi, config->channel,
			hx8379_bank0, sizeof(hx8379_bank0));
	if (ret < 0) {
		return ret;
	}

	// TODO continue verif after this mark (bank0-1-2)

	// Exit Sleep Mode
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel,
				MIPI_DCS_EXIT_SLEEP_MODE, NULL, 0);
	if (ret < 0) {
		return ret;
	}
	/* Display need 120ms after exiting sleep mode per datasheet */
	k_sleep(K_MSEC(120));

	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel,
				MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
				
	k_sleep(K_MSEC(120));

	if (config->bl_gpio.port != NULL) {
		ret = gpio_pin_configure_dt(&config->bl_gpio, GPIO_OUTPUT_ACTIVE);
		if (ret < 0) {
			LOG_ERR("Could not configure bl GPIO (%d)", ret);
			return ret;
		}
	}

	return ret;
}

#define HX8379_PANEL(id)							\
	static const struct hx8379_config hx8379_config_##id = {		\
		.mipi_dsi = DEVICE_DT_GET(DT_INST_BUS(id)),			\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(id, reset_gpios, {0}),	\
		.bl_gpio = GPIO_DT_SPEC_INST_GET_OR(id, bl_gpios, {0}),		\
		.num_of_lanes = DT_INST_PROP_BY_IDX(id, data_lanes, 0),		\
		.pixel_format = DT_INST_PROP(id, pixel_format),			\
		.panel_width = DT_INST_PROP(id, width),				\
		.panel_height = DT_INST_PROP(id, height),			\
		.channel = DT_INST_REG_ADDR(id),				\
	};									\
	DEVICE_DT_INST_DEFINE(id,						\
			    &hx8379_init,					\
			    NULL,						\
			    NULL,						\
			    &hx8379_config_##id,				\
			    POST_KERNEL,					\
			    87,			\
			    &hx8379_api);

DT_INST_FOREACH_STATUS_OKAY(HX8379_PANEL)
