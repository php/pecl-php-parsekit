/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Sara Golemon <pollita@php.net>                               |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_parsekit.h"

#ifndef Z_REFCOUNT_P
#define Z_REFCOUNT_P(pz)              (pz)->refcount
#endif

#ifndef Z_SET_REFCOUNT_P
#define Z_SET_REFCOUNT_P(pz, rc)      (pz)->refcount = rc
#endif

ZEND_DECLARE_MODULE_GLOBALS(parsekit)
/* Potentially thread-unsafe, see MINIT_FUNCTION */
void (*php_parsekit_original_error_function)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

/* Parsekit Workhorse */

/* {{{ php_parsekit_define_name */
static char* php_parsekit_define_name_ex(long val, php_parsekit_define_list *lookup_list, long *pflags, char *unknown_default)
{
	php_parsekit_define_list *names;

	for(names = lookup_list; names->str; names++) {
		if (names->val == val) {
			if (pflags) {
				*pflags = names->flags;
			}
			return names->str;
		}
	}

	return unknown_default ? unknown_default : PHP_PARSEKIT_UNKNOWN;
}

#define php_parsekit_define_name(val, lookup_list, unknown_default)	\
		php_parsekit_define_name_ex((val), (lookup_list), NULL, (unknown_default))
/* }}} */

/* {{{ php_parsekit_parse_node */
static void php_parsekit_parse_node(zval *return_value, zend_op_array *op_array, znode *node, long flags, long options TSRMLS_DC)
{
	array_init(return_value);
	add_assoc_long(return_value, "type", node->op_type);
	add_assoc_string(return_value, "type_name", php_parsekit_define_name(node->op_type, php_parsekit_nodetype_names, PHP_PARSEKIT_NODETYPE_UNKNOWN), 1);
	if (node->op_type == IS_CONST) {
		zval *tmpzval;
		MAKE_STD_ZVAL(tmpzval);
		*tmpzval = node->u.constant;
		zval_copy_ctor(tmpzval);
		Z_SET_REFCOUNT_P(tmpzval, 1);
		add_assoc_zval(return_value, "constant", tmpzval);
#ifdef IS_CV
/* PHP >= 5.1 */
	} else if (node->op_type == IS_CV) {
		add_assoc_long(return_value, "var", node->u.var);
		add_assoc_stringl(return_value, "varname", op_array->vars[node->u.var].name, op_array->vars[node->u.var].name_len, 1);
#endif
	} else {
		/* IS_VAR || IS_TMP_VAR || IS_UNUSED */
		char sop[(sizeof(void *) * 2) + 1];

		snprintf(sop, (sizeof(void *) * 2) + 1, "%X", (unsigned int)node->u.var); 

		if ((flags & PHP_PARSEKIT_VAR) ||
			(options & PHP_PARSEKIT_ALL_ELEMENTS)) {
			add_assoc_long(return_value, "var", node->u.var / sizeof(temp_variable));
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(return_value, "var");
		}

		if ((flags & PHP_PARSEKIT_OPLINE) ||
			(options & PHP_PARSEKIT_ALL_ELEMENTS)) {
			add_assoc_string(return_value, "opline_num", sop, 1);
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(return_value, "opline_num");
		}

		if ((flags & PHP_PARSEKIT_OPARRAY) ||
			(options & PHP_PARSEKIT_ALL_ELEMENTS)) {
			add_assoc_string(return_value, "op_array", sop, 1);
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(return_value, "op_array");
		}

#ifdef ZEND_ENGINE_2
/* ZE2 Only */
		if ((flags & PHP_PARSEKIT_JMP_ADDR) ||
			(options & PHP_PARSEKIT_ALL_ELEMENTS)) {
			add_assoc_string(return_value, "jmp_addr", sop, 1);
			snprintf(sop, sizeof(sop)-1, "%u",
					((unsigned int)((char*)node->u.var - (char*)op_array->opcodes))/sizeof(zend_op));
			add_assoc_string(return_value, "jmp_offset", sop, 1); 
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(return_value, "jmp_addr");
		}
#endif

		if ((flags & PHP_PARSEKIT_EA_TYPE) ||
			(options & PHP_PARSEKIT_ALL_ELEMENTS)) {
			add_assoc_long(return_value, "EA.type", node->u.EA.type);
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(return_value, "EA.type");
		}
	}
}
/* }}} */

/* {{{ php_parsekit_parse_op */
static void php_parsekit_parse_op(zval *return_value, zend_op_array *op_array, zend_op *op, long options TSRMLS_DC)
{
	zval *result, *op1, *op2;
	long flags = 0;

	array_init(return_value);

	/* op->handler */
	add_assoc_long(return_value, "address", (unsigned int)(&(op->opcode)));
	add_assoc_long(return_value, "opcode", op->opcode);
	add_assoc_string(return_value, "opcode_name", php_parsekit_define_name_ex(op->opcode, php_parsekit_opcode_names, &flags, PHP_PARSEKIT_OPCODE_UNKNOWN) , 1);
	add_assoc_long(return_value, "flags", flags);

	/* args: result, op1, op2 */
	if ((options & PHP_PARSEKIT_ALL_ELEMENTS) ||
		(flags & PHP_PARSEKIT_RESULT_USED)) {
		MAKE_STD_ZVAL(result);
		php_parsekit_parse_node(result, op_array, &(op->result), flags & PHP_PARSEKIT_RESULT_USED, options TSRMLS_CC);
		add_assoc_zval(return_value, "result", result);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "result");
	}

	if ((options & PHP_PARSEKIT_ALL_ELEMENTS) ||
		(flags & PHP_PARSEKIT_OP1_USED)) {
		MAKE_STD_ZVAL(op1);
		php_parsekit_parse_node(op1, op_array, &(op->op1), flags & PHP_PARSEKIT_OP1_USED, options TSRMLS_CC);
		add_assoc_zval(return_value, "op1", op1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "op1");
	}

	if ((options & PHP_PARSEKIT_ALL_ELEMENTS) ||
		(flags & PHP_PARSEKIT_OP2_USED)) {
		MAKE_STD_ZVAL(op2);
		php_parsekit_parse_node(op2, op_array, &(op->op2), flags & PHP_PARSEKIT_OP2_USED, options TSRMLS_CC);
		add_assoc_zval(return_value, "op2", op2);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "op2");
	}

	if ((options & PHP_PARSEKIT_ALL_ELEMENTS) ||
		(flags & PHP_PARSEKIT_EXTENDED_VALUE)) {
		add_assoc_long(return_value, "extended_value", op->extended_value);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "extended_value");
	}

	add_assoc_long(return_value, "lineno", op->lineno);
}
/* }}} */

