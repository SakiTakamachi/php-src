/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sterling Hughes <sterling@php.net>                           |
   +----------------------------------------------------------------------+
*/

#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#include "Zend/zend_smart_str.h"

#include "curl_private.h"

#include <curl/curl.h>
#include <curl/multi.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define SAVE_CURLM_ERROR(__handle, __err) (__handle)->err.no = (int) __err;

/* CurlMultiHandle class */

zend_class_entry *curl_multi_ce;

static inline php_curlm *curl_multi_from_obj(zend_object *obj) {
	return (php_curlm *)((char *)(obj) - XtOffsetOf(php_curlm, std));
}

#define Z_CURL_MULTI_P(zv) curl_multi_from_obj(Z_OBJ_P(zv))

/* {{{ Returns a new cURL multi handle */
PHP_FUNCTION(curl_multi_init)
{
	php_curlm *mh;
	CURLM *multi;

	ZEND_PARSE_PARAMETERS_NONE();
	multi = curl_multi_init();
	if (UNEXPECTED(multi == NULL)) {
		zend_throw_error(NULL, "%s(): Could not initialize a new cURL multi handle", get_active_function_name());
		RETURN_THROWS();
	}
	object_init_ex(return_value, curl_multi_ce);
	mh = Z_CURL_MULTI_P(return_value);
	mh->multi = multi;

	zend_llist_init(&mh->easyh, sizeof(zval), _php_curl_multi_cleanup_list, 0);
}
/* }}} */

/* {{{ Add a normal cURL handle to a cURL multi handle */
PHP_FUNCTION(curl_multi_add_handle)
{
	zval      *z_mh;
	zval      *z_ch;
	php_curlm *mh;
	php_curl  *ch;
	CURLMcode error = CURLM_OK;

	ZEND_PARSE_PARAMETERS_START(2,2)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
		Z_PARAM_OBJECT_OF_CLASS(z_ch, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);
	ch = Z_CURL_P(z_ch);

	_php_curl_verify_handlers(ch, /* reporterror */ true);

	_php_curl_cleanup_handle(ch);

	error = curl_multi_add_handle(mh->multi, ch->cp);
	SAVE_CURLM_ERROR(mh, error);

	if (error == CURLM_OK) {
		Z_ADDREF_P(z_ch);
		zend_llist_add_element(&mh->easyh, z_ch);
	}

	RETURN_LONG((zend_long) error);
}
/* }}} */

void _php_curl_multi_cleanup_list(void *data) /* {{{ */
{
	zval *z_ch = (zval *)data;

	zval_ptr_dtor(z_ch);
}
/* }}} */

/* Used internally as comparison routine passed to zend_list_del_element */
static int curl_compare_objects( zval *z1, zval *z2 ) /* {{{ */
{
	return (Z_TYPE_P(z1) == Z_TYPE_P(z2) &&
			Z_TYPE_P(z1) == IS_OBJECT &&
			Z_OBJ_P(z1) == Z_OBJ_P(z2));
}
/* }}} */

