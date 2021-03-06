/*
 *  Copyright (c) 2000-2001 Vojtech Pavlik
 *  Copyright (c) 2006-2007 Jiri Kosina
 *
 *  HID to Linux Input mapping
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>

#include <linux/hid.h>
#include <linux/hid-debug.h>
#include <linux/spinlock.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <plat/imapx.h>

static int usb_keyboard_disable = 0;
static int caps_locking = 0;
static spinlock_t *usb_keyboard_disable_spinlock = NULL;
static span2_key_mark = 0;
static spinlock_t *caps_spinlock = NULL;
//static int span_key_locking = 0;
#if defined(CONFIG_INPUT_KEYBOARD_SPANISH)
#if defined(CONFIG_KEYBOARD_MATRIX)
extern int span_key_locking;
#else
int span_key_locking = 0;
int span2_key_locking = 0;
EXPORT_SYMBOL(span_key_locking);
EXPORT_SYMBOL(span2_key_locking);
#endif
#endif
static int shift_pressing = 0;
static int leftshift_pressing = 0;
static int rightshift_pressing = 0;
static int leftalt_pressing = 0;
static int rightalt_pressing = 0;
static int span_key_mark = 0;
static spinlock_t *span_key_spinlock = NULL;
static spinlock_t *span2_key_spinlock = NULL;
static int shift_key_locking = 0;
static spinlock_t *shift_key_spinlock = NULL;
static int alt_key_locking = 0;
static spinlock_t *alt_key_spinlock = NULL;
static int flags_numlock = 1;

static int left_alt_locking = 0;
static int right_alt_locking = 0;

#define unk	KEY_UNKNOWN
/*
   static const unsigned char hid_keyboard[256] = {
   0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
   50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
   4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
   27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
   65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
   105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
   72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
   191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
   115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
   122,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
   unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
   unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
   unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
   unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
   29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
   150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
   };
   */
#if defined(CONFIG_INPUT_KEYBOARD_SPANISH)
static const unsigned char hid_keyboard[256] = {
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,158, 14, 15, 57, 40,184, 41,
	 78,185,185,186,187,191, 51, 52, 12, 58, 59, 60, 61, 62, 63, 64,
//	 65, 66, 67, 68, 87, 88, 99, 70,111,110,102,104,119,107,109,106,//spanish
	  65, 66, 67, 68, 87, 88, 99, 70,111,119,102,104,110,107,109,106,//brazil
	105,108,103, 69,188, 98, 55, 74, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,139,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
	183,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
	unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
	 29, 42, 56,102, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
};
#elif defined(CONFIG_INPUT_KEYBOARD_AMERICA)
static const unsigned char hid_keyboard[256] = {
        0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
        50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
        4,  5,  6,  7,  8,  9, 10, 11, 28,158, 14, 15, 57, 12, 13, 26,
        27, 43, 43, 39, 40, 41, 51, 52, 53, 58,59, 60, 61, 62, 63, 64,
        65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
        105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
        72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
        191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
        115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
        122,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        29, 42, 56,102, 97, 54,100,126,164,166,165,163,161,115,114,113,
        150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
};
#elif defined(CONFIG_INPUT_KEYBOARD_JAPANESE)
static const unsigned char hid_keyboard[256] = {
        0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
        50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
        4,  5,  6,  7,  8,  9, 10, 11, 28,158, 14, 15, 57, 12, 250, 248,
        26, 27, 27, 39, 249, 85, 51, 52, 53, 58,59, 60, 61, 62, 63, 64,
        65, 66, 87, 88, 183, 43, 69, 70,119,110,102,104,111,107,109,106,
        105,108,103, unk, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
        72, 73, 82, 83, 86,100,116,117,183,184,185,186,187,188,189,190,
        191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
        115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
        122,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        29, 42, 92,102, 97, 54,94,126,164,166,165,163,161,115,114,113,
        150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
};
#elif defined(CONFIG_INPUT_KEYBOARD_ITALY)
static const unsigned char hid_keyboard[256] = {
        0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
        50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
        4,  5,  6,  7,  8,  9, 10, 11, 28,158, 14, 15, 57, 40, 183, 184,
        78, 185, 185, 186, 187, 43, 51, 52, 12, 58,59, 60, 61, 62, 63, 64,
        65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
        105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
        72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
        191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
        115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
        122,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
        29, 42, 56,102, 97, 54,100,126,164,166,165,163,161,115,114,113,
        150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
};
#endif

static const struct {
	__s32 x;
	__s32 y;
}  hid_hat_to_axis[] = {{ 0, 0}, { 0,-1}, { 1,-1}, { 1, 0}, { 1, 1}, { 0, 1}, {-1, 1}, {-1, 0}, {-1,-1}};

#define map_abs(c)	hid_map_usage(hidinput, usage, &bit, &max, EV_ABS, (c))
#define map_rel(c)	hid_map_usage(hidinput, usage, &bit, &max, EV_REL, (c))
#define map_key(c)	hid_map_usage(hidinput, usage, &bit, &max, EV_KEY, (c))
#define map_led(c)	hid_map_usage(hidinput, usage, &bit, &max, EV_LED, (c))

#define map_abs_clear(c)	hid_map_usage_clear(hidinput, usage, &bit, \
		&max, EV_ABS, (c))
#define map_key_clear(c)	hid_map_usage_clear(hidinput, usage, &bit, \
		&max, EV_KEY, (c))

static inline int match_scancode(int code, int scancode)
{
	if (scancode == 0)
		return 1;
	return ((code & (HID_USAGE_PAGE | HID_USAGE)) == scancode);
}

static inline int match_keycode(int code, int keycode)
{
	if (keycode == 0)
		return 1;
	return (code == keycode);
}

static struct hid_usage *hidinput_find_key(struct hid_device *hid,
		int scancode, int keycode)
{
	int i, j, k;
	struct hid_report *report;
	struct hid_usage *usage;

	for (k = HID_INPUT_REPORT; k <= HID_OUTPUT_REPORT; k++) {
		list_for_each_entry(report, &hid->report_enum[k].report_list, list) {
			for (i = 0; i < report->maxfield; i++) {
				for ( j = 0; j < report->field[i]->maxusage; j++) {
					usage = report->field[i]->usage + j;
					if (usage->type == EV_KEY &&
						match_scancode(usage->hid, scancode) &&
						match_keycode(usage->code, keycode))
						return usage;
				}
			}
		}
	}
	return NULL;
}

static int hidinput_getkeycode(struct input_dev *dev, int scancode,
				int *keycode)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct hid_usage *usage;

	usage = hidinput_find_key(hid, scancode, 0);
	if (usage) {
		*keycode = usage->code;
		return 0;
	}
	return -EINVAL;
}

