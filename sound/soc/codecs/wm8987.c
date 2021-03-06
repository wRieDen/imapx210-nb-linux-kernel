/*
 * wm8987.c -- WM8987 ALSA SoC audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on WM8987.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
//#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/slab.h>
#include <sound/initval.h>
#ifdef CONFIG_HHBF_FAST_REBOOT
#include <asm/reboot.h>
#endif

#include "wm8987.h"

#define AUDIO_NAME "WM8987"
#define WM8987_VERSION "v0.12"
unsigned int system_mute;
unsigned int system_mute_state;
#if 0
enum system_mute_state {
	MUTE_AUDIO_NONE,
	MUTE_AUDIO_ACTIVE,
	MUTE_AUDIO_DONE,
};
#endif
/*
 * Debug
 */

//#define WM8987_DEBUG 1

#ifdef WM8987_DEBUG
#define dbg(format, arg...) \
	printk(KERN_DEBUG AUDIO_NAME ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) \
	printk(KERN_ERR AUDIO_NAME ": " format "\n" , ## arg)
#define info(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) \
	printk(KERN_WARNING AUDIO_NAME ": " format "\n" , ## arg)

/* codec private data */
#ifndef	CONFIG_HHTECH_MINIPMP
struct wm8987_priv {
	unsigned int sysclk;
};
#else
static unsigned wm8987_sysclk;

void (*hhbf_audio_switch)(int flag) = NULL;
EXPORT_SYMBOL(hhbf_audio_switch);
static unsigned short init_reboot = 0;
#endif//CONFIG_HHTECH_MINIPMP

/*
 * wm8987 register cache
 * We can't read the WM8987 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8987_reg[] = {
	0x00b7, 0x0097, 0x0000, 0x0000,  /*  0 */
	0x0000, 0x0008, 0x0000, 0x002a,  /*  4 */
	0x0000, 0x0000, 0x007F, 0x007F,  /*  8 */
	0x000f, 0x000f, 0x0000, 0x0000,  /* 12 */
	0x0080, 0x007b, 0x0000, 0x0032,  /* 16 */
	0x0000, 0x00E0, 0x00E0, 0x00c0,  /* 20 */
	0x0000, 0x000e, 0x0000, 0x0000,  /* 24 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 28 */
	0x0000, 0x0000, 0x0050, 0x0050,  /* 32 */
	0x0050, 0x0050, 0x0050, 0x0050,  /* 36 */
	0x0000, 0x0000, 0x0079,          /* 40 */
};

/*
 * read wm8987 register cache
 */
static inline unsigned int wm8987_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
#ifndef	CONFIG_HHTECH_MINIPMP
	if (reg > WM8987_CACHE_REGNUM)
#else// mhfan
	if (reg > ARRAY_SIZE(wm8987_reg))
#endif//CONFIG_HHTECH_MINIPMP
		return -1;
	return cache[reg];
}

/*
 * write wm8987 register cache
 */
static inline void wm8987_write_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
#ifndef	CONFIG_HHTECH_MINIPMP
	if (reg > WM8987_CACHE_REGNUM)
#else// mhfan
	if (reg > ARRAY_SIZE(wm8987_reg))
#endif//CONFIG_HHTECH_MINIPMP
		return;
	cache[reg] = value;
}

#if 0//def	CONFIG_HHTECH_MINIPMP
static int playtvo = 0;
#endif//CONFIG_HHTECH_MINIPMP

static int wm8987_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];

#if 0 //def CONFIG_SND_SOC_WM8987
	if((10 == reg || 11 == reg) && value == 0x1C2)		value = 0x1C4;
	if((40 == reg || 41 == reg) && value == 0x1C2)		value = 0x1C4;
#endif

#if 0     /* comment by whg HHTECH */
#if( !defined(CONFIG_HHBF_I2C_MCU) || (defined(CONFIG_SND_SOC_WM8987)))
	if (WM8987_PWR2 == reg) {	// XXX: FIXME
	    u16 tmp, *cache = codec->reg_cache;
	    if (reg > ARRAY_SIZE(wm8987_reg)) return -EIO;
	    else tmp = cache[reg];
	#ifdef CONFIG_SND_SOC_WM8987
	    if ( (value & 0x100) && !(tmp & 0x100)) value |=  0x198;
	    if (!(value & 0x080) &&  (tmp & 0x080)) value &= ~0x198;
	    if ( (value & 0x010) && !(tmp & 0x010)) value |=  0x198;
	    if (!(value & 0x008) &&  (tmp & 0x008)) value &= ~0x198;
		if( value == tmp)	return 0;
	#else
	    if ( (value & 0x100) && !(tmp & 0x100)) value |=  0x180;
	    if (!(value & 0x100) &&  (tmp & 0x100)) value &= ~0x180;
	    if ( (value & 0x040) && !(tmp & 0x040)) value |=  0x07a;
	    if (!(value & 0x040) &&  (tmp & 0x040)) value &= ~0x07a;
	    if ( (value & ~0x1a) ==  (tmp & ~0x1a)) return 0;
	#endif
	value = 0x198;
	}
#endif//CONFIG_HHBF_I2C_MCU
#endif    /* comment by WangGang   */

	/* data is
	 *   D15..D9 WM8987 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x00ff;

#ifdef	WM8987_DEBUG
	if (value == wm8987_read_reg_cache(codec, reg)) return 0;

	printk(KERN_INFO AUDIO_NAME ": 0x%02x%02x, R%02d <= 0x%03x\n",
		data[0], data[1], reg, value);
#endif//WM8987_DEBUG

	wm8987_write_reg_cache (codec, reg, value);

#ifdef	CONFIG_HHTECH_MINIPMP
	if (init_reboot) return 0;
#endif//CONFIG_HHTECH_MINIPMP

	if (codec->hw_write(codec->control_data, data, 2) == 2)
		return 0;
	else
		return -EIO;

//return 0;
}

#define wm8987_reset(c)	wm8987_write(c, WM8987_RESET, 0)

/*
 * WM8987 Controls
 */
