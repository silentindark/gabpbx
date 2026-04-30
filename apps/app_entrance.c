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
 * contact mail, german@tecnoxarxa.com
 *
 * app_bridge.c for GABPBX 1.4
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

GABPBX_FILE_VERSION(__FILE__, "$Revision: 1000 $")

static char *app = "Entrance";

struct ast_app *sipaddheader_app = NULL;
struct ast_app *sipremoveheader_app = NULL;

static int SIPAddHeader(struct ast_channel *chan, void *data)
{
        int ret;

        if (sipaddheader_app) {
                ret = pbx_exec(chan, sipaddheader_app, data);
        } else {
                ast_log(LOG_WARNING, "Could not find application SIPAddHeader\n");
                ret = -2;
        }

        return ret;
}

#if 0
static int SIPRemoveHeader(struct ast_channel *chan, void *data)
{
        int ret;

        if (sipremoveheader_app) {
                ret = pbx_exec(chan, sipremoveheader_app, data);
        } else {
                ast_log(LOG_WARNING, "Could not find application SIPRemoveHeader\n");
                ret = -2;
        }

        return ret;
}
#endif

#if 0
static int dial(struct ast_channel *chan, void *data)
{
        struct ast_app *app = NULL;
        int ret;

        app = pbx_findapp("Dial");
        if (app) {
                ret = pbx_exec(chan, app, data);
        } else {
                ast_log(LOG_ERROR, "Could not find application (Dial)\n");
                ret = -2;
        }

        return ret;
}
#endif

static int entrance_exec(struct ast_channel *chan, const char *data)
{
	int res = 0;
	struct ast_variable *var = NULL, *tmpvar = NULL;
	char *tmp = NULL;
	char src[80] = "\0";
	char *accountcode = NULL;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(clibilling);
	);

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (chan && chan->cdr) {
		chan->cdr->callfrom = 1;
	}

	// Remove +34 from number A
	if (strcasestr(chan->caller.id.number.str, "+34")) {
		ast_copy_string(src, &chan->caller.id.number.str[3], strlen(chan->caller.id.number.str) - 2);
		ast_verbose(VERBOSE_PREFIX_3 "[%s|%s] Caller Num. %s@%s to %s@%s\n", chan->cdr->clisrc, chan->cdr->clidst, \
		chan->caller.id.number.str, chan->context, src, chan->context);
		ast_set_callerid(chan, (const char*) &src, (const char*) &src, (const char*) &src);
	}

	if (args.clibilling) {
		ast_channel_lock(chan);
		pbx_builtin_setvar_helper(chan, "CLIBILLING", args.clibilling);
		pbx_builtin_setvar_helper(chan, "__CLIBILLING", args.clibilling);

		char tucall_CliBilling[AST_MAX_EXTENSION*2] = "\0";

		sprintf(tucall_CliBilling, "TucallCliBilling: %s", args.clibilling);
		SIPAddHeader(chan, tucall_CliBilling);
		var = ast_load_realtime("clis", "cli", args.clibilling, SENTINEL);
		tmpvar = var;
		while (var) {
			if (!strcasecmp(var->name, "accountcode")) {
				accountcode = ast_strdupa(var->value);
				break;
			}
			var = var->next;		
		}
		if (accountcode)
			ast_string_field_set(chan, accountcode, accountcode);
		ast_variables_destroy(tmpvar);
		ast_channel_unlock(chan);
	}

	pbx_builtin_setvar_helper(chan, "TUCALL_ENTRANCE", "1");

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
	sipaddheader_app = pbx_findapp("SipAddHeader");
	sipremoveheader_app = pbx_findapp("SipRemoveHeader");

	return ast_register_application_xml(app, entrance_exec);
}

AST_MODULE_INFO(GABPBX_GPL_KEY, AST_MODFLAG_DEFAULT, "GAB Entrance Application",
                .load = load_module,
                .unload = unload_module,
               );