static int hidinput_setkeycode(struct input_dev *dev, int scancode,
				int keycode)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct hid_usage *usage;
	int old_keycode;

	if (keycode < 0 || keycode > KEY_MAX)
		return -EINVAL;

	usage = hidinput_find_key(hid, scancode, 0);
	if (usage) {
		old_keycode = usage->code;
		usage->code = keycode;

		clear_bit(old_keycode, dev->keybit);
		set_bit(usage->code, dev->keybit);
		dbg_hid(KERN_DEBUG "Assigned keycode %d to HID usage code %x\n", keycode, scancode);
		/* Set the keybit for the old keycode if the old keycode is used
		 * by another key */
		if (hidinput_find_key (hid, 0, old_keycode))
			set_bit(old_keycode, dev->keybit);

		return 0;
	}

	return -EINVAL;
}


static void hidinput_configure_usage(struct hid_input *hidinput, struct hid_field *field,
				     struct hid_usage *usage)
{
	struct input_dev *input = hidinput->input;
	struct hid_device *device = input_get_drvdata(input);
	int max = 0, code;
	unsigned long *bit = NULL;

	field->hidinput = hidinput;

	if (field->flags & HID_MAIN_ITEM_CONSTANT)
		goto ignore;

	/* only LED usages are supported in output fields */
	if (field->report_type == HID_OUTPUT_REPORT &&
			(usage->hid & HID_USAGE_PAGE) != HID_UP_LED) {
		goto ignore;
	}

	if (device->driver->input_mapping) {
		int ret = device->driver->input_mapping(device, hidinput, field,
				usage, &bit, &max);
		if (ret > 0)
			goto mapped;
		if (ret < 0)
			goto ignore;
	}