static const char *wm8987_bass[] = {"Linear Control", "Adaptive Boost"};
static const char *wm8987_bass_filter[] = { "130Hz @ 48kHz", "200Hz @ 48kHz" };
static const char *wm8987_treble[] = {"8kHz", "4kHz"};
static const char *wm8987_3d_lc[] = {"200Hz", "500Hz"};
static const char *wm8987_3d_uc[] = {"2.2kHz", "1.5kHz"};
static const char *wm8987_3d_func[] = {"Capture", "Playback"};
static const char *wm8987_alc_func[] = {"Off", "Right", "Left", "Stereo"};
static const char *wm8987_ng_type[] = {"Constant PGA Gain",
	"Mute ADC Output"};
static const char *wm8987_line_mux[] = {"Line 1", "Line 2", "Line 3", "PGA",
	"Differential"};
static const char *wm8987_pga_sel[] = {"Line 1", "Line 2", "Line 3",
	"Differential"};
static const char *wm8987_out3[] = {"VREF", "ROUT1 + Vol", "MonoOut",
	"ROUT1"};
static const char *wm8987_diff_sel[] = {"Line 1", "Line 2"};
#if 0
static const char *wm8987_adcpol[] = {"Normal", "L Invert", "R Invert",
	"L + R Invert"};
static const char *wm8987_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};
#endif
static const char *wm8987_mono_mux[] = {"Stereo", "Mono (Left)",
	"Mono (Right)", "Digital Mono"};

static const struct soc_enum wm8987_enum[] = {
SOC_ENUM_SINGLE(WM8987_BASS, 7, 2, wm8987_bass),
SOC_ENUM_SINGLE(WM8987_BASS, 6, 2, wm8987_bass_filter),
SOC_ENUM_SINGLE(WM8987_TREBLE, 6, 2, wm8987_treble),
SOC_ENUM_SINGLE(WM8987_3D, 5, 2, wm8987_3d_lc),
SOC_ENUM_SINGLE(WM8987_3D, 6, 2, wm8987_3d_uc),
SOC_ENUM_SINGLE(WM8987_3D, 7, 2, wm8987_3d_func),
SOC_ENUM_SINGLE(WM8987_ALC1, 7, 4, wm8987_alc_func),
SOC_ENUM_SINGLE(WM8987_NGATE, 1, 2, wm8987_ng_type),
SOC_ENUM_SINGLE(WM8987_LOUTM1, 0, 5, wm8987_line_mux),
SOC_ENUM_SINGLE(WM8987_ROUTM1, 0, 5, wm8987_line_mux),
SOC_ENUM_SINGLE(WM8987_LADCIN, 6, 4, wm8987_pga_sel), /* 10 */
SOC_ENUM_SINGLE(WM8987_RADCIN, 6, 4, wm8987_pga_sel),
SOC_ENUM_SINGLE(WM8987_ADCTL2, 7, 4, wm8987_out3),
SOC_ENUM_SINGLE(WM8987_ADCIN, 8, 2, wm8987_diff_sel),
//SOC_ENUM_SINGLE(WM8987_ADCDAC, 5, 4, wm8987_adcpol),
//SOC_ENUM_SINGLE(WM8987_ADCDAC, 1, 4, wm8987_deemph),
SOC_ENUM_SINGLE(WM8987_ADCIN, 6, 4, wm8987_mono_mux), /* 16 */

};