#ifdef ZEND_ENGINE_2
/* {{{ php_parsekit_parse_arginfo */
static void php_parsekit_parse_arginfo(zval *return_value, zend_uint num_args, zend_arg_info *arginfo, long options TSRMLS_DC)
{
	zend_uint i;

	array_init(return_value);

	for(i = 0; i < num_args; i++) {
		zval *tmpzval;

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		add_assoc_stringl(tmpzval, "name", arginfo[i].name, arginfo[i].name_len, 1);
		if (arginfo[i].class_name_len) {
			add_assoc_stringl(tmpzval, "class_name", arginfo[i].class_name, arginfo[i].class_name_len, 1);
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(tmpzval, "class_name");
		}
		add_assoc_bool(tmpzval, "allow_null", arginfo[i].allow_null);
		add_assoc_bool(tmpzval, "pass_by_reference", arginfo[i].pass_by_reference);

		add_next_index_zval(return_value, tmpzval);
	}	
}
/* }}} */
#else
/* {{{ php_parsekit_derive_arginfo 
	ZE1 Func Arg loading is done via opcodes "RECV"ing from the caller */
static void php_parsekit_derive_arginfo(zval *return_value, zend_op_array *op_array, long options TSRMLS_DC)
{
	int i;
	zend_op *opcodes = op_array->opcodes;

	array_init(return_value);

	/* Received vars come in pairs:
		A ZEND_FETCH_W, and a ZEND_RECV */
	for(i = 0; i < op_array->arg_types[0]; i++) {
		if (opcodes[i*2].opcode   == ZEND_FETCH_W &&
			opcodes[i*2].op1.op_type == IS_CONST &&
			opcodes[i*2].op1.u.constant.type == IS_STRING &&
			(opcodes[(i*2)+1].opcode == ZEND_RECV || opcodes[(i*2)+1].opcode == ZEND_RECV_INIT)) {
			zval *tmpzval;

			MAKE_STD_ZVAL(tmpzval);
			array_init(tmpzval);

			add_assoc_stringl(tmpzval, "name", opcodes[i*2].op1.u.constant.value.str.val, opcodes[i*2].op1.u.constant.value.str.len, 1);
			add_assoc_bool(tmpzval, "pass_by_reference", op_array->arg_types[i+1]);
			if (opcodes[(i*2)+1].opcode == ZEND_RECV_INIT &&
				opcodes[(i*2)+1].op2.op_type == IS_CONST) {
				zval *def;
	
				MAKE_STD_ZVAL(def);
				*def = opcodes[(i*2)+1].op2.u.constant;
				zval_copy_ctor(def);
				add_assoc_zval(tmpzval, "default", def);
				Z_SET_REFCOUNT_P(tmpzval, 1);
			}

			add_next_index_zval(return_value, tmpzval);
		}
	}
}
/* }}} */
#endif

/* {{{ php_parsekit_parse_op_array */
static void php_parsekit_parse_op_array(zval *return_value, zend_op_array *ops, long options TSRMLS_DC)
{
	zend_op *op;
	zval *tmpzval;
	int i = 0;

	/* TODO: Reorder / Organize */

	array_init(return_value);

	add_assoc_long(return_value, "type", (long)(ops->type));
	add_assoc_string(return_value, "type_name", php_parsekit_define_name(ops->type, php_parsekit_function_types, PHP_PARSEKIT_FUNCTYPE_UNKNOWN), 1);
	if (ops->function_name) {
		add_assoc_string(return_value, "function_name", ops->function_name, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "function_name");
	}

#ifdef ZEND_ENGINE_2
/* ZE2 op_array members */
	if (ops->scope && ops->scope->name) {
		add_assoc_stringl(return_value, "scope", ops->scope->name, ops->scope->name_length, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "scope");
	}
	add_assoc_long(return_value, "fn_flags", ops->fn_flags);
	if (ops->function_name && ops->prototype) {
		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		add_assoc_long(tmpzval, "type", ops->prototype->type);
		add_assoc_string(return_value, "type_name", php_parsekit_define_name(ops->prototype->type, php_parsekit_function_types, PHP_PARSEKIT_FUNCTYPE_UNKNOWN), 1);
		if (ops->prototype->common.function_name) {
			add_assoc_string(tmpzval, "function_name", ops->prototype->common.function_name, 1);
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(tmpzval, "function_name");
		}
		if (ops->prototype->common.scope && ops->prototype->common.scope->name) {
			add_assoc_stringl(tmpzval, "scope", ops->prototype->common.scope->name, ops->prototype->common.scope->name_length, 1);
		} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
			add_assoc_null(tmpzval, "scope");
		}
		add_assoc_zval(return_value, "prototype", tmpzval);		
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "prototype");
	}
	add_assoc_long(return_value, "num_args", ops->num_args);
	add_assoc_long(return_value, "required_num_args", ops->required_num_args);
	add_assoc_bool(return_value, "pass_rest_by_reference", ops->pass_rest_by_reference);

	if (ops->num_args && ops->arg_info) {
		MAKE_STD_ZVAL(tmpzval);
		php_parsekit_parse_arginfo(tmpzval, ops->num_args, ops->arg_info, options TSRMLS_CC);
		add_assoc_zval(return_value, "arg_info", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "arg_info");
	}

	if (ops->last_try_catch > 0) {
		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		for(i = 0; i < ops->last_try_catch; i++) {
			zval *tmp_zval;

			MAKE_STD_ZVAL(tmp_zval);
			array_init(tmp_zval);
			add_assoc_long(tmp_zval, "try_op", ops->try_catch_array[i].try_op);
			add_assoc_long(tmp_zval, "catch_op", ops->try_catch_array[i].catch_op);
			add_index_zval(tmpzval, i, tmp_zval);
		}
		add_assoc_zval(return_value, "try_catch_array", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "try_catch_array");
	}