	switch (usage->hid & HID_USAGE_PAGE) {
	case HID_UP_UNDEFINED:
		goto ignore;

	case HID_UP_KEYBOARD:
		set_bit(EV_REP, input->evbit);

		if ((usage->hid & HID_USAGE) < 256) {
			if (!hid_keyboard[usage->hid & HID_USAGE]) goto ignore;
			map_key_clear(hid_keyboard[usage->hid & HID_USAGE]);
		} else
			map_key(KEY_UNKNOWN);

		break;

	case HID_UP_BUTTON:
		code = ((usage->hid - 1) & 0xf);

		switch (field->application) {
		case HID_GD_MOUSE:
		case HID_GD_POINTER:  code += 0x110; break;
		case HID_GD_JOYSTICK: code += 0x120; break;
		case HID_GD_GAMEPAD:  code += 0x130; break;
		default:
			switch (field->physical) {
			case HID_GD_MOUSE:
			case HID_GD_POINTER:  code += 0x110; break;
			case HID_GD_JOYSTICK: code += 0x120; break;
			case HID_GD_GAMEPAD:  code += 0x130; break;
			default:              code += 0x100;
			}
		}

		map_key(code);
		break;

	case HID_UP_SIMULATION:
		switch (usage->hid & 0xffff) {
		case 0xba: map_abs(ABS_RUDDER);   break;
		case 0xbb: map_abs(ABS_THROTTLE); break;
		case 0xc4: map_abs(ABS_GAS);      break;
		case 0xc5: map_abs(ABS_BRAKE);    break;
		case 0xc8: map_abs(ABS_WHEEL);    break;
		default:   goto ignore;
		}
		break;

	case HID_UP_GENDESK:
		if ((usage->hid & 0xf0) == 0x80) {	/* SystemControl */
			switch (usage->hid & 0xf) {
			case 0x1: map_key_clear(KEY_POWER);  break;
			case 0x2: map_key_clear(KEY_SLEEP);  break;
			case 0x3: map_key_clear(KEY_WAKEUP); break;
			default: goto unknown;
			}
			break;
		}

		if ((usage->hid & 0xf0) == 0x90) {	/* D-pad */
			switch (usage->hid) {
			case HID_GD_UP:	   usage->hat_dir = 1; break;
			case HID_GD_DOWN:  usage->hat_dir = 5; break;
			case HID_GD_RIGHT: usage->hat_dir = 3; break;
			case HID_GD_LEFT:  usage->hat_dir = 7; break;
			default: goto unknown;
			}
			if (field->dpad) {
				map_abs(field->dpad);
				goto ignore;
			}
			map_abs(ABS_HAT0X);
			break;
		}

		switch (usage->hid) {
		/* These usage IDs map directly to the usage codes. */
		case HID_GD_X: case HID_GD_Y: case HID_GD_Z:
		case HID_GD_RX: case HID_GD_RY: case HID_GD_RZ:
		case HID_GD_SLIDER: case HID_GD_DIAL: case HID_GD_WHEEL:
			if (field->flags & HID_MAIN_ITEM_RELATIVE)
				map_rel(usage->hid & 0xf);
			else
				map_abs(usage->hid & 0xf);
			break;

		case HID_GD_HATSWITCH:
			usage->hat_min = field->logical_minimum;
			usage->hat_max = field->logical_maximum;
			map_abs(ABS_HAT0X);
			break;

		case HID_GD_START:	map_key_clear(BTN_START);	break;
		case HID_GD_SELECT:	map_key_clear(BTN_SELECT);	break;

		default: goto unknown;
		}

		break;

	case HID_UP_LED:
		switch (usage->hid & 0xffff) {		      /* HID-Value:                   */
		case 0x01:  map_led (LED_NUML);     break;    /*   "Num Lock"                 */
		case 0x02:  map_led (LED_CAPSL);    break;    /*   "Caps Lock"                */
		case 0x03:  map_led (LED_SCROLLL);  break;    /*   "Scroll Lock"              */
		case 0x04:  map_led (LED_COMPOSE);  break;    /*   "Compose"                  */
		case 0x05:  map_led (LED_KANA);     break;    /*   "Kana"                     */
		case 0x27:  map_led (LED_SLEEP);    break;    /*   "Stand-By"                 */
		case 0x4c:  map_led (LED_SUSPEND);  break;    /*   "System Suspend"           */
		case 0x09:  map_led (LED_MUTE);     break;    /*   "Mute"                     */
		case 0x4b:  map_led (LED_MISC);     break;    /*   "Generic Indicator"        */
		case 0x19:  map_led (LED_MAIL);     break;    /*   "Message Waiting"          */
		case 0x4d:  map_led (LED_CHARGING); break;    /*   "External Power Connected" */

		default: goto ignore;
		}
		break;

	case HID_UP_DIGITIZER:
		switch (usage->hid & 0xff) {
		case 0x30: /* TipPressure */
			if (!test_bit(BTN_TOUCH, input->keybit)) {
				device->quirks |= HID_QUIRK_NOTOUCH;
				set_bit(EV_KEY, input->evbit);
				set_bit(BTN_TOUCH, input->keybit);
			}
			map_abs_clear(ABS_PRESSURE);
			break;

		case 0x32: /* InRange */
			switch (field->physical & 0xff) {
			case 0x21: map_key(BTN_TOOL_MOUSE); break;
			case 0x22: map_key(BTN_TOOL_FINGER); break;
			default: map_key(BTN_TOOL_PEN); break;
			}
			break;

		case 0x3c: /* Invert */
			map_key_clear(BTN_TOOL_RUBBER);
			break;

		case 0x33: /* Touch */
		case 0x42: /* TipSwitch */
		case 0x43: /* TipSwitch2 */
			device->quirks &= ~HID_QUIRK_NOTOUCH;
			map_key_clear(BTN_TOUCH);
			break;

		case 0x44: /* BarrelSwitch */
			map_key_clear(BTN_STYLUS);
			break;

		default:  goto unknown;
		}
		break;

	case HID_UP_CONSUMER:	/* USB HUT v1.1, pages 56-62 */
		switch (usage->hid & HID_USAGE) {
		case 0x000: goto ignore;
		case 0x034: map_key_clear(KEY_SLEEP);		break;
		case 0x036: map_key_clear(BTN_MISC);		break;

		case 0x040: map_key_clear(KEY_MENU);		break;
		case 0x045: map_key_clear(KEY_RADIO);		break;

		case 0x083: map_key_clear(KEY_LAST);		break;
		case 0x088: map_key_clear(KEY_PC);		break;
		case 0x089: map_key_clear(KEY_TV);		break;
		case 0x08a: map_key_clear(KEY_WWW);		break;
		case 0x08b: map_key_clear(KEY_DVD);		break;
		case 0x08c: map_key_clear(KEY_PHONE);		break;
		case 0x08d: map_key_clear(KEY_PROGRAM);		break;
		case 0x08e: map_key_clear(KEY_VIDEOPHONE);	break;
		case 0x08f: map_key_clear(KEY_GAMES);		break;
		case 0x090: map_key_clear(KEY_MEMO);		break;
		case 0x091: map_key_clear(KEY_CD);		break;
		case 0x092: map_key_clear(KEY_VCR);		break;
		case 0x093: map_key_clear(KEY_TUNER);		break;
		case 0x094: map_key_clear(KEY_EXIT);		break;
		case 0x095: map_key_clear(KEY_HELP);		break;
		case 0x096: map_key_clear(KEY_TAPE);		break;
		case 0x097: map_key_clear(KEY_TV2);		break;
		case 0x098: map_key_clear(KEY_SAT);		break;
		case 0x09a: map_key_clear(KEY_PVR);		break;

		case 0x09c: map_key_clear(KEY_CHANNELUP);	break;
		case 0x09d: map_key_clear(KEY_CHANNELDOWN);	break;
		case 0x0a0: map_key_clear(KEY_VCR2);		break;

		case 0x0b0: map_key_clear(KEY_PLAY);		break;
		case 0x0b1: map_key_clear(KEY_PAUSE);		break;
		case 0x0b2: map_key_clear(KEY_RECORD);		break;
		case 0x0b3: map_key_clear(KEY_FASTFORWARD);	break;
		case 0x0b4: map_key_clear(KEY_REWIND);		break;
		case 0x0b5: map_key_clear(KEY_NEXTSONG);	break;
		case 0x0b6: map_key_clear(KEY_PREVIOUSSONG);	break;
		case 0x0b7: map_key_clear(KEY_STOPCD);		break;
		case 0x0b8: map_key_clear(KEY_EJECTCD);		break;
		case 0x0bc: map_key_clear(KEY_MEDIA_REPEAT);	break;

		case 0x0cd: map_key_clear(KEY_PLAYPAUSE);	break;
		case 0x0e0: map_abs_clear(ABS_VOLUME);		break;
		case 0x0e2: map_key_clear(KEY_MUTE);		break;
		case 0x0e5: map_key_clear(KEY_BASSBOOST);	break;
		case 0x0e9: map_key_clear(KEY_VOLUMEUP);	break;
		case 0x0ea: map_key_clear(KEY_VOLUMEDOWN);	break;

		case 0x182: map_key_clear(KEY_BOOKMARKS);	break;
		case 0x183: map_key_clear(KEY_CONFIG);		break;
		case 0x184: map_key_clear(KEY_WORDPROCESSOR);	break;
		case 0x185: map_key_clear(KEY_EDITOR);		break;
		case 0x186: map_key_clear(KEY_SPREADSHEET);	break;
		case 0x187: map_key_clear(KEY_GRAPHICSEDITOR);	break;
		case 0x188: map_key_clear(KEY_PRESENTATION);	break;
		case 0x189: map_key_clear(KEY_DATABASE);	break;
		case 0x18a: map_key_clear(KEY_MAIL);		break;
		case 0x18b: map_key_clear(KEY_NEWS);		break;
		case 0x18c: map_key_clear(KEY_VOICEMAIL);	break;
		case 0x18d: map_key_clear(KEY_ADDRESSBOOK);	break;
		case 0x18e: map_key_clear(KEY_CALENDAR);	break;
		case 0x191: map_key_clear(KEY_FINANCE);		break;
		case 0x192: map_key_clear(KEY_CALC);		break;
		case 0x194: map_key_clear(KEY_FILE);		break;
		case 0x196: map_key_clear(KEY_WWW);		break;
		case 0x19c: map_key_clear(KEY_LOGOFF);		break;
		case 0x19e: map_key_clear(KEY_COFFEE);		break;
		case 0x1a6: map_key_clear(KEY_HELP);		break;
		case 0x1a7: map_key_clear(KEY_DOCUMENTS);	break;
		case 0x1ab: map_key_clear(KEY_SPELLCHECK);	break;
		case 0x1b6: map_key_clear(KEY_MEDIA);		break;
		case 0x1b7: map_key_clear(KEY_SOUND);		break;
		case 0x1bc: map_key_clear(KEY_MESSENGER);	break;
		case 0x1bd: map_key_clear(KEY_INFO);		break;
		case 0x201: map_key_clear(KEY_NEW);		break;
		case 0x202: map_key_clear(KEY_OPEN);		break;
		case 0x203: map_key_clear(KEY_CLOSE);		break;
		case 0x204: map_key_clear(KEY_EXIT);		break;
		case 0x207: map_key_clear(KEY_SAVE);		break;
		case 0x208: map_key_clear(KEY_PRINT);		break;
		case 0x209: map_key_clear(KEY_PROPS);		break;
		case 0x21a: map_key_clear(KEY_UNDO);		break;
		case 0x21b: map_key_clear(KEY_COPY);		break;
		case 0x21c: map_key_clear(KEY_CUT);		break;
		case 0x21d: map_key_clear(KEY_PASTE);		break;
		case 0x21f: map_key_clear(KEY_FIND);		break;
		case 0x221: map_key_clear(KEY_SEARCH);		break;
		case 0x222: map_key_clear(KEY_GOTO);		break;
		case 0x223: map_key_clear(KEY_HOMEPAGE);	break;
		case 0x224: map_key_clear(KEY_BACK);		break;
		case 0x225: map_key_clear(KEY_FORWARD);		break;
		case 0x226: map_key_clear(KEY_STOP);		break;
		case 0x227: map_key_clear(KEY_REFRESH);		break;
		case 0x22a: map_key_clear(KEY_BOOKMARKS);	break;
		case 0x22d: map_key_clear(KEY_ZOOMIN);		break;
		case 0x22e: map_key_clear(KEY_ZOOMOUT);		break;
		case 0x22f: map_key_clear(KEY_ZOOMRESET);	break;
		case 0x233: map_key_clear(KEY_SCROLLUP);	break;
		case 0x234: map_key_clear(KEY_SCROLLDOWN);	break;
		case 0x238: map_rel(REL_HWHEEL);		break;
		case 0x25f: map_key_clear(KEY_CANCEL);		break;
		case 0x279: map_key_clear(KEY_REDO);		break;

		case 0x289: map_key_clear(KEY_REPLY);		break;
		case 0x28b: map_key_clear(KEY_FORWARDMAIL);	break;
		case 0x28c: map_key_clear(KEY_SEND);		break;

		default:    goto ignore;
		}
		break;

	case HID_UP_HPVENDOR:	/* Reported on a Dutch layout HP5308 */
		set_bit(EV_REP, input->evbit);
		switch (usage->hid & HID_USAGE) {
		case 0x021: map_key_clear(KEY_PRINT);           break;
		case 0x070: map_key_clear(KEY_HP);		break;
		case 0x071: map_key_clear(KEY_CAMERA);		break;
		case 0x072: map_key_clear(KEY_SOUND);		break;
		case 0x073: map_key_clear(KEY_QUESTION);	break;
		case 0x080: map_key_clear(KEY_EMAIL);		break;
		case 0x081: map_key_clear(KEY_CHAT);		break;
		case 0x082: map_key_clear(KEY_SEARCH);		break;
		case 0x083: map_key_clear(KEY_CONNECT);	        break;
		case 0x084: map_key_clear(KEY_FINANCE);		break;
		case 0x085: map_key_clear(KEY_SPORT);		break;
		case 0x086: map_key_clear(KEY_SHOP);	        break;
		default:    goto ignore;
		}
		break;

	case HID_UP_MSVENDOR:
		goto ignore;

	case HID_UP_CUSTOM: /* Reported on Logitech and Apple USB keyboards */
		set_bit(EV_REP, input->evbit);
		goto ignore;

	case HID_UP_LOGIVENDOR:
		goto ignore;
	
	case HID_UP_PID:
		switch (usage->hid & HID_USAGE) {
		case 0xa4: map_key_clear(BTN_DEAD);	break;
		default: goto ignore;
		}
		break;

	default:
	unknown:
		if (field->report_size == 1) {
			if (field->report->type == HID_OUTPUT_REPORT) {
				map_led(LED_MISC);
				break;
			}
			map_key(BTN_MISC);
			break;
		}
		if (field->flags & HID_MAIN_ITEM_RELATIVE) {
			map_rel(REL_MISC);
			break;
		}
		map_abs(ABS_MISC);
		break;
	}

mapped:
	if (device->driver->input_mapped && device->driver->input_mapped(device,
				hidinput, field, usage, &bit, &max) < 0)
		goto ignore;