static const struct snd_kcontrol_new wm8987_snd_controls[] = {

//SOC_DOUBLE_R("Capture Volume", WM8987_LINVOL, WM8987_RINVOL, 0, 63, 0),
//SOC_DOUBLE_R("Capture ZC Switch", WM8987_LINVOL, WM8987_RINVOL, 6, 1, 0),
//SOC_DOUBLE_R("Capture Switch", WM8987_LINVOL, WM8987_RINVOL, 7, 1, 1),
#ifndef	CONFIG_HHTECH_MINIPMP
SOC_DOUBLE_R("Headphone Playback ZC Switch", WM8987_LOUT1V,
	WM8987_ROUT1V, 7, 1, 0),
#endif
//SOC_DOUBLE_R("Speaker Playback ZC Switch", WM8987_LOUT2V,
//	WM8987_ROUT2V, 7, 1, 0),

//SOC_ENUM("Playback De-emphasis", wm8987_enum[15]),

//SOC_ENUM("Capture Polarity", wm8987_enum[14]),
//SOC_SINGLE("Playback 6dB Attenuate Volume", WM8987_ADCDAC, 7, 1, 0),
//SOC_SINGLE("Capture 6dB Attenuate Volume", WM8987_ADCDAC, 8, 1, 0),
//SOC_SINGLE("Mute Playback Volume", WM8987_ADCDAC, 3, 1, 0),
SOC_SINGLE("Mute Volume" , WM8987_ADCDAC, 3, 1, 0),

#ifndef	CONFIG_HHTECH_MINIPMP
SOC_DOUBLE_R("PCM Volume", WM8987_LDAC, WM8987_RDAC, 0, 255, 0),
#else// mhfan, XXX:
SOC_DOUBLE_R("Master Playback Volume", WM8987_LDAC, WM8987_RDAC, 0, 510, 0),
#endif//CONFIG_HHTECH_MINIPMP
#if 0
SOC_ENUM("Bass Boost", wm8987_enum[0]),
SOC_ENUM("Bass Filter", wm8987_enum[1]),
SOC_SINGLE("Bass Volume", WM8987_BASS, 0, 15, 1),

SOC_SINGLE("Treble Volume", WM8987_TREBLE, 0, 15, 0),
SOC_ENUM("Treble Cut-off", wm8987_enum[2]),

SOC_SINGLE("3D Switch", WM8987_3D, 0, 1, 0),
SOC_SINGLE("3D Volume", WM8987_3D, 1, 15, 0),
SOC_ENUM("3D Lower Cut-off", wm8987_enum[3]),
SOC_ENUM("3D Upper Cut-off", wm8987_enum[4]),
SOC_ENUM("3D Mode", wm8987_enum[5]),

SOC_SINGLE("ALC Capture Target Volume", WM8987_ALC1, 0, 7, 0),
SOC_SINGLE("ALC Capture Max Volume", WM8987_ALC1, 4, 7, 0),
SOC_ENUM("ALC Capture Function", wm8987_enum[6]),
SOC_SINGLE("ALC Capture ZC Switch", WM8987_ALC2, 7, 1, 0),
SOC_SINGLE("ALC Capture Hold Time", WM8987_ALC2, 0, 15, 0),
SOC_SINGLE("ALC Capture Decay Time", WM8987_ALC3, 4, 15, 0),
SOC_SINGLE("ALC Capture Attack Time", WM8987_ALC3, 0, 15, 0),
SOC_SINGLE("ALC Capture NG Threshold", WM8987_NGATE, 3, 31, 0),
SOC_ENUM("ALC Capture NG Type", wm8987_enum[4]),
SOC_SINGLE("ALC Capture NG Switch", WM8987_NGATE, 0, 1, 0),
#endif
SOC_DOUBLE_R("Input Volume", WM8987_LADC, WM8987_RADC, 0, 512, 0),
//SOC_SINGLE("Left Capture Volume", WM8987_LADC, 0, 255, 0),
//SOC_SINGLE("Right Capture Volume", WM8987_RADC, 0, 255, 0),
#if 0
SOC_SINGLE("ZC Timeout Switch", WM8987_ADCTL1, 0, 1, 0),
SOC_SINGLE("Playback Invert Switch", WM8987_ADCTL1, 1, 1, 0),

SOC_SINGLE("Right Speaker Playback Invert Switch", WM8987_ADCTL2, 4, 1, 0),
#endif
/* Unimplemented */
/* ADCDAC Bit 0 - ADCHPD */
/* ADCDAC Bit 4 - HPOR */
/* ADCTL1 Bit 2,3 - DATSEL */
/* ADCTL1 Bit 4,5 - DMONOMIX */
/* ADCTL1 Bit 6,7 - VSEL */
/* ADCTL2 Bit 2 - LRCM */
/* ADCTL2 Bit 3 - TRI */
/* ADCTL3 Bit 5 - HPFLREN */
/* ADCTL3 Bit 6 - VROI */
/* ADCTL3 Bit 7,8 - ADCLRM */
/* ADCIN Bit 4 - LDCM */
/* ADCIN Bit 5 - RDCM */
#if 0
SOC_DOUBLE_R("Mic Boost", WM8987_LADCIN, WM8987_RADCIN, 4, 3, 0),

SOC_DOUBLE_R("Bypass Left Playback Volume", WM8987_LOUTM1,
	WM8987_LOUTM2, 4, 7, 1),
SOC_DOUBLE_R("Bypass Right Playback Volume", WM8987_ROUTM1,
	WM8987_ROUTM2, 4, 7, 1),
SOC_DOUBLE_R("Bypass Mono Playback Volume", WM8987_MOUTM1,
	WM8987_MOUTM2, 4, 7, 1),

SOC_SINGLE("Mono Playback ZC Switch", WM8987_MOUTV, 7, 1, 0),
#endif
#ifndef	CONFIG_HHTECH_MINIPMP
SOC_DOUBLE_R("Headphone Playback Volume", WM8987_LOUT1V, WM8987_ROUT1V,
	0, 127, 0),
#endif
//SOC_DOUBLE_R("Speaker Playback Volume", WM8987_LOUT2V, WM8987_ROUT2V,
//	0, 512, 0),

//SOC_SINGLE("Mono Playback Volume", WM8987_MOUTV, 0, 127, 0),

};

/* add non dapm controls */
static int wm8987_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(wm8987_snd_controls); i++) {
		err = snd_ctl_add(codec->card,
				snd_soc_cnew(&wm8987_snd_controls[i],codec, NULL));
		if (err < 0)
			return err;
	}
	return 0;
}

/*
 * DAPM Controls
 */

