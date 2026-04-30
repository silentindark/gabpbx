/*
 * GABPBX -- Germán Aracil Boned PBX.
 *
 * Copyright (C) 2008 - present, Germán Luis Aracil Boned <garacilb@gmail.com>
 *
 * GABPBX was first created in 2008 by
 * Germán Luis Aracil Boned <garacilb@gmail.com>.
 *
 * GABPBX as a project is based on Asterisk 1.8 and was later updated
 * to the final stable Asterisk 1.8 release.
 *
 * Copyleft: GABPBX is free software, distributed under the terms of
 * the GNU General Public License Version 2.
 *
 * Existing copyright, authorship, Asterisk/Digium notices,
 * third-party notices, and GPL licensing terms are preserved when present.
 *
 * Copyright (C) Germán Aracil Boned
 * contact mail, garacilb@gmail.com
 *
 * app_presencecontrol.c for GABPBX 6.0.0
 *
*/

#ifdef HAVE_CONFIG_H
#include "confdefs.h"
#endif

#include "gabpbx.h"
#include "gabpbx/module.h"
#include "gabpbx/channel.h"
#include "gabpbx/pbx.h"
#include "gabpbx/lock.h"
#include "gabpbx/app.h"
#include "gabpbx/causes.h"
#include "gabpbx/file.h"
#include "gabpbx/cdr.h"
#include "gabpbx/say.h"
#include "gabpbx/cdr.h"
#include "gabpbx/options.h"

#include <math.h>


static char *app = "PresenceControl";

static int presencecontrol_exec(struct ast_channel *chan, const char *data)
{
	int res = 0;
	char accountcodeid[AST_MAX_ACCOUNT_CODE];
	char dni[32];
	char pin[32];
	char pswd[32];
	char staffid[32];
	char exitcode[32];
	char val[32];
	struct ast_variable *var, *varp = NULL;
	int usedniorpwd = -2;
	int cstatus = -2;

	ast_answer(chan);
	sleep(2);

	var = ast_load_realtime("client", "accountcode", chan->accountcode, SENTINEL);
	if (var == NULL)
	{
		ast_play_and_wait(chan, "service-not-implemented");
		return res;
	}

	varp = var;
	while (var)
	{
		if (!strcasecmp(var->name, "id"))
		{
			ast_copy_string(&accountcodeid[0], ast_strdupa(var->value), sizeof(accountcodeid));
			break;
		}
	}
	ast_variables_destroy(varp);
	var = NULL;

	do {
		do {
			ast_app_getdata(chan, "pr/introduzcacodigo", dni, 8, 2000);
		} while (ast_strlen_zero(dni) && (!ast_check_hangup(chan)));

		if (ast_strlen_zero(dni))
			return res;

		if (strlen(dni) > 4)	
			var = ast_load_realtime("prstaff", "accountcodeid", accountcodeid, "dni", dni, "active", "1", SENTINEL);
		else
			var = ast_load_realtime("prstaff", "accountcodeid", accountcodeid, "passwd", dni, "active", "1", SENTINEL);

		if (var == NULL)
		{
			ast_play_and_wait(chan, "pr/codigonocorrecto");
		}
	} while ((!var) && (!ast_check_hangup(chan)));

	varp = var;
	while (var) {
		if (!strcasecmp(var->name, "usedniorpwd"))
			usedniorpwd = atoi(ast_strdupa(var->value));
		else if (!strcasecmp(var->name, "cstatus"))
			cstatus = atoi(ast_strdupa(var->value));
		else if (!strcasecmp(var->name, "passwd"))
			ast_copy_string(&pswd[0], ast_strdupa(var->value), sizeof(pswd));
		else if (!strcasecmp(var->name, "id")) {
			ast_verbose(VERBOSE_PREFIX_3 "Staff id %s\n", ast_strdupa(var->value));
			ast_copy_string(&staffid[0], ast_strdupa(var->value), sizeof(staffid));
		}
		var = var->next;
	}
	ast_variables_destroy(varp);
	var = NULL;

	if ((usedniorpwd == 0) && (strlen(dni) < 5))
	{
		ast_play_and_wait(chan, "pr/codigonocorrecto");
		return res;
	} else if ((usedniorpwd == 2) && (strlen(dni) > 4))
	{
		sleep(1);
		do {
			ast_app_getdata(chan, "pr/introduzcacodigo", pin, 4, 2000);
		} while (ast_strlen_zero(pin) && (!ast_check_hangup(chan)));

		if ((strlen(pin) != 4) || (strcasecmp(pin, pswd)))
		{
			ast_play_and_wait(chan, "pr/codigonocorrecto");
			return res;
		}
	}

	sprintf(val, "%i", cstatus);
	ast_verbose(VERBOSE_PREFIX_3 "id %s before cstatus:%i (%s)\n", staffid, cstatus, val);

	if (cstatus == 0) {
		while (!ast_check_hangup(chan)) {
			// Exit code (custom 2..9 or not 1)
			ast_app_getdata(chan, "pr/motivosalida", exitcode, 1, 3000);
			ast_verbose(VERBOSE_PREFIX_3 "accountcodeid:%s exitcode: %s\n", accountcodeid, exitcode);
			if ((!ast_strlen_zero(exitcode)) && (atoi(exitcode) != 0))
			{
				var = ast_load_realtime("prexitcodes", "accountcodeid", accountcodeid, "code", exitcode, SENTINEL);
				if (var)
				{
					ast_variables_destroy(var);
					break;
				}
				ast_play_and_wait(chan, "pr/codigonocorrectosalida");
			}
		}
		if (strlen(exitcode) == 0) {
			ast_verbose(VERBOSE_PREFIX_3 "accountcodeid:%s Cancel\n", accountcodeid);
			return res;
		}
		ast_copy_string(&val[0], exitcode, sizeof(val));
		cstatus = atoi(exitcode);
		ast_play_and_wait(chan, "pr/unmomento");
	}
	else
	{
		ast_copy_string(&val[0], "0", sizeof(val));
		cstatus = 0;
		ast_play_and_wait(chan, "pr/unmomento");
	}

	ast_verbose(VERBOSE_PREFIX_3 "id %s after cstatus:%i\n", staffid, cstatus);

	if (!ast_strlen_zero(staffid))
		ast_update_realtime("prstaff", "id", staffid, "cstatus", &val[0], SENTINEL);

	if (cstatus == 0)
		ast_play_and_wait(chan, "pr/bienvenido");
	else
		ast_play_and_wait(chan, "pr/goodbye");

	return res;
}

static int unload_module(void)
{
        int res;

        res = ast_unregister_application(app);

        return res;
}

static int load_module(void)
{
	return ast_register_application_xml(app, presencecontrol_exec);
}

AST_MODULE_INFO(GABPBX_GPL_KEY, AST_MODFLAG_DEFAULT, "GAB Presence Control Application",
                .load = load_module,
                .unload = unload_module,
               );