	set_bit(usage->type, input->evbit);

	while (usage->code <= max && test_and_set_bit(usage->code, bit))
		usage->code = find_next_zero_bit(bit, max + 1, usage->code);

	if (usage->code > max)
		goto ignore;


	if (usage->type == EV_ABS) {

		int a = field->logical_minimum;
		int b = field->logical_maximum;

		if ((device->quirks & HID_QUIRK_BADPAD) && (usage->code == ABS_X || usage->code == ABS_Y)) {
			a = field->logical_minimum = 0;
			b = field->logical_maximum = 255;
		}

		if (field->application == HID_GD_GAMEPAD || field->application == HID_GD_JOYSTICK)
			input_set_abs_params(input, usage->code, a, b, (b - a) >> 8, (b - a) >> 4);
		else	input_set_abs_params(input, usage->code, a, b, 0, 0);

	}

	if (usage->type == EV_ABS &&
	    (usage->hat_min < usage->hat_max || usage->hat_dir)) {
		int i;
		for (i = usage->code; i < usage->code + 2 && i <= max; i++) {
			input_set_abs_params(input, i, -1, 1, 0, 0);
			set_bit(i, input->absbit);
		}
		if (usage->hat_dir && !field->dpad)
			field->dpad = usage->code;
	}

	/* for those devices which produce Consumer volume usage as relative,
	 * we emulate pressing volumeup/volumedown appropriate number of times
	 * in hidinput_hid_event()
	 */
	if ((usage->type == EV_ABS) && (field->flags & HID_MAIN_ITEM_RELATIVE) &&
			(usage->code == ABS_VOLUME)) {
		set_bit(KEY_VOLUMEUP, input->keybit);
		set_bit(KEY_VOLUMEDOWN, input->keybit);
	}

	if (usage->type == EV_KEY) {
		set_bit(EV_MSC, input->evbit);
		set_bit(MSC_SCAN, input->mscbit);
	}

ignore:
	return;

}