/* Left Mixer */
static const struct snd_kcontrol_new wm8987_left_mixer_controls[] = {
#if 1
SOC_DAPM_SINGLE("Playback Switch", WM8987_LOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", WM8987_LOUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", WM8987_LOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", WM8987_LOUTM2, 7, 1, 0),
#endif
};

/* Right Mixer */
static const struct snd_kcontrol_new wm8987_right_mixer_controls[] = {
#if 1
SOC_DAPM_SINGLE("Left Playback Switch", WM8987_ROUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", WM8987_ROUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Playback Switch", WM8987_ROUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", WM8987_ROUTM2, 7, 1, 0),
#endif
};

/* Mono Mixer */
static const struct snd_kcontrol_new wm8987_mono_mixer_controls[] = {
#if 0
SOC_DAPM_SINGLE("Left Playback Switch", WM8987_MOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", WM8987_MOUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", WM8987_MOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", WM8987_MOUTM2, 7, 1, 0),
#endif
};
#if 0
/* Left Line Mux */
static const struct snd_kcontrol_new wm8987_left_line_controls =
SOC_DAPM_ENUM("Route", wm8987_enum[8]);

/* Right Line Mux */
static const struct snd_kcontrol_new wm8987_right_line_controls =
SOC_DAPM_ENUM("Route", wm8987_enum[9]);

/* Left PGA Mux */
static const struct snd_kcontrol_new wm8987_left_pga_controls =
SOC_DAPM_ENUM("Route", wm8987_enum[10]);

/* Right PGA Mux */
static const struct snd_kcontrol_new wm8987_right_pga_controls =
SOC_DAPM_ENUM("Route", wm8987_enum[11]);

/* Out 3 Mux */
#if 0
static const struct snd_kcontrol_new wm8987_out3_controls =
SOC_DAPM_ENUM("Route", wm8987_enum[12]);
#endif
/* Differential Mux */
static const struct snd_kcontrol_new wm8987_diffmux_controls =
SOC_DAPM_ENUM("Route", wm8987_enum[13]);

/* Mono ADC Mux */
static const struct snd_kcontrol_new wm8987_monomux_controls =
//SOC_DAPM_ENUM("Route", wm8987_enum[16]);
SOC_DAPM_ENUM("Route", wm8987_enum[14]);
#endif
static const struct snd_soc_dapm_widget wm8987_dapm_widgets[] = {
#if 1
	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
		&wm8987_left_mixer_controls[0],
		ARRAY_SIZE(wm8987_left_mixer_controls)),
#endif

	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
		&wm8987_right_mixer_controls[0],
		ARRAY_SIZE(wm8987_right_mixer_controls)),
#if 0
	SND_SOC_DAPM_MIXER("Mono Mixer", WM8987_PWR2, 2, 0,
		&wm8987_mono_mixer_controls[0],
		ARRAY_SIZE(wm8987_mono_mixer_controls)),
#endif
	SND_SOC_DAPM_PGA("Right Out 2", WM8987_PWR2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", WM8987_PWR2, 4, 0, NULL, 0),

//	SND_SOC_DAPM_PGA("Right Out 1", WM8987_PWR2, 5, 0,
//		wm8987_rout1_ctrls, ARRAY_SIZE(wm8987_rout1_ctrls)),
//	SND_SOC_DAPM_PGA("Left Out 1", WM8987_PWR2, 6, 0,
//		wm8987_lout1_ctrls, ARRAY_SIZE(wm8987_lout1_ctrls)),
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", WM8987_PWR2, 7, 0),
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", WM8987_PWR2, 8, 0),

	SND_SOC_DAPM_MICBIAS("Mic Bias", WM8987_PWR1, 1, 0),
	//SND_SOC_DAPM_MICBIAS("Mic Bias", WM8987_PWR1, 1, 1), //lzcx 
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", WM8987_PWR1, 2, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", WM8987_PWR1, 3, 0),
#if 0
	SND_SOC_DAPM_MUX("Left PGA Mux", WM8987_PWR1, 5, 0,
		&wm8987_left_pga_controls),
	SND_SOC_DAPM_MUX("Right PGA Mux", WM8987_PWR1, 4, 0,
		&wm8987_right_pga_controls),
	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0,
		&wm8987_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0,
		&wm8987_right_line_controls),
#endif
//	SND_SOC_DAPM_MUX("Out3 Mux", SND_SOC_NOPM, 0, 0, &wm8987_out3_controls),
	SND_SOC_DAPM_PGA("Out 3", WM8987_PWR2, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono Out 1", WM8987_PWR2, 2, 0, NULL, 0),
#if 0
	SND_SOC_DAPM_MUX("Differential Mux", SND_SOC_NOPM, 0, 0,
		&wm8987_diffmux_controls),
	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0,
		&wm8987_monomux_controls),

	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0,
		&wm8987_monomux_controls),
#endif
	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_OUTPUT("MONO"),
	SND_SOC_DAPM_OUTPUT("OUT3"),

	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("LINPUT2"),
	SND_SOC_DAPM_INPUT("LINPUT3"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT2"),
	SND_SOC_DAPM_INPUT("RINPUT3"),
};

