// automatically generated - do not edit

#include "conf.h"
#include "menu.h"
#include "cli.h"
#include "defproto.h"

const struct Menu guitop;
const struct Menu menu_aaaa;
const struct Menu menu_aaab;
const struct Menu menu_aaac;
const struct Menu menu_aaaf;
const struct Menu menu_aaag;
const struct Menu guilogon;
const struct Menu guilogoff;
const char * arg_aaad[] = { "-ui", "3" };
const char * arg_aaae[] = { "-ui", "0" };
const char * argv_empty[] = { "-ui" };

const struct Menu menu_aaab = {
    "Settings", &menu_aaaa, 0, {
	{ "I_charge", MTYP_FUNC, (void*)ui_f_charge_current, sizeof(argv_empty)/4, argv_empty },
	{ "I_input", MTYP_FUNC, (void*)ui_f_input_current, sizeof(argv_empty)/4, argv_empty },
	{ "I_trickle", MTYP_FUNC, (void*)ui_f_trickle_current, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu menu_aaaa = {
    "Charger", &guitop, 0, {
	{ "status", MTYP_FUNC, (void*)ui_f_charge_status, sizeof(argv_empty)/4, argv_empty },
	{ "reset", MTYP_FUNC, (void*)ui_f_charge_reset, sizeof(argv_empty)/4, argv_empty },
	{ "settings", MTYP_MENU, (void*)&menu_aaab },
	{}
    }
};
const struct Menu menu_aaac = {
    "Candle", &guitop, 0, {
	{ "light", MTYP_FUNC, (void*)ui_f_set_blinky, sizeof(arg_aaad)/4, arg_aaad },
	{ "blow", MTYP_FUNC, (void*)ui_f_set_blinky, sizeof(arg_aaae)/4, arg_aaae },
	{}
    }
};
const struct Menu menu_aaaf = {
    "Settings", &guitop, 0, {
	{ "save", MTYP_FUNC, (void*)ui_f_save, sizeof(argv_empty)/4, argv_empty },
	{ "volume", MTYP_FUNC, (void*)ui_f_volume, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu menu_aaag = {
    "Diag", &guitop, 0, {
	{ "imu", MTYP_FUNC, (void*)ui_f_imutest, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu guitop = {
    "Main Menu", &guitop, 0, {
	{ "charger", MTYP_MENU, (void*)&menu_aaaa },
	{ "logger", MTYP_FUNC, (void*)ui_f_logger_menu, sizeof(argv_empty)/4, argv_empty },
	{ "clock", MTYP_FUNC, (void*)ui_f_clock, sizeof(argv_empty)/4, argv_empty },
	{ "candle", MTYP_MENU, (void*)&menu_aaac },
	{ "settings", MTYP_MENU, (void*)&menu_aaaf },
	{ "diag", MTYP_MENU, (void*)&menu_aaag },
	{ "power off", MTYP_FUNC, (void*)ui_f_shutdown, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu guilogon = {
    "Logger", &guilogon, 0, {
	{ "stop", MTYP_FUNC, (void*)ui_f_logger_stop, sizeof(argv_empty)/4, argv_empty },
	{ "reload", MTYP_FUNC, (void*)ui_f_logger_reload, sizeof(argv_empty)/4, argv_empty },
	{ "rotate", MTYP_FUNC, (void*)ui_f_logger_rotate, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu guilogoff = {
    "Logger", &guilogoff, 0, {
	{ "start", MTYP_FUNC, (void*)ui_f_logger_start, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