/* Used to find the php_curl resource for a given curl easy handle */
static zval *_php_curl_multi_find_easy_handle(php_curlm *mh, CURL *easy) /* {{{ */
{
	php_curl 			*tmp_ch;
	zend_llist_position pos;
	zval				*pz_ch_temp;

	for(pz_ch_temp = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch_temp;
		pz_ch_temp = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {
		tmp_ch = Z_CURL_P(pz_ch_temp);

		if (tmp_ch->cp == easy) {
			return pz_ch_temp;
		}
	}

	return NULL;
}
/* }}} */

/* {{{ Remove a multi handle from a set of cURL handles */
PHP_FUNCTION(curl_multi_remove_handle)
{
	zval      *z_mh;
	zval      *z_ch;
	php_curlm *mh;
	php_curl  *ch;
	CURLMcode error = CURLM_OK;

	ZEND_PARSE_PARAMETERS_START(2,2)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
		Z_PARAM_OBJECT_OF_CLASS(z_ch, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);
	ch = Z_CURL_P(z_ch);

	error = curl_multi_remove_handle(mh->multi, ch->cp);
	SAVE_CURLM_ERROR(mh, error);

	if (error == CURLM_OK) {
		zend_llist_del_element(&mh->easyh, z_ch, (int (*)(void *, void *))curl_compare_objects);
	}

	RETURN_LONG((zend_long) error);
}
/* }}} */

PHP_FUNCTION(curl_multi_get_handles)
{
	zval      *z_mh;
	php_curlm *mh;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	array_init_size(return_value, zend_llist_count(&mh->easyh));
	zend_hash_real_init_packed(Z_ARRVAL_P(return_value));
	zend_llist_position pos;
	zval	*pz_ch;

	for (pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
		pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {

		Z_TRY_ADDREF_P(pz_ch);
		add_next_index_zval(return_value, pz_ch);
	}
}

/* {{{ Get all the sockets associated with the cURL extension, which can then be "selected" */
PHP_FUNCTION(curl_multi_select)
{
	zval           *z_mh;
	php_curlm      *mh;
	double          timeout = 1.0;
	int             numfds = 0;
	CURLMcode error = CURLM_OK;

	ZEND_PARSE_PARAMETERS_START(1,2)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
		Z_PARAM_OPTIONAL
		Z_PARAM_DOUBLE(timeout)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	if (!(timeout >= 0.0 && timeout <= (INT_MAX / 1000.0))) {
		zend_argument_value_error(2, "must be between 0 and %f", INT_MAX / 1000.0);
		RETURN_THROWS();
	}

	error = curl_multi_wait(mh->multi, NULL, 0, (int) (timeout * 1000.0), &numfds);
	if (CURLM_OK != error) {
		SAVE_CURLM_ERROR(mh, error);
		RETURN_LONG(-1);
	}

	RETURN_LONG(numfds);
}
/* }}} */

/* {{{ Run the sub-connections of the current cURL handle */
PHP_FUNCTION(curl_multi_exec)
{
	zval      *z_mh;
	zval      *z_still_running;
	php_curlm *mh;
	int        still_running;
	CURLMcode error = CURLM_OK;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
		Z_PARAM_ZVAL(z_still_running)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	{
		zend_llist_position pos;
		php_curl *ch;
		zval	*pz_ch;

		for (pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
			pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {
			ch = Z_CURL_P(pz_ch);

			_php_curl_verify_handlers(ch, /* reporterror */ true);
		}
	}

	still_running = zval_get_long(z_still_running);
	error = curl_multi_perform(mh->multi, &still_running);
	ZEND_TRY_ASSIGN_REF_LONG(z_still_running, still_running);

	SAVE_CURLM_ERROR(mh, error);
	RETURN_LONG((zend_long) error);
}
/* }}} */

/* {{{ Return the content of a cURL handle if CURLOPT_RETURNTRANSFER is set */
PHP_FUNCTION(curl_multi_getcontent)
{
	zval     *z_ch;
	php_curl *ch;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(z_ch, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(z_ch);

	if (ch->handlers.write->method == PHP_CURL_RETURN) {
		if (!ch->handlers.write->buf.s) {
			RETURN_EMPTY_STRING();
		}
		smart_str_0(&ch->handlers.write->buf);
		RETURN_STR_COPY(ch->handlers.write->buf.s);
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ Get information about the current transfers */
PHP_FUNCTION(curl_multi_info_read)
{
	zval      *z_mh;
	php_curlm *mh;
	CURLMsg	  *tmp_msg;
	int        queued_msgs;
	zval      *zmsgs_in_queue = NULL;
	php_curl  *ch;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
		Z_PARAM_OPTIONAL
		Z_PARAM_ZVAL(zmsgs_in_queue)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	tmp_msg = curl_multi_info_read(mh->multi, &queued_msgs);
	if (tmp_msg == NULL) {
		RETURN_FALSE;
	}

	if (zmsgs_in_queue) {
		ZEND_TRY_ASSIGN_REF_LONG(zmsgs_in_queue, queued_msgs);
	}

	array_init(return_value);
	add_assoc_long(return_value, "msg", tmp_msg->msg);
	add_assoc_long(return_value, "result", tmp_msg->data.result);

	/* find the original easy curl handle */
	{
		zval *pz_ch = _php_curl_multi_find_easy_handle(mh, tmp_msg->easy_handle);
		if (pz_ch != NULL) {
			/* we must save result to be able to read error message */
			ch = Z_CURL_P(pz_ch);
			SAVE_CURL_ERROR(ch, tmp_msg->data.result);

			Z_ADDREF_P(pz_ch);
			add_assoc_zval(return_value, "handle", pz_ch);
		}
	}
}
/* }}} */

/* {{{ Close a set of cURL handles */
PHP_FUNCTION(curl_multi_close)
{
	php_curlm *mh;
	zval *z_mh;

	zend_llist_position pos;
	zval *pz_ch;

	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	for (pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
		pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {
		php_curl *ch = Z_CURL_P(pz_ch);
		_php_curl_verify_handlers(ch, /* reporterror */ true);
		curl_multi_remove_handle(mh->multi, ch->cp);
	}
	zend_llist_clean(&mh->easyh);
}
/* }}} */

/* {{{ Return an integer containing the last multi curl error number */
PHP_FUNCTION(curl_multi_errno)
{
	zval        *z_mh;
	php_curlm   *mh;

	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	RETURN_LONG(mh->err.no);
}
/* }}} */

/* {{{ return string describing error code */
PHP_FUNCTION(curl_multi_strerror)
{
	zend_long code;
	const char *str;

	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_LONG(code)
	ZEND_PARSE_PARAMETERS_END();

	str = curl_multi_strerror(code);
	if (str) {
		RETURN_STRING(str);
	} else {
		RETURN_NULL();
	}
}
/* }}} */


static int _php_server_push_callback(CURL *parent_ch, CURL *easy, size_t num_headers, struct curl_pushheaders *push_headers, void *userp) /* {{{ */
{
	php_curl 				*ch;
	php_curl 				*parent;
	php_curlm 				*mh 			= (php_curlm *)userp;
	int 					rval 			= CURL_PUSH_DENY;
	zval					*pz_parent_ch 	= NULL;
	zval 					pz_ch;
	zval 					headers;
	zval 					retval;

	pz_parent_ch = _php_curl_multi_find_easy_handle(mh, parent_ch);
	if (pz_parent_ch == NULL) {
		return rval;
	}

	parent = Z_CURL_P(pz_parent_ch);

	ch = init_curl_handle_into_zval(&pz_ch);
	ch->cp = easy;
	_php_setup_easy_copy_handlers(ch, parent);

	array_init_size(&headers, num_headers);
	zend_hash_real_init_packed(Z_ARRVAL(headers));
	for (size_t i = 0; i < num_headers; i++) {
		char *header = curl_pushheader_bynum(push_headers, i);
		add_index_string(&headers, i, header);
	}

	ZEND_ASSERT(pz_parent_ch);
	zval call_args[3] = {*pz_parent_ch, pz_ch, headers};

	zend_call_known_fcc(&mh->handlers.server_push, &retval, /* param_count */ 3, call_args, /* named_params */ NULL);
	zval_ptr_dtor_nogc(&headers);

	if (!Z_ISUNDEF(retval)) {
		if (CURL_PUSH_DENY != php_curl_get_long(&retval)) {
		    rval = CURL_PUSH_OK;
			zend_llist_add_element(&mh->easyh, &pz_ch);
		} else {
			/* libcurl will free this easy handle, avoid double free */
			ch->cp = NULL;
		}
	}

	return rval;
}
/* }}} */

static bool _php_curl_multi_setopt(php_curlm *mh, zend_long option, zval *zvalue, zval *return_value) /* {{{ */
{
	CURLMcode error = CURLM_OK;

	switch (option) {
		case CURLMOPT_PIPELINING:
		case CURLMOPT_MAXCONNECTS:
		case CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE:
		case CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE:
		case CURLMOPT_MAX_HOST_CONNECTIONS:
		case CURLMOPT_MAX_PIPELINE_LENGTH:
		case CURLMOPT_MAX_TOTAL_CONNECTIONS:
#if LIBCURL_VERSION_NUM >= 0x074300 /* Available since 7.67.0 */
		case CURLMOPT_MAX_CONCURRENT_STREAMS:
#endif
		{
			zend_long lval = zval_get_long(zvalue);

			if (option == CURLMOPT_PIPELINING && (lval & 1)) {
#if LIBCURL_VERSION_NUM >= 0x073e00 /* Available since 7.62.0 */
				php_error_docref(NULL, E_WARNING, "CURLPIPE_HTTP1 is no longer supported");
#else
				php_error_docref(NULL, E_DEPRECATED, "CURLPIPE_HTTP1 is deprecated");
#endif
			}
			error = curl_multi_setopt(mh->multi, option, lval);
			break;
		}
		case CURLMOPT_PUSHFUNCTION: {
			/* See php_curl_set_callable_handler */
			if (ZEND_FCC_INITIALIZED(mh->handlers.server_push)) {
				zend_fcc_dtor(&mh->handlers.server_push);
			}

			char *error_str = NULL;
			if (UNEXPECTED(!zend_is_callable_ex(zvalue, /* object */ NULL, /* check_flags */ 0, /* callable_name */ NULL, &mh->handlers.server_push, /* error */ &error_str))) {
				if (!EG(exception)) {
					zend_argument_type_error(2, "must be a valid callback for option CURLMOPT_PUSHFUNCTION, %s", error_str);
				}
				efree(error_str);
				return false;
			}
			zend_fcc_addref(&mh->handlers.server_push);

			error = curl_multi_setopt(mh->multi, CURLMOPT_PUSHFUNCTION, _php_server_push_callback);
			if (error != CURLM_OK) {
				return false;
			}
			error = curl_multi_setopt(mh->multi, CURLMOPT_PUSHDATA, mh);
			break;
		}
		default:
			zend_argument_value_error(2, "is not a valid cURL multi option");
			error = CURLM_UNKNOWN_OPTION;
			break;
	}

	SAVE_CURLM_ERROR(mh, error);

	return error == CURLM_OK;
}
/* }}} */

/* {{{ Set an option for the curl multi handle */
PHP_FUNCTION(curl_multi_setopt)
{
	zval       *z_mh, *zvalue;
	zend_long        options;
	php_curlm *mh;

	ZEND_PARSE_PARAMETERS_START(3,3)
		Z_PARAM_OBJECT_OF_CLASS(z_mh, curl_multi_ce)
		Z_PARAM_LONG(options)
		Z_PARAM_ZVAL(zvalue)
	ZEND_PARSE_PARAMETERS_END();

	mh = Z_CURL_MULTI_P(z_mh);

	RETURN_BOOL(_php_curl_multi_setopt(mh, options, zvalue, return_value));
}
/* }}} */

/* CurlMultiHandle class */

static zend_object *curl_multi_create_object(zend_class_entry *class_type) {
	php_curlm *intern = zend_object_alloc(sizeof(php_curlm), class_type);

	zend_object_std_init(&intern->std, class_type);
	object_properties_init(&intern->std, class_type);

	return &intern->std;
}

static zend_function *curl_multi_get_constructor(zend_object *object) {
	zend_throw_error(NULL, "Cannot directly construct CurlMultiHandle, use curl_multi_init() instead");
	return NULL;
}

static void curl_multi_free_obj(zend_object *object)
{
	php_curlm *mh = curl_multi_from_obj(object);

	zend_llist_position pos;
	php_curl *ch;
	zval	*pz_ch;

	if (!mh->multi) {
		/* Can happen if constructor throws. */
		zend_object_std_dtor(&mh->std);
		return;
	}

	for (pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
		pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {
		if (!(OBJ_FLAGS(Z_OBJ_P(pz_ch)) & IS_OBJ_FREE_CALLED)) {
			ch = Z_CURL_P(pz_ch);
			_php_curl_verify_handlers(ch, /* reporterror */ false);
		}
	}

	curl_multi_cleanup(mh->multi);
	zend_llist_clean(&mh->easyh);

	if (ZEND_FCC_INITIALIZED(mh->handlers.server_push)) {
		zend_fcc_dtor(&mh->handlers.server_push);
	}

	zend_object_std_dtor(&mh->std);
}

static HashTable *curl_multi_get_gc(zend_object *object, zval **table, int *n)
{
	php_curlm *curl_multi = curl_multi_from_obj(object);

	zend_get_gc_buffer *gc_buffer = zend_get_gc_buffer_create();

	if (ZEND_FCC_INITIALIZED(curl_multi->handlers.server_push)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl_multi->handlers.server_push);
	}

	zend_llist_position pos;
	for (zval *pz_ch = (zval *) zend_llist_get_first_ex(&curl_multi->easyh, &pos); pz_ch;
		pz_ch = (zval *) zend_llist_get_next_ex(&curl_multi->easyh, &pos)) {
		zend_get_gc_buffer_add_zval(gc_buffer, pz_ch);
	}

	zend_get_gc_buffer_use(gc_buffer, table, n);

	/* CurlMultiHandle can never have properties as it's final and has strict-properties on.
	 * Avoid building a hash table. */
	return NULL;
}

static zend_object_handlers curl_multi_handlers;

void curl_multi_register_handlers(void) {
	curl_multi_ce->create_object = curl_multi_create_object;
	curl_multi_ce->default_object_handlers = &curl_multi_handlers;

	memcpy(&curl_multi_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	curl_multi_handlers.offset = XtOffsetOf(php_curlm, std);
	curl_multi_handlers.free_obj = curl_multi_free_obj;
	curl_multi_handlers.get_gc = curl_multi_get_gc;
	curl_multi_handlers.get_constructor = curl_multi_get_constructor;
	curl_multi_handlers.clone_obj = NULL;
	curl_multi_handlers.cast_object = curl_cast_object;
	curl_multi_handlers.compare = zend_objects_not_comparable;
}