static const char *audio_map[][3] = {
	/* left mixer */
	{"Left Mixer", "Playback Switch", "Left DAC"},
	{"Left Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Left Mixer", "Right Playback Switch", "Right DAC"},
	{"Left Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* right mixer */
	{"Right Mixer", "Left Playback Switch", "Left DAC"},
	{"Right Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Right Mixer", "Playback Switch", "Right DAC"},
	{"Right Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* left out 1 */
	{"Left Out 1", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},

	/* left out 2 */
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT2", NULL, "Left Out 2"},

	/* right out 1 */
	{"Right Out 1", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},

	/* right out 2 */
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT2", NULL, "Right Out 2"},

	/* mono mixer */
	{"Mono Mixer", "Left Playback Switch", "Left DAC"},
	{"Mono Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Mono Mixer", "Right Playback Switch", "Right DAC"},
	{"Mono Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* mono out */
	{"Mono Out 1", NULL, "Mono Mixer"},
	{"MONO1", NULL, "Mono Out 1"},

	/* out 3 */
	{"Out3 Mux", "VREF", "VREF"},
	{"Out3 Mux", "ROUT1 + Vol", "ROUT1"},
	{"Out3 Mux", "ROUT1", "Right Mixer"},
	{"Out3 Mux", "MonoOut", "MONO1"},
	{"Out 3", NULL, "Out3 Mux"},
	{"OUT3", NULL, "Out 3"},

	/* Left Line Mux */
	{"Left Line Mux", "Line 1", "LINPUT1"},
	{"Left Line Mux", "Line 2", "LINPUT2"},
	{"Left Line Mux", "Line 3", "LINPUT3"},
	{"Left Line Mux", "PGA", "Left PGA Mux"},
	{"Left Line Mux", "Differential", "Differential Mux"},

	/* Right Line Mux */
	{"Right Line Mux", "Line 1", "RINPUT1"},
	{"Right Line Mux", "Line 2", "RINPUT2"},
	{"Right Line Mux", "Line 3", "RINPUT3"},
	{"Right Line Mux", "PGA", "Right PGA Mux"},
	{"Right Line Mux", "Differential", "Differential Mux"},

	/* Left PGA Mux */
	{"Left PGA Mux", "Line 1", "LINPUT1"},
	{"Left PGA Mux", "Line 2", "LINPUT2"},
	{"Left PGA Mux", "Line 3", "LINPUT3"},
	{"Left PGA Mux", "Differential", "Differential Mux"},

	/* Right PGA Mux */
	{"Right PGA Mux", "Line 1", "RINPUT1"},
	{"Right PGA Mux", "Line 2", "RINPUT2"},
	{"Right PGA Mux", "Line 3", "RINPUT3"},
	{"Right PGA Mux", "Differential", "Differential Mux"},

	/* Differential Mux */
	{"Differential Mux", "Line 1", "LINPUT1"},
	{"Differential Mux", "Line 1", "RINPUT1"},
	{"Differential Mux", "Line 2", "LINPUT2"},
	{"Differential Mux", "Line 2", "RINPUT2"},

	/* Left ADC Mux */
	{"Left ADC Mux", "Stereo", "Left PGA Mux"},
	{"Left ADC Mux", "Mono (Left)", "Left PGA Mux"},
	{"Left ADC Mux", "Digital Mono", "Left PGA Mux"},

	/* Right ADC Mux */
	{"Right ADC Mux", "Stereo", "Right PGA Mux"},
	{"Right ADC Mux", "Mono (Right)", "Right PGA Mux"},
	{"Right ADC Mux", "Digital Mono", "Right PGA Mux"},

	/* ADC */
	{"Left ADC", NULL, "Left ADC Mux"},
	{"Right ADC", NULL, "Right ADC Mux"},

	/* terminator */
	{NULL, NULL, NULL},
};

static int wm8987_add_widgets(struct snd_soc_codec *codec)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(wm8987_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &wm8987_dapm_widgets[i]);
	}
#if 0
	/* set up audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0],
			audio_map[i][1], audio_map[i][2]);
	}
#endif

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));
	snd_soc_dapm_new_widgets(codec);
	return 0;
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:5;
	u8 usb:1;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0x6, 0x0},
	{11289600, 8000, 1408, 0x16, 0x0},
	{18432000, 8000, 2304, 0x7, 0x0},
	{16934400, 8000, 2112, 0x17, 0x0},
	{12000000, 8000, 1500, 0x6, 0x1},

	/* 11.025k */
	{11289600, 11025, 1024, 0x18, 0x0},
	{16934400, 11025, 1536, 0x19, 0x0},
	{12000000, 11025, 1088, 0x19, 0x1},

	/* 16k */
	{12288000, 16000, 768, 0xa, 0x0},
	{18432000, 16000, 1152, 0xb, 0x0},
	{12000000, 16000, 750, 0xa, 0x1},

	/* 22.05k */
	{11289600, 22050, 512, 0x1a, 0x0},
	{16934400, 22050, 768, 0x1b, 0x0},
	{12000000, 22050, 544, 0x1b, 0x1},

	/* 32k */
	{12288000, 32000, 384, 0xc, 0x0},
	{18432000, 32000, 576, 0xd, 0x0},
	{12000000, 32000, 375, 0xc, 0x1},	// mhfan

	/* 44.1k */
	{11289600, 44100, 256, 0x10, 0x0},
	{16934400, 44100, 384, 0x11, 0x0},
	{12000000, 44100, 272, 0x11, 0x1},

	/* 48k */
	{12288000, 48000, 256, 0x0, 0x0},
	{18432000, 48000, 384, 0x1, 0x0},
	{12000000, 48000, 250, 0x0, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0x1e, 0x0},
	{16934400, 88200, 192, 0x1f, 0x0},
	{12000000, 88200, 136, 0x1f, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0xe, 0x0},
	{18432000, 96000, 192, 0xf, 0x0},
	{12000000, 96000, 125, 0xe, 0x1},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	printk(KERN_ERR "wm8987: could not get coeff for mclk %d @ rate %d\n",
		mclk, rate);
	return -EINVAL;
}

#if 1 
static int wm8987_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;
	return 0;
}

#endif
static int wm8987_set_dai_sysclk(struct snd_soc_codec_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
#ifndef	CONFIG_HHTECH_MINIPMP
	struct snd_soc_codec *codec = codec_dai->codec;
	struct wm8987_priv *wm8987 = codec->private_data;
#endif//CONFIG_HHTECH_MINIPMP
	printk("freq is %ld \n",freq);

	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
#ifndef	CONFIG_HHTECH_MINIPMP
		wm8987->sysclk = freq;
#else// mhfan
	case 24576000:
		wm8987_sysclk = freq;
#endif//CONFIG_HHTECH_MINIPMP
		return 0;
	}
	return -EINVAL;
}

static int wm8987_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

#if 1 
#ifdef	CONFIG_HHTECH_MINIPMP
	iface |= 0x20;	// XXX: mhfan
#endif//CONFIG_HHTECH_MINIPMP
#endif //lzcx
	if(iface != wm8987_read_reg_cache(codec, WM8987_IFACE))
		wm8987_write(codec, WM8987_IFACE, iface);
	return 0;
}

static int wm8987_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	static int ifirst = 1;
	u16 valface, valrate;
	u16 iface = (valface = wm8987_read_reg_cache(codec, WM8987_IFACE)) & 0x1f3;
	u16 srate = (valrate = wm8987_read_reg_cache(codec, WM8987_SRATE)) & 0x1c0;
	printk(KERN_INFO "----params is %d\n", params_rate(params));
#ifndef	CONFIG_HHTECH_MINIPMP
	struct wm8987_priv *wm8987 = codec->private_data;

	int coeff = get_coeff(wm8987->sysclk, params_rate(params));
#else// mhfan
#if 1
	int coeff;
	if (params_rate(params)== 96000)
		coeff = get_coeff(wm8987_sysclk, 32000);
	else
	 	coeff = get_coeff(wm8987_sysclk, params_rate(params));
#endif
//	int coeff = get_coeff(wm8987_sysclk, params_rate(params));
#endif//CONFIG_HHTECH_MINIPMP


	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
#if 0
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x000c;
		break;
#endif
	}
	if(1 == ifirst || valface != iface) 
		wm8987_write(codec, WM8987_IFACE, iface);
	if(coeff >= 0)	{
		srate = srate | ((coeff_div[coeff].sr << 1) | coeff_div[coeff].usb);
		printk("state write is 0x%x\n",srate);
		if(valrate != srate || 1 == ifirst)	
		{
			printk("valrate is not equal the value of the srate \n");
			wm8987_write(codec, WM8987_SRATE, srate); 
	}
}
	ifirst = 0;

	return 0;
}

static int wm8987_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = wm8987_read_reg_cache(codec, WM8987_ADCDAC) & 0xfff7;
#if 0 	/* comment by mhfan */
	if (mute)
		wm8987_write(codec, WM8987_ADCDAC, mute_reg | 0x8);
	else
		wm8987_write(codec, WM8987_ADCDAC, mute_reg);