#ifndef ZEND_ACC_CLOSURE
/* PHP<5.3 */
	add_assoc_bool(return_value, "uses_this", ops->uses_this);
#endif
	add_assoc_long(return_value, "line_start", ops->line_start);
	add_assoc_long(return_value, "line_end", ops->line_end);

	if (ops->doc_comment && ops->doc_comment_len) {
		add_assoc_stringl(return_value, "doc_comment", ops->doc_comment, ops->doc_comment_len, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "doc_comment");
	}

#else
/* ZE1 op_array members */

	if (ops->arg_types) {
		zend_uchar *arg_types = ops->arg_types;
		int numargs = *(ops->arg_types);

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		add_assoc_long(tmpzval, "arg_count", numargs);

		for(i = 0; i < numargs; i++) {
			add_next_index_long(tmpzval, arg_types[i+1]);
		}

		add_assoc_zval(return_value, "arg_types", tmpzval);

		/* Emulated arg_info */
		MAKE_STD_ZVAL(tmpzval);
		php_parsekit_derive_arginfo(tmpzval, ops, options TSRMLS_CC);
		add_assoc_zval(return_value, "arg_info", tmpzval);
	} else {
		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		add_assoc_long(tmpzval, "arg_count", 0);

		add_assoc_zval(return_value, "arg_types", tmpzval);
		add_assoc_null(return_value, "arg_info");
	}

	add_assoc_bool(return_value, "uses_global", ops->uses_globals);
#endif
/* ZE1 and ZE2 */

	add_assoc_bool(return_value, "return_reference", ops->return_reference);
	add_assoc_long(return_value, "refcount", *(ops->refcount));
	add_assoc_long(return_value, "last", ops->last);
	add_assoc_long(return_value, "size", ops->size);
	add_assoc_long(return_value, "T", ops->T);
	add_assoc_long(return_value, "last_brk_cont", ops->last_brk_cont);
	add_assoc_long(return_value, "current_brk_cont", ops->current_brk_cont);
	add_assoc_long(return_value, "backpatch_count", ops->backpatch_count);
	add_assoc_bool(return_value, "done_pass_two", ops->done_pass_two);

	if (ops->last_brk_cont > 0) {
		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		for(i = 0; i < ops->last_brk_cont; i++) {
			zval *tmp_zval;

			MAKE_STD_ZVAL(tmp_zval);
			array_init(tmp_zval);
			add_assoc_long(tmp_zval, "cont", ops->brk_cont_array[i].cont);
			add_assoc_long(tmp_zval, "brk", ops->brk_cont_array[i].brk);
			add_assoc_long(tmp_zval, "parent", ops->brk_cont_array[i].parent);
			add_index_zval(tmpzval, i, tmp_zval);
		}
		add_assoc_zval(return_value, "brk_cont_array", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "brk_cont_array");
	}

	if (ops->static_variables) {
		zval *tmp_zval;

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		zend_hash_copy(HASH_OF(tmpzval), ops->static_variables, (copy_ctor_func_t) zval_add_ref, (void *) &tmp_zval, sizeof(zval *));
		add_assoc_zval(return_value, "static_variables", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "static_variables");
	}

	if (ops->start_op) {
		char sop[(sizeof(void *) * 2) + 1];

		snprintf(sop, sizeof(sop), "%X", (unsigned int)ops->start_op); 
		add_assoc_string(return_value, "start_op", sop, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "start_op");
	}

	if (ops->filename) {
		add_assoc_string(return_value, "filename", ops->filename, 1);
	} else {
		add_assoc_null(return_value, "filename");
	}

	/* Leave this last, it simplifies readability */
	MAKE_STD_ZVAL(tmpzval);
	array_init(tmpzval);
	for(op = ops->opcodes, i = 0; op && i < ops->size; op++, i++) {
		zval *zop;

		MAKE_STD_ZVAL(zop);
		php_parsekit_parse_op(zop, ops, op, options TSRMLS_CC);
		add_next_index_zval(tmpzval, zop);
	}	
	add_assoc_zval(return_value, "opcodes", tmpzval);

}
/* }}} */