void hidinput_hid_event(struct hid_device *hid, struct hid_field *field, struct hid_usage *usage, __s32 value)
{
	struct input_dev *input;
	unsigned *quirks = &hid->quirks;

	if (!field->hidinput)
		return;

	input = field->hidinput->input;

	if (!usage->type)
		return;

	if (usage->hat_min < usage->hat_max || usage->hat_dir) {
		int hat_dir = usage->hat_dir;
		if (!hat_dir)
			hat_dir = (value - usage->hat_min) * 8 / (usage->hat_max - usage->hat_min + 1) + 1;
		if (hat_dir < 0 || hat_dir > 8) hat_dir = 0;
		input_event(input, usage->type, usage->code    , hid_hat_to_axis[hat_dir].x);
                input_event(input, usage->type, usage->code + 1, hid_hat_to_axis[hat_dir].y);
                return;
        }

	if (usage->hid == (HID_UP_DIGITIZER | 0x003c)) { /* Invert */
		*quirks = value ? (*quirks | HID_QUIRK_INVERT) : (*quirks & ~HID_QUIRK_INVERT);
		return;
	}

	if (usage->hid == (HID_UP_DIGITIZER | 0x0032)) { /* InRange */
		if (value) {
			input_event(input, usage->type, (*quirks & HID_QUIRK_INVERT) ? BTN_TOOL_RUBBER : usage->code, 1);
			return;
		}
		input_event(input, usage->type, usage->code, 0);
		input_event(input, usage->type, BTN_TOOL_RUBBER, 0);
		return;
	}

	if (usage->hid == (HID_UP_DIGITIZER | 0x0030) && (*quirks & HID_QUIRK_NOTOUCH)) { /* Pressure */
		int a = field->logical_minimum;
		int b = field->logical_maximum;
		input_event(input, EV_KEY, BTN_TOUCH, value > a + ((b - a) >> 3));
	}

	if (usage->hid == (HID_UP_PID | 0x83UL)) { /* Simultaneous Effects Max */
		dbg_hid("Maximum Effects - %d\n",value);
		return;
	}

	if (usage->hid == (HID_UP_PID | 0x7fUL)) {
		dbg_hid("PID Pool Report\n");
		return;
	}

	if ((usage->type == EV_KEY) && (usage->code == 0)) /* Key 0 is "unassigned", not KEY_UNKNOWN */
		return;

	if ((usage->type == EV_ABS) && (field->flags & HID_MAIN_ITEM_RELATIVE) &&
			(usage->code == ABS_VOLUME)) {
		int count = abs(value);
		int direction = value > 0 ? KEY_VOLUMEUP : KEY_VOLUMEDOWN;
		int i;

		for (i = 0; i < count; i++) {
			input_event(input, EV_KEY, direction, 1);
			input_sync(input);
			input_event(input, EV_KEY, direction, 0);
			input_sync(input);
		}
		return;
	}

	/* report the usage code as scancode if the key status has changed */
	if (usage->type == EV_KEY && !!test_bit(usage->code, input->key) != value)
		input_event(input, EV_MSC, MSC_SCAN, usage->hid);
	//printk("............usage->code is %d,............value is %d ............\n",usage->code, value);


	/**
	 *  When Fn+Fin=107 the suspend-key are  pushed,the machine just
	 *  turns off backlight,again and backlight shall turn on
	 **/	
	if((usage->code == 107) && (value == 1)){
		unsigned int cfg;
		cfg = imapx_gpio_getcfg(__imapx_name_to_gpio(CONFIG_IG_LCD_BACKLIGHT), IG_NORMAL);
		if(cfg == IG_OUTPUT){
			/*turn on backlight*/	
			imapx_gpio_setcfg(__imapx_name_to_gpio(CONFIG_IG_LCD_BACKLIGHT), IG_CTRL0, IG_NORMAL);
		} else if (cfg == IG_CTRL0){
			/* turn off backlight*/
			imapx_gpio_setcfg(__imapx_name_to_gpio(CONFIG_IG_LCD_BACKLIGHT), IG_OUTPUT, IG_NORMAL); 
			imapx_gpio_setpin(__imapx_name_to_gpio(CONFIG_IG_LCD_BACKLIGHT), 0, IG_NORMAL); 
		}
		
		value = 0;//and here we disable suspend
	}

	/*disable num keyboard*/
/*	if ((usage->code == KEY_NUMLOCK) && value) {

		if (flags_numlock)
			flags_numlock = 0;
		else
			flags_numlock = 1;
	}
*/
	/* Support Caps lock. */
	if ((value == 1) && (usage->code == KEY_CAPSLOCK)) {
		spin_lock(caps_spinlock);
		if (caps_locking) {
			caps_locking = 0;
			/* printk(KERN_ALERT "Capslock is disabled.\n"); */
		} else {
			caps_locking = 1;
			/* printk(KERN_ALERT "Capslock is enabled.\n"); */
		}
		spin_unlock(caps_spinlock);
	}
	/* Support Spanish characters. */
#if 0
	if ((value == 1) && (usage->code == KEY_COMPOSE)) {
		spin_lock(span_key_spinlock);
		if (span_key_locking) {
			span_key_locking = 0;
			/* printk(KERN_ALERT "Spankey is disabled.\n"); */
		} else {
			span_key_locking = 1;
			/* printk(KERN_ALERT "Spankey is enabled.\n"); */
		}
		spin_unlock(span_key_spinlock);
	}
#else
	/* Process shift press, it will affects the spankey status. */
	/*
	if ((usage->code == KEY_LEFTSHIFT) || (usage->code == KEY_RIGHTSHIFT)) {
		if (value) {
			shift_pressing = 1;
			printk(KERN_ALERT "%d, code %d\n", __LINE__, usage->code);
		} else {
			shift_pressing = 0;
			printk(KERN_ALERT "%d, code %d\n", __LINE__, usage->code);
		}
	}
	*/
	if (usage->code == KEY_LEFTSHIFT) {
		if (value) {
			leftshift_pressing = 1;
		} else {
			leftshift_pressing = 0;
		}
	}
	if (usage->code == KEY_RIGHTSHIFT) {
		if (value) {
			rightshift_pressing = 1;
		} else {
			rightshift_pressing = 0;
		}
	}

	if (usage->code == KEY_LEFTALT) {
		if (value) {
			leftalt_pressing = 1;
		} else {
			leftalt_pressing = 0;
		}
	}
	if (usage->code == KEY_RIGHTALT) {
		if (value) {
			rightalt_pressing = 1;
		} else {
			rightalt_pressing = 0;

		}
	}

	if (usage->code == KEY_LEFTSHIFT ||usage->code == KEY_RIGHTSHIFT) {
		spin_lock(shift_key_spinlock);
		if (leftshift_pressing || rightshift_pressing) {
			shift_key_locking = 1;
		} else if (shift_key_locking) {
			shift_key_locking = 0;
		}
		spin_unlock(shift_key_spinlock);
	}
				
	if (usage->code == KEY_LEFTALT ||usage->code == KEY_RIGHTALT) {
		spin_lock(alt_key_spinlock);
		if (rightalt_pressing || leftalt_pressing) {
			alt_key_locking = 1;
		} else if (alt_key_locking) {
			alt_key_locking = 0;
		}
		spin_unlock(alt_key_spinlock);
	}
#ifndef CONFIG_KEYBOARD_FOR_TEST 

	/* KEY_APOSTROPHE is used to do spankey status record. */
/*	if (usage->code == KEY_F17) {
		if (value) {
			spin_lock(span2_key_spinlock);
			if (span2_key_locking) {
				span2_key_locking = 0;
				spin_unlock(span2_key_spinlock);

				input_event(input, usage->type, span2_key_mark, 1);
				input_event(input, usage->type, span2_key_mark, 0);
				// printk(KERN_ALERT "%d, mark %d\n", __LINE__, span_key_mark); 
			} else {
				if (leftalt_pressing || rightalt_pressing) {
					spin_unlock(span2_key_spinlock);
					input_event(input, usage->type, KEY_F17, 1);
					input_event(input, usage->type, KEY_F17, 0);
					return;
				}
				
				if (leftshift_pressing || rightshift_pressing) {
					span2_key_mark = KEY_F20;
					span2_key_locking = 2;
					//printk(KERN_ALERT "%d\n", __LINE__);
					spin_unlock(span2_key_spinlock);
					return;
				} else {
					span2_key_locking = 0;
					spin_unlock(span2_key_spinlock);
					input_event(input, usage->type, KEY_F17, 1);
					input_event(input, usage->type, KEY_F17, 0);
					return;
					//span2_key_mark = KEY_F17;
					//span2_key_locking = 1;
					//printk(KERN_ALERT "%d\n", __LINE__); 
				}
			}
		}
	}
*/

/*	if (usage->code == KEY_GRAVE) {
	        if (value) {
	                spin_lock(span2_key_spinlock);
			span_key_locking = 0;
	                if (span2_key_locking) {
		                span2_key_locking = 0;
	                        spin_unlock(span2_key_spinlock);
	                	input_event(input, usage->type, span2_key_mark, 1);
				input_event(input, usage->type, span2_key_mark, 0);

			} else {
	                        if (leftalt_pressing || rightalt_pressing) {
	                                spin_unlock(span2_key_spinlock);
	                                input_event(input, usage->type, KEY_GRAVE, 1);
					input_event(input, usage->type, KEY_GRAVE, 0);
					return;
	                        }

	                        if (leftshift_pressing || rightshift_pressing) {
	                                span2_key_mark = KEY_F20;//spanish kbd
	                                span2_key_locking = 2;
	                        //         printk(KERN_ALERT "%d\n", __LINE__); 
	                        } else {
	                                span2_key_mark = KEY_GRAVE;//spanish kbd
	                                span2_key_locking = 1;
	                         //        printk(KERN_ALERT "%d\n", __LINE__); 
	                        }
			        spin_unlock(span2_key_spinlock);
			}
	       }
	       
		return;
	}
*/



	/* *
	 * this is only for brazil keyboard because their accent-keys
	 * have different places in keyboard.
	 * added by zachary
	 * */
	if(usage->code == KEY_6){
		if(value){
			if(leftshift_pressing || rightshift_pressing){
				spin_lock(span_key_spinlock);
				if(span_key_locking){
					span_key_locking = 0;
					spin_unlock(span_key_spinlock);
					input_event(input, usage->type, KEY_6, 1);
					input_event(input, usage->type, KEY_6, 0);
				} else {
					span_key_mark = KEY_F19;
					span_key_locking = 2;//" .. "
					spin_unlock(span_key_spinlock);
				//	printk(KERN_ALERT "****SHIFT+6 & accent .. : triggered****");

				}
				return;
			}
		}
	}


	/* *
	 * this is also for brazil kbd which has the same reason as former
	 * 
	 * */
	if(usage->code == KEY_F17){
		if(value){
		        if(leftalt_pressing || rightalt_pressing){
				if(leftshift_pressing || rightshift_pressing){
					value = 0;//disable alt+shift+grave
   				}    
     				span2_key_locking = 0; 
			//	span_key_locking = 0; 
			} else {
				spin_lock(span2_key_spinlock);
				if(span2_key_locking){
					span2_key_locking = 0;
					spin_unlock(span2_key_spinlock);
					input_event(input, usage->type, KEY_F17, 1);
					input_event(input, usage->type, KEY_F17, 0);
				} else if(leftshift_pressing || rightshift_pressing){
					span2_key_mark = KEY_F20;
					span2_key_locking = 2;//"^"
					spin_unlock(span2_key_spinlock);
				//	printk(KERN_ALERT "***Shift+~ the accent mark ^ triggered***");
				} else {
					span2_key_mark = KEY_F17;
					span2_key_locking = 3;//"~"
					spin_unlock(span2_key_spinlock);
				//	printk(KERN_ALERT "***F17 the accent mark ~ triggered***");
				}
				return;
			}
		}
	}

	
	/* *
	 * same reason as former
	 * 
	 * */
	if(usage->code == KEY_GRAVE){
		if(value){
			if(leftalt_pressing || rightalt_pressing){
				if(leftshift_pressing || rightshift_pressing){
					value = 0;//disable alt+shift+grave
				}
				span2_key_locking = 0;
				span_key_locking = 0;
			} else {
				if(leftshift_pressing || rightshift_pressing){
					spin_lock(span2_key_spinlock);
					if(span2_key_locking){
						span2_key_locking = 0;
						spin_unlock(span2_key_spinlock);
						input_event(input, usage->type, KEY_GRAVE, 1);
						input_event(input, usage->type, KEY_GRAVE, 0);
					} else {
						span2_key_mark = KEY_GRAVE;
						span2_key_locking = 1;//"`"the 4th-tone
						spin_unlock(span2_key_spinlock);
				//		printk(KERN_ALERT "***Shift+grave the accent mark `  triggered***");
					}
				} else {
					spin_lock(span_key_spinlock);
					if(span_key_locking){
						span_key_locking = 0;
						spin_unlock(span_key_spinlock);
						input_event(input, usage->type, KEY_GRAVE, 1);
						input_event(input, usage->type, KEY_GRAVE, 0);
					} else {
						span_key_mark = KEY_GRAVE;
						span_key_locking = 1;//"'"the 2nd-tone
						spin_unlock(span_key_spinlock);
				//		printk(KERN_ALERT "***grave the accent mark ,2 triggered***");
					}
				}
				return;
			}
		}
	}

	/* This part of code is without lock protection but nothing matters. */
	if (value && (usage->code != KEY_F17)) {
		if (span_key_locking) {
			switch (usage->code) {
				case KEY_A:
				case KEY_E:
				case KEY_I:
				case KEY_O:
				case KEY_U:
				case KEY_LEFTSHIFT:
				case KEY_RIGHTSHIFT:
				case KEY_LEFTALT:
				case KEY_RIGHTALT:
				case KEY_CAPSLOCK:
				case KEY_ENTER:
				case KEY_BACKSPACE:
				case KEY_ESC:
				case KEY_LEFTCTRL:
				case KEY_RIGHTCTRL:
				case KEY_MENU:
				case KEY_HOME:
				case KEY_UP:
				case KEY_DOWN:
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_F1:
				case KEY_F2:
				case KEY_F3:
				case KEY_F4:
				case KEY_F5:
				case KEY_F6:
				case KEY_F7:
				case KEY_F8:
				case KEY_F9:
				case KEY_F10:
				case KEY_F11:
				case KEY_F12:
				case KEY_VOLUMEDOWN:
				case KEY_VOLUMEUP:
				case KEY_NUMLOCK:
				case KEY_DELETE:
				case KEY_END:
					/* TODO */
					break;

				case KEY_SPACE:
					input_event(input, usage->type, span_key_mark, 1);
					input_event(input, usage->type, span_key_mark, 0);
					return;

				default:
					input_event(input, usage->type, span_key_mark, 1);
					input_event(input, usage->type, span_key_mark, 0);
					/* printk(KERN_ALERT "%d, %d\n", __LINE__, span_key_mark); */
					break;
			}
		} else if (span2_key_locking) {
			switch (usage->code) {
				case KEY_A:
				case KEY_E:
				case KEY_I:
				case KEY_O:
				case KEY_U:
				case KEY_LEFTSHIFT:
				case KEY_RIGHTSHIFT:
				case KEY_LEFTALT:
				case KEY_RIGHTALT:
				case KEY_CAPSLOCK:
				case KEY_ENTER:
				case KEY_BACKSPACE:
				case KEY_ESC:
				case KEY_LEFTCTRL:
				case KEY_RIGHTCTRL:
				case KEY_MENU:
				case KEY_HOME:
				case KEY_UP:
				case KEY_DOWN:
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_F1:
				case KEY_F2:
				case KEY_F3:
				case KEY_F4:
				case KEY_F5:
				case KEY_F6:
				case KEY_F7:
				case KEY_F8:
				case KEY_F9:
				case KEY_F10:
				case KEY_F11:
				case KEY_F12:
				case KEY_VOLUMEDOWN:
				case KEY_VOLUMEUP:
				case KEY_NUMLOCK:
				case KEY_DELETE:
				case KEY_END:
			        /* TODO */
				        break;

				case KEY_SPACE:
					input_event(input, usage->type, span2_key_mark, 1);
					input_event(input, usage->type, span2_key_mark, 0);
					return;

				default:
					input_event(input, usage->type, span2_key_mark, 1);
					input_event(input, usage->type, span2_key_mark, 0);

					/* printk(KERN_ALERT "%d, %d\n", __LINE__, span_key_mark); */
					break;
			}
		}

	}
	
#endif
#endif
	if (usage->code == KEY_LEFTALT){
	        if (value == 1 ) {
			left_alt_locking++;
		} else {
			left_alt_locking=0;	
		}
	}
	if (usage->code == KEY_RIGHTALT){
		if (value == 1 ) 
			right_alt_locking++;
		else
			right_alt_locking=0;
	}
//	if (flags_numlock && (usage->code >= KEY_KP7) && (usage->code <= KEY_KPDOT))
//	    ;
	else if((left_alt_locking!=0||right_alt_locking!=0)&&(usage->code == KEY_LEFT||usage->code == KEY_RIGHT))
		//printk("....................alt_locking is ok\n");
		;
#ifndef CONFIG_KEYBOARD_FOR_TEST
	//else if (usage->code == KEY_LEFTCTRL && value == 1)
		//;
#endif
	else if ((left_alt_locking!=0||right_alt_locking!=0)&&usage->code == KEY_F1&&value) {
		input_event(input, usage->type, KEY_LEFTALT,0);
		input_event(input, usage->type, usage->code, value);
	} else
		input_event(input, usage->type, usage->code, value);	
	if ((field->flags & HID_MAIN_ITEM_RELATIVE) && (usage->type == EV_KEY))
		input_event(input, usage->type, usage->code, 0);
}