#else
//	static int _mute_flag = -1;

//	if(mute == _mute_flag)	return 0;
//	_mute_flag = mute;
	printk(KERN_INFO "---------wm8987_mute!\n");	
	if (mute) {
//		system_mute_state = MUTE_AUDIO_DONE;
	//	wm8987_write(codec, WM8987_ADCDAC, mute_reg | 0x8);
		if (hhbf_audio_switch) hhbf_audio_switch(0);
	} else {if (hhbf_audio_switch) hhbf_audio_switch(1);
		if (system_mute == 2){
			printk(KERN_INFO "---now, the system is mute,it could not open the volume!\n");
			return 0;
		}
		else {
			printk(KERN_INFO "---congratulation, you can listen to the music!\n");
		wm8987_write(codec, WM8987_ADCDAC, mute_reg);
		}
	}
#endif	/* comment by mhfan */
	return 0;
}
#if defined (CONFIG_CPU_IMAPX200)
static int wm8987_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	//u16 reg = wm8731_read_reg_cache(codec, WM8731_PWR) & 0xff7f;

	u16 reg = wm8987_read_reg_cache(codec, WM8987_PWR1) & 0xfe3e;
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* vref/mid, osc on, dac unmute */
		
		wm8987_write(codec, WM8987_PWR1, reg | 0x00c0);
		//wm8731_write(codec, WM8731_PWR, reg);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/* everything off except vref/vmid, */
		wm8987_write(codec, WM8987_PWR1, reg | 0x0141);
		break;
	case SND_SOC_BIAS_OFF:
		/* everything off, dac mute, inactive */
		//wm8987_write(codec, WM8987_ACTIVE1, 0x0);
		wm8987_write(codec, WM8987_PWR1, 0x0001);
		break;
	}
	codec->bias_level = level;
	return 0;
}


#else
static int wm8987_dapm_event(struct snd_soc_codec *codec, int event)
{
	u16 pwr_reg = wm8987_read_reg_cache(codec, WM8987_PWR1) & 0xfe3e;
	pwr_reg |= 0x2;//lzcx micbias on
	switch (event) {
	case SNDRV_CTL_POWER_D0: /* full On */
		/* set vmid to 50k and unmute dac */
		wm8987_write(codec, WM8987_PWR1, pwr_reg | 0x00c0);
		break;
	case SNDRV_CTL_POWER_D1: /* partial On */
		wm8987_write(codec, WM8987_PWR1, pwr_reg | 0x01c0); 
		break;
	case SNDRV_CTL_POWER_D2: /* partial On */
		/* set vmid to 5k for quick power up */
		wm8987_write(codec, WM8987_PWR1, pwr_reg | 0x01c1);
		break;
	case SNDRV_CTL_POWER_D3hot: /* Off, with power */
		/* mute dac and set vmid to 500k, enable VREF */
		wm8987_write(codec, WM8987_PWR1, pwr_reg | 0x0141);
		break;
	case SNDRV_CTL_POWER_D3cold: /* Off, without power */
		wm8987_write(codec, WM8987_PWR1, 0x0001);
		break;
	}
//	codec->dapm_state = event;
	return 0;
}
#endif
#define WM8987_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define WM8987_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)
static struct snd_soc_dai_ops wm8987_ops = {
	.hw_params = wm8987_pcm_hw_params,
	.set_fmt = wm8987_set_dai_fmt,
	.set_sysclk = wm8987_set_dai_sysclk,
	.digital_mute = wm8987_mute,
	.set_clkdiv = wm8987_set_dai_clkdiv,
};


struct snd_soc_dai wm8987_dai = {
	.name = "WM8987",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = WM8987_RATES,
		.formats = WM8987_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = WM8987_RATES,
		.formats = WM8987_FORMATS,},
	.ops = &wm8987_ops,	

#if 0
	.dai_ops = {
		.digital_mute = wm8987_mute,
		.set_fmt = wm8987_set_dai_fmt,
		.set_sysclk = wm8987_set_dai_sysclk,
		.set_clkdiv = wm8987_set_dai_clkdiv,
	},
#endif
};
EXPORT_SYMBOL_GPL(wm8987_dai);

static void wm8987_work(struct work_struct *work)
{
	struct snd_soc_codec *codec =
//#if 	LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
//		(struct snd_soc_codec*)work;	// XXX:
//#else//	XXX: mhfan
		container_of(work, struct snd_soc_codec, delayed_work.work);
//#endif//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#if defined (CONFIG_CPU_IMAPX200)
	wm8987_set_bias_level(codec, SND_SOC_BIAS_ON);
#else
	wm8987_dapm_event(codec, codec->dapm_state);
#endif
}

static int wm8987_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
#if defined (CONFIG_CPU_IMAPX200)

	wm8987_set_bias_level(codec, SND_SOC_BIAS_OFF);
#else
	wm8987_dapm_event(codec, SNDRV_CTL_POWER_D3cold);
#endif
	return 0;
}

static int wm8987_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i, reg;
	u8 data[2];
	u16 *cache = codec->reg_cache;
	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8987_reg); i++) {
		if (i == WM8987_RESET)
			continue;
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->hw_write(codec->control_data, data, 2);
	}

	reg = wm8987_read_reg_cache(codec, WM8987_PWR2);
	wm8987_write(codec, WM8987_PWR1, reg | 0x0198);	// XXX: MIC BIAS
	reg = wm8987_read_reg_cache(codec, WM8987_PWR1);
	wm8987_write(codec, WM8987_PWR1, reg | 0x00fe);	// XXX: MIC BIAS
#if defined (CONFIG_CPU_IMAPX200)
	wm8987_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	wm8987_set_bias_level(codec, codec->suspend_bias_level);