/* {{{ php_parsekit_parse_node_simple */
static int php_parsekit_parse_node_simple(char **pret, zend_op_array *op_array, znode *node, zend_op_array *oparray TSRMLS_DC)
{
	if (node->op_type == IS_UNUSED) {
		if (node->u.var) {
#ifdef ZEND_ENGINE_2
			if (node->u.jmp_addr >= oparray->opcodes &&
				node->u.jmp_addr <= (oparray->opcodes + (sizeof(zend_op) * oparray->size))) 
			{
				spprintf(pret, 0, "#%d", node->u.jmp_addr - oparray->opcodes);
			} 
			else
#endif
			{
				spprintf(pret, 0, "0x%X", node->u.var);
			}
			return 1;
		} else {
			*pret =  "UNUSED";
			return 0;
		}
	}
	if (node->op_type == IS_CONST) {
		switch (node->u.constant.type) {
			case IS_NULL:
				*pret = "NULL";
				return 0;
				break;
			case IS_BOOL:
				if (node->u.constant.value.lval) {
					*pret = "TRUE";
				} else {
					*pret = "FALSE";
				}
				return 0;
				break;
			case IS_LONG:
				spprintf(pret, 0, "%ld", node->u.constant.value.lval);
				return 1;
				break;
			case IS_DOUBLE:
				spprintf(pret, 0, "%f", node->u.constant.value.dval);
				return 1;
				break;
			case IS_STRING:
				if (node->u.constant.value.str.len > 15) {
					spprintf(pret, 0, "'%12s...'", node->u.constant.value.str.val);
				} else {
					spprintf(pret, 0, "'%s'", node->u.constant.value.str.val);
				}
				return 1;
				break;
			/* Should these ever occur? */
			case IS_RESOURCE:
				spprintf(pret, 0, "Resource ID#%ld", node->u.constant.value.lval);
				return 1;
				break;
			case IS_ARRAY:
				*pret = "Array";
				return 0;
				break;
			case IS_OBJECT:
				*pret = "Object";
				return 0;
				break;
			default:
				*pret = "Unknown";
				return 0;
		}
	}

	spprintf(pret, 0, "T(%d)", node->u.var / sizeof(temp_variable));
	return 1;
}
/* }}} */

/* {{{ php_parsekit_parse_op_array_simple */
static void php_parsekit_parse_op_array_simple(zval *return_value, zend_op_array *ops, long options TSRMLS_DC)
{
	zend_op *op;
	int i;
	long flags;

	array_init(return_value);

	for (op = ops->opcodes, i = 0; op && i < ops->size; op++, i++) {
		char *opline, *result, *op1, *op2;
		int opline_len, freeit = 0;

		if (php_parsekit_parse_node_simple(&result, ops, &(op->result), ops TSRMLS_CC)) {
			freeit |= 1;
		}
		if (php_parsekit_parse_node_simple(&op1, ops, &(op->op1), ops TSRMLS_CC)) {
			freeit |= 2;
		}
		if (php_parsekit_parse_node_simple(&op2, ops, &(op->op2), ops TSRMLS_CC)) {
			freeit |= 4;
		}

		opline_len = spprintf(&opline, 0, "%s %s %s %s", 
			php_parsekit_define_name_ex(op->opcode, php_parsekit_opcode_names, &flags, PHP_PARSEKIT_OPCODE_UNKNOWN),
			result, op1, op2);

		if (freeit & 1) efree(result);
		if (freeit & 2) efree(op1);
		if (freeit & 4) efree(op2);

		add_next_index_stringl(return_value, opline, opline_len, 0);
	}
}
/* }}} */