void hidinput_report_event(struct hid_device *hid, struct hid_report *report)
{
	struct hid_input *hidinput;

	list_for_each_entry(hidinput, &hid->inputs, list)
		input_sync(hidinput->input);
}
EXPORT_SYMBOL_GPL(hidinput_report_event);

int hidinput_find_field(struct hid_device *hid, unsigned int type, unsigned int code, struct hid_field **field)
{
	struct hid_report *report;
	int i, j;

	list_for_each_entry(report, &hid->report_enum[HID_OUTPUT_REPORT].report_list, list) {
		for (i = 0; i < report->maxfield; i++) {
			*field = report->field[i];
			for (j = 0; j < (*field)->maxusage; j++)
				if ((*field)->usage[j].type == type && (*field)->usage[j].code == code)
					return j;
		}
	}
	return -1;
}
EXPORT_SYMBOL_GPL(hidinput_find_field);

static int hidinput_open(struct input_dev *dev)
{
	struct hid_device *hid = input_get_drvdata(dev);

	return hid->ll_driver->open(hid);
}

static void hidinput_close(struct input_dev *dev)
{
	struct hid_device *hid = input_get_drvdata(dev);

	hid->ll_driver->close(hid);
}

/*
 * Register the input device; print a message.
 * Configure the input layer interface
 * Read all reports and initialize the absolute field values.
 */