#else
	 wm8987_dapm_event(codec, SNDRV_CTL_POWER_D3hot);

	if (codec->suspend_dapm_state == SNDRV_CTL_POWER_D0) {
		wm8987_dapm_event(codec, SNDRV_CTL_POWER_D2);
//		codec->dapm_state = SNDRV_CTL_POWER_D0;
		schedule_delayed_work(&codec->delayed_work, msecs_to_jiffies(1000));
	}
#endif
	return 0;
}

/*
 * initialise the WM8987 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int wm8987_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->card->codec;
	int reg, ret = 0;


	codec->name = "WM8987";
	codec->owner = THIS_MODULE;
	codec->read = wm8987_read_reg_cache;
	codec->write = wm8987_write;
#if defined (CONFIG_CPU_IMAPX200)	
	codec->set_bias_level = wm8987_set_bias_level;
#else
	codec->dapm_event = wm8987_dapm_event;
#endif
	codec->dai = &wm8987_dai;
	codec->num_dai = 1;
#ifndef	CONFIG_HHTECH_MINIPMP
	//codec->reg_cache_size = ARRAY_SIZE(wm8987_reg);

	codec->reg_cache =
			kzalloc(sizeof(u16) * ARRAY_SIZE(wm8987_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL)
		return -ENOMEM;
	memcpy(codec->reg_cache, wm8987_reg,
		sizeof(u16) * ARRAY_SIZE(wm8987_reg));
	codec->reg_cache_size = sizeof(u16) * ARRAY_SIZE(wm8987_reg);
#else// XXX: mhfan
	codec->reg_cache = (void*)wm8987_reg;
	codec->reg_cache_size = ARRAY_SIZE(wm8987_reg);

#ifdef CONFIG_HHBF_FAST_REBOOT
	if (_bfin_swrst & FAST_REBOOT_FLAG) init_reboot = 1;
	// XXX: read register values from codec?
#endif
#endif//CONFIG_HHTECH_MINIPMP

	wm8987_reset(codec);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "wm8987: failed to create pcms\n");
		goto pcm_err;
	}
#if defined (CONFIG_CPU_IMAPX200)
	wm8987_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
#else
	wm8987_dapm_event(codec, SNDRV_CTL_POWER_D1);	// XXX:
#endif
	//codec->dapm_state = SNDRV_CTL_POWER_D3hot;
	schedule_delayed_work(&codec->delayed_work, msecs_to_jiffies(1000));

#ifdef	CONFIG_HHTECH_MINIPMP
	// init first mutes
	wm8987_write(codec, WM8987_PWR1, 0x01fe);	// XXX: MIC BIAS
	wm8987_write(codec, WM8987_LOUTM1, 0x0150);
	wm8987_write(codec, WM8987_ROUTM2, 0x0150);

	wm8987_write(codec, WM8987_ADCTL1, 0x00c1);	// XXX:
	wm8987_write(codec, WM8987_ADCIN, 0x0000);	// XXX:

	wm8987_write(codec, WM8987_LADCIN,  0x0060);	// XXX: LINP2
	wm8987_write(codec, WM8987_RADCIN,  0x0060);	// XXX: LINP2
	wm8987_write(codec, WM8987_LADC,  0x01f0);
	wm8987_write(codec, WM8987_RADC,  0x01f0);
#endif//CONFIG_HHTECH_MINIPMP

	wm8987_write(codec, WM8987_LDAC,  0x01f0);
	wm8987_write(codec, WM8987_RDAC,  0x01f0);

	wm8987_write(codec, WM8987_LINVOL, 0x0157);
	wm8987_write(codec, WM8987_RINVOL, 0x0157);

	wm8987_write(codec, WM8987_3D,  0x80);

	wm8987_write(codec, WM8987_ROUT2V, 0x17f);
	wm8987_write(codec, WM8987_LOUT2V, 0x007f); //lzcx
	wm8987_write(codec, WM8987_PWR2, 0x0198); //lzcx
	wm8987_write(codec, WM8987_PWR1, 0x00fe); //lzcx
	wm8987_write(codec, WM8987_ADCDAC, 0x00); //lzcx
	
	wm8987_add_controls(codec);
	wm8987_add_widgets(codec);
////	ret = snd_soc_register_card(socdev);
#if 0
	ret = snd_soc_register_codec(codec);
//	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "wm8987: failed to register card\n");
		goto card_err;
	}

	ret = snd_soc_register_dai(&wm8987_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		snd_soc_unregister_codec(codec);
		return ret;
	}
#endif

#ifdef	CONFIG_HHTECH_MINIPMP
	init_reboot = 0;
#endif//CONFIG_HHTECH_MINIPMP

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
#ifndef	CONFIG_HHTECH_MINIPMP
	kfree(codec->reg_cache);
#endif//CONFIG_HHTECH_MINIPMP
	return ret;
}

/* If the i2c layer weren't so broken, we could pass this kind of data
   around */
static struct snd_soc_device *wm8987_socdev;
#if 0
#ifdef	CONFIG_HHTECH_MINIPMP
void hhbf_audio_close(void)
{
    if (!wm8987_socdev) return;
    cancel_delayed_work(&wm8987_socdev->delayed_work);
    wm8987_reset(wm8987_socdev->codec);
}
EXPORT_SYMBOL(hhbf_audio_close);
#endif//CONFIG_HHTECH_MINIPMP
#endif
/*******************************************************************************************************/
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)

static int wm8987_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = wm8987_socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret;
	i2c_set_clientdata(i2c, codec);

	codec->control_data = i2c;

	ret = wm8987_init(socdev);
	if (ret < 0)
		pr_err("failed to initialise WM8971\n");

	return ret;
}

