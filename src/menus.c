// automatically generated - do not edit

#include "conf.h"
#include "menu.h"
#include "cli.h"
#include "defproto.h"

const struct Menu guitop;
const struct Menu menu_aaaa;
const struct Menu menu_aaab;
const struct Menu menu_aaac;
const struct Menu menu_aaad;
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
    "Settings", &guitop, 0, {
	{ "save", MTYP_FUNC, (void*)ui_f_save, sizeof(argv_empty)/4, argv_empty },
	{ "volume", MTYP_FUNC, (void*)ui_f_volume, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu menu_aaad = {
    "Diag", &guitop, 0, {
	{ "imu", MTYP_FUNC, (void*)ui_f_imutest, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
const struct Menu guitop = {
    "Main Menu", &guitop, 0, {
	{ "charger", MTYP_MENU, (void*)&menu_aaaa },
	{ "clock", MTYP_FUNC, (void*)ui_f_clock, sizeof(argv_empty)/4, argv_empty },
	{ "settings", MTYP_MENU, (void*)&menu_aaac },
	{ "diag", MTYP_MENU, (void*)&menu_aaad },
	{ "power off", MTYP_FUNC, (void*)ui_f_shutdown, sizeof(argv_empty)/4, argv_empty },
	{}
    }
};