int hidinput_connect(struct hid_device *hid, unsigned int force)
{
	struct hid_report *report;
	struct hid_input *hidinput = NULL;
	struct input_dev *input_dev;
	int i, j, k;
	int max_report_type = HID_OUTPUT_REPORT;

	INIT_LIST_HEAD(&hid->inputs);

	/* Added by Sololz. */
	/* FIXME: These spinlocks are create at a hid device connected to system board, and never
	 * released. Because I suppose the spinlocks are always needed. */
	if (caps_spinlock == NULL) {
		caps_spinlock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (caps_spinlock == NULL) {
			printk(KERN_ERR "kmalloc() for capslock key spinlock error.\n");
			return -1;
		}
		spin_lock_init(caps_spinlock);
	}
#if defined(CONFIG_INPUT_KEYBOARD_SPANISH)
	if (span_key_spinlock == NULL) {
		span_key_spinlock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (span_key_spinlock == NULL) {
			printk(KERN_ERR "kmalloc() for span key spinlock error.\n");
			return -1;
		}
		spin_lock_init(span_key_spinlock);
	}

	if (span2_key_spinlock == NULL) {
	        span2_key_spinlock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	        if (span2_key_spinlock == NULL) {
	                printk(KERN_ERR "kmalloc() for span2 key spinlock error.\n");
	                return -1;
	        }
	        spin_lock_init(span2_key_spinlock);
	}

#endif
	if (shift_key_spinlock ==NULL) {
		shift_key_spinlock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (shift_key_spinlock == NULL) {
			printk(KERN_ERR "kmalloc() for shift key spinlock error.\n");
			return -1;
		}
		spin_lock_init(shift_key_spinlock);
	}
	if (alt_key_spinlock ==NULL) {
		alt_key_spinlock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (alt_key_spinlock == NULL) {
			printk(KERN_ERR "kmalloc() for alt key spinlock error.\n");
			return -1;
		}
		spin_lock_init(alt_key_spinlock);
	}
	if (usb_keyboard_disable_spinlock ==NULL) {
	        usb_keyboard_disable_spinlock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	        if (usb_keyboard_disable_spinlock == NULL) {
	                printk(KERN_ERR "kmalloc() for usb_keyboard_disable_spinlock error.\n");
	                return -1;
	        }
	        spin_lock_init(usb_keyboard_disable_spinlock);
	}


	if (!force) {
		for (i = 0; i < hid->maxcollection; i++) {
			struct hid_collection *col = &hid->collection[i];
			if (col->type == HID_COLLECTION_APPLICATION ||
					col->type == HID_COLLECTION_PHYSICAL)
				if (IS_INPUT_APPLICATION(col->usage))
					break;
		}

		if (i == hid->maxcollection)
			return -1;
	}

	if (hid->quirks & HID_QUIRK_SKIP_OUTPUT_REPORTS)
		max_report_type = HID_INPUT_REPORT;

	for (k = HID_INPUT_REPORT; k <= max_report_type; k++)
		list_for_each_entry(report, &hid->report_enum[k].report_list, list) {

			if (!report->maxfield)
				continue;

			if (!hidinput) {
				hidinput = kzalloc(sizeof(*hidinput), GFP_KERNEL);
				input_dev = input_allocate_device();
				if (!hidinput || !input_dev) {
					kfree(hidinput);
					input_free_device(input_dev);
					err_hid("Out of memory during hid input probe");
					goto out_unwind;
				}

				input_set_drvdata(input_dev, hid);
				input_dev->event =
					hid->ll_driver->hidinput_input_event;
				input_dev->open = hidinput_open;
				input_dev->close = hidinput_close;
				input_dev->setkeycode = hidinput_setkeycode;
				input_dev->getkeycode = hidinput_getkeycode;

				input_dev->name = hid->name;
				input_dev->phys = hid->phys;
				input_dev->uniq = hid->uniq;
				input_dev->id.bustype = hid->bus;
				input_dev->id.vendor  = hid->vendor;
				input_dev->id.product = hid->product;
				input_dev->id.version = hid->version;
				input_dev->dev.parent = hid->dev.parent;
				hidinput->input = input_dev;
				list_add_tail(&hidinput->list, &hid->inputs);
			}

			for (i = 0; i < report->maxfield; i++)
				for (j = 0; j < report->field[i]->maxusage; j++)
					hidinput_configure_usage(hidinput, report->field[i],
								 report->field[i]->usage + j);

			if (hid->quirks & HID_QUIRK_MULTI_INPUT) {
				/* This will leave hidinput NULL, so that it
				 * allocates another one if we have more inputs on
				 * the same interface. Some devices (e.g. Happ's
				 * UGCI) cram a lot of unrelated inputs into the
				 * same interface. */
				hidinput->report = report;
				if (input_register_device(hidinput->input))
					goto out_cleanup;
				hidinput = NULL;
			}
		}

	if (hidinput && input_register_device(hidinput->input))
		goto out_cleanup;

	return 0;

out_cleanup:
	input_free_device(hidinput->input);
	kfree(hidinput);
out_unwind:
	/* unwind the ones we already registered */
	hidinput_disconnect(hid);

	return -1;
}
EXPORT_SYMBOL_GPL(hidinput_connect);

