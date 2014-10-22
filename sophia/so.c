
/*
 * sophia database
 * sphia.org
 *
 * Copyright (c) Dmitry Simonenko
 * BSD License
*/

#include <libsr.h>
#include <libsv.h>
#include <libsm.h>
#include <libsl.h>
#include <libsd.h>
#include <libsi.h>
#include <libso.h>
#include <sophia.h>

static void*
so_ctl(soobj *obj, va_list args srunused)
{
	so *o = (so*)obj;
	return &o->ctl;
}

static int
so_open(soobj *o, va_list args)
{
	so *e = (so*)o;
	if (so_active(e))
		return -1;
	so_statusset(&e->status, SO_RECOVER);
	srlist *i, *n;
	sr_listforeach_safe(&e->db.list, i, n) {
		soobj *o = srcast(i, soobj, link);
		int rc = o->i->open(o, args);
		if (srunlikely(rc == -1))
			return -1;
	}
	so_statusset(&e->status, SO_ONLINE);
	return 0;
}

static int
so_destroy(soobj *o)
{
	so *e = (so*)o;
	int rcret = 0;
	int rc;
	so_statusset(&e->status, SO_SHUTDOWN);
	rc = so_objindex_destroy(&e->ctlcursor);
	if (srunlikely(rc == -1))
		rcret = -1;
	rc = so_objindex_destroy(&e->db);
	if (srunlikely(rc == -1))
		rcret = -1;
	sr_mutexfree(&e->apilock);
	sr_seqfree(&e->seq);
	sr_pagerfree(&e->pager);
	so_statusfree(&e->status);
	free(e);
	return rcret;
}

static void*
so_type(soobj *o srunused, va_list args srunused) {
	return "env";
}

static soobjif soif =
{
	.ctl      = so_ctl,
	.open     = so_open,
	.destroy  = so_destroy,
	.error    = NULL,
	.set      = NULL,
	.get      = NULL,
	.del      = NULL,
	.begin    = NULL,
	.prepare  = NULL,
	.commit   = NULL,
	.rollback = NULL,
	.cursor   = NULL,
	.object   = NULL,
	.type     = so_type
};

soobj *so_new(void)
{
	so *e = malloc(sizeof(*e));
	if (srunlikely(e == NULL))
		return NULL;
	memset(e, 0, sizeof(*e));
	so_objinit(&e->o, SOENV, &soif, &e->o /* self */);
	/* init allocation family */
	sr_pagerinit(&e->pager, 10, 1024);
	int rc = sr_pageradd(&e->pager);
	if (srunlikely(rc == -1)) {
		free(e);
		return NULL;
	}
	sr_allocopen(&e->a, &sr_astd);
	sr_allocopen(&e->a_db, &sr_aslab, &e->pager, sizeof(sodb));
	sr_allocopen(&e->a_v, &sr_aslab, &e->pager, sizeof(sov));
	sr_allocopen(&e->a_cursor, &sr_aslab, &e->pager, sizeof(socursor));
	sr_allocopen(&e->a_ctlcursor, &sr_aslab, &e->pager, sizeof(soctlcursor));
	sr_allocopen(&e->a_tx, &sr_aslab, &e->pager, sizeof(sotx));
	sr_allocopen(&e->a_smv, &sr_aslab, &e->pager, sizeof(smv));
	so_statusinit(&e->status);
	so_statusset(&e->status, SO_OFFLINE);
	so_ctlinit(&e->ctl, e);
	so_objindex_init(&e->db);
	so_objindex_init(&e->ctlcursor);
	sr_mutexinit(&e->apilock);
	sr_seqinit(&e->seq);
	sr_errorinit(&e->error);
	sr_init(&e->r, &e->error, &e->a, &e->seq, NULL, NULL);
	return &e->o;
}