/* {{{ php_parsekit_pop_functions */
static int php_parsekit_pop_functions(zval *return_value, HashTable *function_table, int target_count, long options TSRMLS_DC)
{
	HashPosition pos;

	array_init(return_value);

	zend_hash_internal_pointer_end_ex(function_table, &pos);
	while (target_count < zend_hash_num_elements(function_table)) {
		long func_index;
		unsigned int func_name_len;
		char *func_name;
		zend_function *function;
		zval *function_ops;

		if (zend_hash_get_current_data_ex(function_table, (void **)&function, &pos) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to remove pollution from function table: Illegal function entry found.");
			return FAILURE;
		}
		if (function->type == ZEND_INTERNAL_FUNCTION) {
			/* Inherited internal method */
			zend_hash_move_backwards_ex(function_table, &pos);
			target_count++;
			continue;
		} else if (function->type != ZEND_USER_FUNCTION) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to remove pollution from function table: %s%s%s - "
							"Found %s where ZEND_USER_FUNCTION was expected.",
							function->common.scope ? function->common.scope->name : "",
							function->common.scope ? "::" : "",
							function->common.function_name,
							php_parsekit_define_name(function->type, php_parsekit_function_types, PHP_PARSEKIT_FUNCTYPE_UNKNOWN));
			return FAILURE;
		}
		MAKE_STD_ZVAL(function_ops);
		if (options == PHP_PARSEKIT_SIMPLE) {
			php_parsekit_parse_op_array_simple(function_ops, &(function->op_array), options TSRMLS_CC);
		} else {
			php_parsekit_parse_op_array(function_ops, &(function->op_array), options TSRMLS_CC);
		}
		add_assoc_zval(return_value, function->common.function_name, function_ops);

		if (zend_hash_get_current_key_ex(function_table, &func_name, &func_name_len, &func_index, 0, &pos) == HASH_KEY_IS_STRING) {
			zend_hash_move_backwards_ex(function_table, &pos);

			/* TODO: dispose of the function properly */
			if (zend_hash_del(function_table, func_name, func_name_len) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to remove pollution from function table: Unknown hash_del failure.");
				return FAILURE;
			}
		} else {
			/* Absolutely no reason this should ever occur */
			zend_hash_move_backwards_ex(function_table, &pos);
			zend_hash_index_del(function_table, func_index);
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ php_parsekit_parse_class_entry */
static int php_parsekit_parse_class_entry(zval *return_value, zend_class_entry *ce, long options TSRMLS_DC)
{
	zval *tmpzval;
#ifdef ZEND_ENGINE_2
	int i;
#endif

	array_init(return_value);

	add_assoc_long(return_value, "type", ce->type);
	add_assoc_string(return_value, "type_name", php_parsekit_define_name(ce->type, php_parsekit_class_types, PHP_PARSEKIT_CLASSTYPE_UNKNOWN), 1);
	add_assoc_stringl(return_value, "name", ce->name, ce->name_length, 1);
	if (ce->parent) {
		add_assoc_stringl(return_value, "parent", ce->parent->name, ce->parent->name_length, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "parent");
	}
	add_assoc_bool(return_value, "constants_updated", ce->constants_updated);
#ifdef ZEND_ENGINE_2
/* ZE2 class_entry members */
	add_assoc_long(return_value, "ce_flags", ce->ce_flags);

	/* function table pop destorys entries! */
	if (ce->constructor) {
		add_assoc_string(return_value, "constructor", ce->constructor->common.function_name, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "constructor");
	}

	if (ce->clone) {
		add_assoc_string(return_value, "clone", ce->clone->common.function_name, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "clone");
	}

	if (ce->__get) {
		add_assoc_string(return_value, "__get", ce->__get->common.function_name, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "__get");
	}

	if (ce->__set) {
		add_assoc_string(return_value, "__set", ce->__set->common.function_name, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "__set");
	}

	if (ce->__call) {
		add_assoc_string(return_value, "__call", ce->__call->common.function_name, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "__call");
	}

	if (zend_hash_num_elements(&(ce->properties_info)) > 0) {
		zend_property_info *property_info;

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		for(zend_hash_internal_pointer_reset(&(ce->properties_info));
			zend_hash_get_current_data(&(ce->properties_info), (void **)&property_info) == SUCCESS;
			zend_hash_move_forward(&(ce->properties_info))) {
			zval *tmp_zval;

			MAKE_STD_ZVAL(tmp_zval);
			array_init(tmp_zval);
			add_assoc_long(tmp_zval, "flags", property_info->flags);
			add_assoc_stringl(tmp_zval, "name", property_info->name, property_info->name_length, 1);
			add_assoc_long(tmp_zval, "h", property_info->h);
			add_next_index_zval(tmpzval, tmp_zval);
		}
		add_assoc_zval(return_value, "properties_info", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "properties_info");
	}
	
	if (ce->static_members && zend_hash_num_elements(ce->static_members) > 0) {
		zval *tmp_zval;

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		zend_hash_copy(HASH_OF(tmpzval), ce->static_members, (copy_ctor_func_t) zval_add_ref, (void *) &tmp_zval, sizeof(zval *));	
		add_assoc_zval(return_value, "static_members", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "static_members");
	}

	if (zend_hash_num_elements(&(ce->constants_table)) > 0) {
		zval *tmp_zval;

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		zend_hash_copy(HASH_OF(tmpzval), &(ce->constants_table), (copy_ctor_func_t) zval_add_ref, (void *) &tmp_zval, sizeof(zval *));	
		add_assoc_zval(return_value, "constants_table", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "constants_table");
	}

	if (ce->num_interfaces > 0) {
		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		for(i = 0; i < ce->num_interfaces; i++) {
			add_next_index_stringl(tmpzval, ce->interfaces[i]->name, ce->interfaces[i]->name_length, 1);
		}
		add_assoc_zval(return_value, "interfaces", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "interfaces");
	}

	add_assoc_string(return_value, "filename", ce->filename, 1);
	add_assoc_long(return_value, "line_start", ce->line_start);
	add_assoc_long(return_value, "line_end", ce->line_end);
	if (ce->doc_comment) {
		add_assoc_stringl(return_value, "doc_comment", ce->doc_comment, ce->doc_comment_len, 1);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "doc_comment");
	}

	add_assoc_long(return_value, "refcount", Z_REFCOUNT_P(ce));

#else
/* ZE1 class_entry members */
	if (ce->refcount) {
		add_assoc_long(return_value, "refcount", *(ce->refcount));
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "refcount");
	}

#endif
/* ZE1 and ZE2 */

	if (zend_hash_num_elements(&(ce->function_table)) > 0) {
		MAKE_STD_ZVAL(tmpzval);
		if (php_parsekit_pop_functions(tmpzval, &(ce->function_table), 0, options TSRMLS_CC) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to cleanup class %s: Error scrubbing function_table", ce->name);
			return FAILURE;
		}
		add_assoc_zval(return_value, "function_table", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "function_table");
	}

	if (zend_hash_num_elements(&(ce->default_properties)) > 0) {
		zval *tmp_zval;

		MAKE_STD_ZVAL(tmpzval);
		array_init(tmpzval);
		zend_hash_copy(HASH_OF(tmpzval), &(ce->default_properties), (copy_ctor_func_t) zval_add_ref, (void *) &tmp_zval, sizeof(zval *));	
		add_assoc_zval(return_value, "default_properties", tmpzval);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "default_properties");
	}

	return SUCCESS;
}
/* }}} */