void hidinput_disconnect(struct hid_device *hid)
{
	struct hid_input *hidinput, *next;

	list_for_each_entry_safe(hidinput, next, &hid->inputs, list) {
		list_del(&hidinput->list);
		input_unregister_device(hidinput->input);
		kfree(hidinput);
	}
}
EXPORT_SYMBOL_GPL(hidinput_disconnect);

int usbinput_get_caps_locking_status(void)
{
	int status = 0;

	spin_lock(caps_spinlock);
	status = caps_locking;
	spin_unlock(caps_spinlock);

	return status;
}
EXPORT_SYMBOL(usbinput_get_caps_locking_status);
#if defined(CONFIG_INPUT_KEYBOARD_SPANISH)
int usbinput_get_span_key_locking_status(void)
{
	int status = 0;

	spin_lock(span_key_spinlock);
	status = span_key_locking;
	spin_unlock(span_key_spinlock);

	return status;
}
EXPORT_SYMBOL(usbinput_get_span_key_locking_status);
void usbinput_clear_span_key_locking_status(void)
{
	spin_lock(span_key_spinlock);
	span_key_locking = 0;
	spin_unlock(span_key_spinlock);
}
EXPORT_SYMBOL(usbinput_clear_span_key_locking_status);

int usbinput_get_span2_key_locking_status(void)
{
        int status = 0;

        spin_lock(span2_key_spinlock);
        status = span2_key_locking;
        spin_unlock(span2_key_spinlock);

        return status;
}
EXPORT_SYMBOL(usbinput_get_span2_key_locking_status);
void usbinput_clear_span2_key_locking_status(void)
{
        spin_lock(span2_key_spinlock);
        span2_key_locking = 0;
        spin_unlock(span2_key_spinlock);
}
EXPORT_SYMBOL(usbinput_clear_span2_key_locking_status);

#endif

int usbinput_get_key_shift_locking_status(void)
{
	int status = 0;
	spin_lock(shift_key_spinlock);
	status = shift_key_locking;
	spin_unlock(shift_key_spinlock);
		
	return status;
}
EXPORT_SYMBOL(usbinput_get_key_shift_locking_status);
int usbinput_get_key_alt_locking_status(void)
{
	int status = 0;
	spin_lock(alt_key_spinlock);
	status = alt_key_locking;
	spin_unlock(alt_key_spinlock);
		
	return status;
}
EXPORT_SYMBOL(usbinput_get_key_alt_locking_status);