static int wm8987_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id wm8987_i2c_id[] = {
	{ "wm8987", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8987_i2c_id);

static struct i2c_driver wm8987_i2c_driver = {
	.driver = {
		.name = "WM8987 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe    = wm8987_i2c_probe,
	.remove   = wm8987_i2c_remove,
	.id_table = wm8987_i2c_id,
};

static int wm8987_add_i2c_device(struct platform_device *pdev,
				 const struct wm8987_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&wm8987_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}
#if 1
	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "wm8987", I2C_NAME_SIZE);

	adapter = i2c_get_adapter(setup->i2c_bus);
	if (!adapter) {
		dev_err(&pdev->dev, "can't get i2c adapter %d\n",
			setup->i2c_bus);
		goto err_driver;
	}
	client = i2c_new_device(adapter, &info);

	i2c_put_adapter(adapter);
	if (!client) {
		dev_err(&pdev->dev, "can't add i2c device at 0x%x\n",
			(unsigned int)info.addr);
		goto err_driver;
	}
#endif
	return 0;

err_driver:
	i2c_del_driver(&wm8987_i2c_driver);
	return -ENODEV;
}

#endif
/**********************************************************************************************************/


#if 0
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
/*
 * WM8731 2 wire address is determined by GPIO5
 * state during powerup.
 *    low  = 0x1a
 *    high = 0x1b
 */
//static unsigned short normal_i2c[] = { 0, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static struct i2c_driver wm8987_i2c_driver;
static struct i2c_client client_template;

static int wm8987_codec_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct snd_soc_device *socdev = wm8987_socdev;
	struct wm8987_setup_data *setup = socdev->codec_data;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct i2c_client *i2c;
	int ret;
	if (addr != setup->i2c_address)
		return -ENODEV;

	client_template.adapter = adap;
	client_template.addr = addr;

#ifndef	CONFIG_HHTECH_MINIPMP
	i2c = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (i2c == NULL) {
		kfree(codec);
		return -ENOMEM;
	}
	memcpy(i2c, &client_template, sizeof(struct i2c_client));
#else// mhfan
	i2c = &client_template;
#endif//CONFIG_HHTECH_MINIPMP
	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = i2c_attach_client(i2c);
	if (ret < 0) {
		err("failed to attach codec at addr %x\n", addr);
		goto err;
	}

	ret = wm8987_init(socdev);
	if (ret < 0) {
	err("failed to initialise WM8987\n");
		goto err;
	}
	printk("init WM8987 OK\n");
	return ret;

err:
	kfree(codec);
#ifndef	CONFIG_HHTECH_MINIPMP
	kfree(i2c);
#endif//CONFIG_HHTECH_MINIPMP
	return ret;
}

static int wm8987_i2c_detach(struct i2c_client *client)
{
#ifndef	CONFIG_HHTECH_MINIPMP
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	i2c_detach_client(client);
	kfree(codec->reg_cache);
	kfree(client);
#else// mhfan
	i2c_detach_client(client);
#endif//CONFIG_HHTECH_MINIPMP
	return 0;
}

static int wm8987_i2c_attach(struct i2c_adapter *adap)
{
	printk(KERN_INFO "i2c_attach !\n");
	return i2c_probe(adap, &addr_data, wm8987_codec_probe);
}

/* corgi i2c codec control layer */
static struct i2c_driver wm8987_i2c_driver = {
	.driver = {
		.name = "WM8987 I2C Codec",
		.owner = THIS_MODULE,
	},
	.id =             I2C_DRIVERID_WM8987,
	.attach_adapter = wm8987_i2c_attach,
	.detach_client =  wm8987_i2c_detach,
	.command =        NULL,
};

static struct i2c_client client_template = {
	.name =   "WM8987",
	.driver = &wm8987_i2c_driver,
};
#endif
#endif
static int wm8987_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct wm8987_setup_data *setup = socdev->codec_data;
	struct snd_soc_codec *codec;
	//struct wm8987_priv *wm8987;	// mhfan
	int ret = 0;
//	printk(KERN_INFO "i2c->address is %d\n",setup->i2c_address);
//	printk(KERN_INFO "i2c->bus is %d\n",setup->i2c_bus);
	info("Audio Codec Driver %s", WM8987_VERSION);
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

#ifndef	CONFIG_HHTECH_MINIPMP
	wm8987 = kzalloc(sizeof(struct wm8987_priv), GFP_KERNEL);
	if (wm8987 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	codec->private_data = wm8987;
#endif//CONFIG_HHTECH_MINIPMP
	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	wm8987_socdev = socdev;
//#if 	LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
//	INIT_WORK(&codec->delayed_work, wm8987_work, codec);
//#else//	XXX: mhfan
	INIT_DELAYED_WORK(&codec->delayed_work, wm8987_work);
//#endif// LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		//normal_i2c[0] = setup->i2c_address;
		codec->hw_write = (hw_write_t)i2c_master_send;
		ret = wm8987_add_i2c_device(pdev, setup);
#if 0

		ret = i2c_add_driver(&wm8987_i2c_driver);
		if (ret != 0)
			printk(KERN_ERR "can't add i2c driver");
#endif
	}
#else
		/* Add other interfaces here */
#endif

	return ret;
}

/*
 * This function forces any delayed work to be queued and run.
 */
static int run_delayed_work(struct delayed_work *dwork)
{
	int ret;

	/* cancel any work waiting to be queued. */
	ret = cancel_delayed_work(dwork);

	/* if there was any work waiting then we run it now and
	 * wait for it's completion */
	if (ret) {
		schedule_delayed_work(dwork, 0);
		flush_scheduled_work();
	}
	return ret;
}

/* power down chip */
static int wm8987_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (codec->control_data)
#if defined (CONFIG_CPU_IMAPX200)

		wm8987_set_bias_level(codec, SND_SOC_BIAS_OFF);
#else
		wm8987_dapm_event(codec, SNDRV_CTL_POWER_D3cold);
#endif
	run_delayed_work(&codec->delayed_work);
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	i2c_del_driver(&wm8987_i2c_driver);
#endif
#ifndef	CONFIG_HHTECH_MINIPMP
	kfree(codec->private_data);
#endif//CONFIG_HHTECH_MINIPMP
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8987 = {
	.probe = 	wm8987_probe,
	.remove = 	wm8987_remove,
	.suspend = 	wm8987_suspend,
	.resume =	wm8987_resume,
};

EXPORT_SYMBOL_GPL(soc_codec_dev_wm8987);

MODULE_DESCRIPTION("ASoC WM8987 driver");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("GPL");