/* {{{ php_parsekit_pop_classes */
static int php_parsekit_pop_classes(zval *return_value, HashTable *class_table, int target_count, long options TSRMLS_DC)
{
	array_init(return_value);

	while (target_count < zend_hash_num_elements(class_table)) {
		long class_index;
		unsigned int class_name_len;
		char *class_name;
		zend_class_entry *class_entry, **pce;
		zval *class_data;

		zend_hash_internal_pointer_end(class_table);
		if (zend_hash_get_current_data(class_table, (void **)&pce) == FAILURE || !pce || !(*pce)) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to remove pollution from class table: Illegal class entry found.");
			return FAILURE;
		}
#ifdef ZEND_ENGINE_2
		class_entry = *pce;
#else
		class_entry = (zend_class_entry*)pce;
#endif
		if (class_entry->type != ZEND_USER_CLASS) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to remove pollution from class table: %s - "
							"Found %s where ZEND_USER_CLASS was expected.", class_entry->name,
							php_parsekit_define_name(class_entry->type, php_parsekit_class_types, PHP_PARSEKIT_CLASSTYPE_UNKNOWN));
			return FAILURE;
		}
		MAKE_STD_ZVAL(class_data);
		if (php_parsekit_parse_class_entry(class_data, class_entry, options TSRMLS_CC) == FAILURE) {
			return FAILURE; /* Exit gracefully even though the E_ERROR condition will clean up after us */
		}
		add_assoc_zval(return_value, class_entry->name, class_data);

		if (zend_hash_get_current_key_ex(class_table, &class_name, &class_name_len, &class_index, 0, NULL) == HASH_KEY_IS_STRING) {
			/* TODO: dispose of the class properly */
			if (zend_hash_del(class_table, class_name, class_name_len) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to remove pollution from class table: Unknown hash_del failure.");
				return FAILURE;
			}
		} else {
			/* Absolutely no reason this should ever occur */
			zend_hash_index_del(class_table, class_index);
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ php_parsekit_common */
static void php_parsekit_common(zval *return_value, int original_num_functions, int original_num_classes, zend_op_array *ops, long options TSRMLS_DC)
{
	zval *declared_functions, *declared_classes;

	/* main() */
	if (options == PHP_PARSEKIT_SIMPLE) {
		php_parsekit_parse_op_array_simple(return_value, ops, options TSRMLS_CC);
	} else {
		php_parsekit_parse_op_array(return_value, ops, options TSRMLS_CC);
	}

	if (original_num_functions < zend_hash_num_elements(EG(function_table))) {
		/* The compiled code introduced new functions, get them out of there! */
		MAKE_STD_ZVAL(declared_functions);
		php_parsekit_pop_functions(declared_functions, EG(function_table), original_num_functions, options TSRMLS_CC);
		add_assoc_zval(return_value, "function_table", declared_functions);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "function_table");
	}

	if (original_num_classes < zend_hash_num_elements(EG(class_table))) {
		/* The compiled code introduced new classes, get them out of here */
		MAKE_STD_ZVAL(declared_classes);
		php_parsekit_pop_classes(declared_classes, EG(class_table), original_num_classes, options TSRMLS_CC);
		add_assoc_zval(return_value, "class_table", declared_classes);
	} else if (options & PHP_PARSEKIT_ALWAYS_SET) {
		add_assoc_null(return_value, "class_table");
	}
}
/* }}} */

/* ****************************************** */
/* Module Housekeeping and Userland Functions */
/* ****************************************** */

/* {{{ proto array parsekit_compile_string(string phpcode[, array &errors[, int options]])
   Return array of opcodes compiled from phpcode */
PHP_FUNCTION(parsekit_compile_string)
{
	int original_num_functions = zend_hash_num_elements(EG(function_table));
	int original_num_classes = zend_hash_num_elements(EG(class_table));
	zend_uchar original_compiler_options;
	zend_op_array *ops;
	zval *zcode, *zerrors = NULL;
	long options = PHP_PARSEKIT_QUIET;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zl", &zcode, &zerrors, &options) == FAILURE) {
		RETURN_FALSE;
	}

	if (zerrors) {
		zval_dtor(zerrors);
		ZVAL_NULL(zerrors);
		PARSEKIT_G(compile_errors) = zerrors;
	}

	convert_to_string(zcode);
#ifdef ZEND_COMPILE_HANDLE_OP_ARRAY
	original_compiler_options = CG(compiler_options);
	CG(compiler_options) = CG(compiler_options) & ~ZEND_COMPILE_HANDLE_OP_ARRAY;
#else
	original_compiler_options = CG(handle_op_arrays);
	CG(handle_op_arrays) = 0;
#endif
	PARSEKIT_G(in_parsekit_compile) = 1;

	zend_try {
		ops = compile_string(zcode, "Parsekit Compiler" TSRMLS_CC);
	} zend_catch {
		ops = NULL;
	} zend_end_try();

	PARSEKIT_G(in_parsekit_compile) = 0;
	PARSEKIT_G(compile_errors) = NULL;

#ifdef ZEND_COMPILE_HANDLE_OP_ARRAY
	CG(compiler_options) = original_compiler_options;
#else
	CG(handle_op_arrays) = original_compiler_options;
#endif

	if (ops) {
		php_parsekit_common(return_value, original_num_functions, original_num_classes, ops, options TSRMLS_CC);
		destroy_op_array(ops PHP_PARSEKIT_TSRMLS_CC_ZE2ONLY);
		efree(ops);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array parsekit_compile_file(string filename[, array &errors[, int options]])
   Return array of opcodes compiled from phpfile */
PHP_FUNCTION(parsekit_compile_file)
{
	int original_num_functions = zend_hash_num_elements(EG(function_table));
	int original_num_classes = zend_hash_num_elements(EG(class_table));
	zend_uchar original_compiler_options;
	zend_op_array *ops;
	zval *zfilename, *zerrors = NULL;
	long options = PHP_PARSEKIT_QUIET;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zl", &zfilename, &zerrors, &options) == FAILURE) {
		RETURN_FALSE;
	}

	if (zerrors) {
		zval_dtor(zerrors);
		ZVAL_NULL(zerrors);
		PARSEKIT_G(compile_errors) = zerrors;
	}

	convert_to_string(zfilename);
#ifdef ZEND_COMPILE_HANDLE_OP_ARRAY
	original_compiler_options = CG(compiler_options);
	CG(compiler_options) = CG(compiler_options) & ~ZEND_COMPILE_HANDLE_OP_ARRAY;
#else
	original_compiler_options = CG(handle_op_arrays);
	CG(handle_op_arrays) = 0;
#endif
	PARSEKIT_G(in_parsekit_compile) = 1;

	zend_try {
		ops = compile_filename(ZEND_INCLUDE, zfilename TSRMLS_CC);
	} zend_catch {
		ops = NULL;
	} zend_end_try();

	PARSEKIT_G(in_parsekit_compile) = 0;
	PARSEKIT_G(compile_errors) = NULL;

#ifdef ZEND_COMPILE_HANDLE_OP_ARRAY
	CG(compiler_options) = original_compiler_options;
#else
	CG(handle_op_arrays) = original_compiler_options;
#endif

	if (ops) {
		php_parsekit_common(return_value, original_num_functions, original_num_classes, ops, options TSRMLS_CC);

		destroy_op_array(ops PHP_PARSEKIT_TSRMLS_CC_ZE2ONLY);
		efree(ops);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto long parsekit_opcode_flags(long opcode)
	Return the flags associated with an opcode */
PHP_FUNCTION(parsekit_opcode_flags)
{
	long opcode;
	php_parsekit_define_list *opcodes = php_parsekit_opcode_names;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &opcode) == FAILURE) {
		RETURN_FALSE;
	}

	while (opcodes) {
		if (opcodes->val == opcode) {
			RETURN_LONG(opcodes->flags);
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string parsekit_opcode_name(long opcode)
	Return the name of a given opcode */
PHP_FUNCTION(parsekit_opcode_name)
{
	long opcode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &opcode) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_STRING(php_parsekit_define_name(opcode, php_parsekit_opcode_names, PHP_PARSEKIT_OPCODE_UNKNOWN), 1);
}
/* }}} */

/* {{{ proto array parsekit_func_arginfo(mixed function)
	Return the arg_info data for a given user function/method */
PHP_FUNCTION(parsekit_func_arginfo)
{
	zval *function;
	char *class = NULL, *fname = NULL;
	int class_len = 0, fname_len = 0;
	HashTable *function_table = NULL;
	zend_function *fe = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &function) == FAILURE) {
		RETURN_FALSE;
	}

	switch (Z_TYPE_P(function)) {
		case IS_STRING:
			fname = Z_STRVAL_P(function);
			fname_len = Z_STRLEN_P(function);
			function_table = EG(function_table);
			break;
		case IS_ARRAY:
		{
			zval **classname;
			zval **funcname;

			zend_hash_internal_pointer_reset(HASH_OF(function));

			/* Name that class */
			if (zend_hash_get_current_data(HASH_OF(function), (void **)&classname) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expecting string or array containing two elements.");
				RETURN_FALSE;
			}
			if (!classname || !*classname || (Z_TYPE_PP(classname) != IS_STRING && Z_TYPE_PP(classname) != IS_OBJECT)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid class name given");
				RETURN_FALSE;
			}
			if (Z_TYPE_PP(classname) == IS_STRING) {
				class = Z_STRVAL_PP(classname);
				class_len = Z_STRLEN_PP(classname);
			} else {
				class = Z_OBJCE_PP(classname)->name;
				class_len = Z_OBJCE_PP(classname)->name_length;

				/* Save looking it up later! */
				function_table = &(Z_OBJCE_PP(classname)->function_table);
			}

			zend_hash_move_forward(HASH_OF(function));

			/* Name that function */
			if (zend_hash_get_current_data(HASH_OF(function), (void **)&funcname) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expecting string or array containing two elements.");
				RETURN_FALSE;
			}
			if (!funcname || !*funcname || Z_TYPE_PP(funcname) != IS_STRING) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid method name given");
				RETURN_FALSE;
			}
			fname = Z_STRVAL_PP(funcname);
			fname_len = Z_STRLEN_PP(funcname);

			break;
		}
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expecting string or array containing two elements.");
			RETURN_FALSE;
	}

	if (class && !function_table) {
		zend_class_entry **pce;
#ifndef ZEND_ENGINE_2
		zend_class_entry *ce;
#endif

		/* Fetch class's method table */
#ifdef ZEND_ENGINE_2
		if (zend_lookup_class(class, class_len, &pce TSRMLS_CC) == FAILURE) {
#else
		pce = &ce;
		if (zend_hash_find(EG(class_table), class, class_len + 1, (void**)&ce) == FAILURE) {
#endif
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown class: %s", class);
			RETURN_FALSE;
		}
		if (!pce || !*pce) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to fetch class entry.");
			RETURN_FALSE;
		}

		function_table = &((*pce)->function_table);
	}

	if (!function_table) {
		/* Should never happen */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error locating function table");
		RETURN_FALSE;
	}

	if (zend_hash_find(function_table, fname, fname_len + 1, (void **)&fe) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s%s%s() not found.", class ? class : "", class ? "::" : "", fname);
		RETURN_FALSE;
	}

	if (fe->type != ZEND_USER_FUNCTION) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Only user defined functions support reflection");
		RETURN_FALSE;
	}

#ifdef ZEND_ENGINE_2
	php_parsekit_parse_arginfo(return_value, fe->common.num_args, fe->common.arg_info, 0 TSRMLS_CC);
#else
	php_parsekit_derive_arginfo(return_value, &fe->op_array, 0 TSRMLS_CC);
#endif
}
/* }}} */

#ifdef ZEND_ENGINE_2
	ZEND_BEGIN_ARG_INFO(php_parsekit_second_arg_force_ref, 0)
		ZEND_ARG_PASS_INFO(0)
		ZEND_ARG_PASS_INFO(1)
	ZEND_END_ARG_INFO()
#else
static unsigned char php_parsekit_second_arg_force_ref[] = { 3, BYREF_NONE, BYREF_FORCE };
#endif

/* {{{ zend_function_entry */
zend_function_entry parsekit_functions[] = {
	PHP_FE(parsekit_compile_string,			php_parsekit_second_arg_force_ref)
	PHP_FE(parsekit_compile_file,			php_parsekit_second_arg_force_ref)
	PHP_FE(parsekit_opcode_flags,			NULL)
	PHP_FE(parsekit_opcode_name,			NULL)
	PHP_FE(parsekit_func_arginfo,			NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ parsekit_module_entry
 */
zend_module_entry parsekit_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"parsekit",
	parsekit_functions,
	PHP_MINIT(parsekit),
	PHP_MSHUTDOWN(parsekit),
	NULL, /* RINIT */
	NULL, /* RSHUTDOWN */
	PHP_MINFO(parsekit),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_PARSEKIT_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PARSEKIT
ZEND_GET_MODULE(parsekit)
#endif

/* {{{ php_parsekit_error_cb 
	Capture error messages and locations while suppressing otherwise fatal (non-core) errors */
static void php_parsekit_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *buffer;
	int buffer_len;
	zval *tmpzval;
	TSRMLS_FETCH();

	if (!PARSEKIT_G(in_parsekit_compile) || type == E_CORE_ERROR) {
		/* Some normal (or massively abnormal) event triggered this error. */
		php_parsekit_original_error_function(type, (char *)error_filename, error_lineno, format, args);
		return;
	}

	if (!PARSEKIT_G(compile_errors)) {
		/* All errors ignored */
		return;
	}

	/* If an error gets triggered in here, revert to normal handling to avoid potential loop */
	PARSEKIT_G(in_parsekit_compile) = 0;
	MAKE_STD_ZVAL(tmpzval);
	array_init(tmpzval);
	add_assoc_long(tmpzval, "errno", type);
	add_assoc_string(tmpzval, "filename", (char *)error_filename, 1);
	add_assoc_long(tmpzval, "lineno", error_lineno);
	buffer_len = vspprintf(&buffer, PG(log_errors_max_len), format, args);
	add_assoc_stringl(tmpzval, "errstr", buffer, buffer_len, 1);

	if (Z_TYPE_P(PARSEKIT_G(compile_errors)) == IS_NULL) {
		array_init(PARSEKIT_G(compile_errors));
	}
	add_next_index_zval(PARSEKIT_G(compile_errors), tmpzval);

	/* Restore compiler state */
	PARSEKIT_G(in_parsekit_compile) = 1;
}
/* }}} */

#define REGISTER_PARSEKIT_CONSTANTS(define_list)		\
	{	\
		char const_name[96];	\
		int const_name_len;	\
		php_parsekit_define_list *defines = (define_list);	\
		while (defines->str) {	\
			/* the macros don't like variable constant names */ \
			const_name_len = snprintf(const_name, sizeof(const_name), "PARSEKIT_%s", defines->str);	\
			zend_register_long_constant(const_name, const_name_len+1, defines->val, CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC); \
			defines++;	\
		}	\
	}

/* {{{ php_parsekit_init_globals
 */
static void php_parsekit_init_globals(zend_parsekit_globals *parsekit_globals)
{
	parsekit_globals->in_parsekit_compile = 0;
	parsekit_globals->compile_errors = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(parsekit)
{
	REGISTER_PARSEKIT_CONSTANTS(php_parsekit_class_types);
	REGISTER_PARSEKIT_CONSTANTS(php_parsekit_function_types);
	REGISTER_PARSEKIT_CONSTANTS(php_parsekit_nodetype_names);
	REGISTER_PARSEKIT_CONSTANTS(php_parsekit_opcode_names);
	REGISTER_PARSEKIT_CONSTANTS(php_parsekit_opnode_flags);

	REGISTER_LONG_CONSTANT("PARSEKIT_QUIET",	PHP_PARSEKIT_QUIET,		CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PARSEKIT_SIMPLE",	PHP_PARSEKIT_SIMPLE,		CONST_CS | CONST_PERSISTENT );

	ZEND_INIT_MODULE_GLOBALS(parsekit, php_parsekit_init_globals, NULL);

	/* Changing zend_error_cb isn't threadsafe,
	   so we'll have to just change it for everybody
	   and track whether or not we're in parsekit_compile()
	   on a perthread basis and go from there.
	   DANGER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	   This could tank if another module does this same hack
	   before us then unloads. */
	php_parsekit_original_error_function = zend_error_cb;
	zend_error_cb = php_parsekit_error_cb;

	return SUCCESS;
}
/* }}} */

	/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(parsekit)
{
	zend_error_cb = php_parsekit_original_error_function;

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(parsekit)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "parsekit support", "enabled");
	php_info_print_table_row(2, "version", PHP_PARSEKIT_VERSION);
	php_info_print_table_end();
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
