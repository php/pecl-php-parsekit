/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_PARSEKIT_H
#define PHP_PARSEKIT_H

#ifdef ZTS
#include "TSRM.h"
#endif

extern zend_module_entry parsekit_module_entry;
#define phpext_parsekit_ptr &parsekit_module_entry

PHP_MINIT_FUNCTION(parsekit);
PHP_MSHUTDOWN_FUNCTION(parsekit);
PHP_MINFO_FUNCTION(parsekit);

PHP_FUNCTION(parsekit_compile_string);
PHP_FUNCTION(parsekit_compile_file);

ZEND_BEGIN_MODULE_GLOBALS(parsekit)
	int in_parsekit_compile;
	zval *compile_errors;
ZEND_END_MODULE_GLOBALS(parsekit)

#ifdef ZTS
#define PARSEKIT_G(v) TSRMG(parsekit_globals_id, zend_parsekit_globals *, v)
#else
#define PARSEKIT_G(v) (parsekit_globals.v)
#endif

typedef struct _php_parsekit_define_list {
	long val;
	char *str;
} php_parsekit_define_list;

#define PHP_PARSEKIT_UNKNOWN "UNKNOWN"
#define PHP_PARSEKIT_OPCODE_UNKNOWN "Unknown Opcode"
#define PHP_PARSEKIT_NODETYPE_UNKNOWN "Unknown Nodetype"
#define PHP_PARSEKIT_FUNCTYPE_UNKNOWN "Unknown Functiontype"
#define PHP_PARSEKIT_CLASSTYPE_UNKNOWN "Unknown Classtype"

static php_parsekit_define_list php_parsekit_class_types[] = {
	{ ZEND_INTERNAL_CLASS, "ZEND_INTERNAL_CLASS" },
	{ ZEND_USER_CLASS, "ZEND_USER_CLASS" },
	{ 0, NULL }
};

static php_parsekit_define_list php_parsekit_function_types[] = {
	{ ZEND_INTERNAL_FUNCTION, "ZEND_INTERNAL_FUNCTION" },
	{ ZEND_USER_FUNCTION, "ZEND_USER_FUNCTION" },
	{ ZEND_OVERLOADED_FUNCTION, "ZEND_OVERLOADED_FUNCTION" },
	{ ZEND_EVAL_CODE, "ZEND_EVAL_CODE" },
	{ ZEND_OVERLOADED_FUNCTION_TEMPORARY, "ZEND_OVERLOADED_FUNCTION_TEMPORARY" },
	{ 0, NULL }
};

static php_parsekit_define_list php_parsekit_nodetype_names[] = {
	{ IS_CONST, "IS_CONST" },
	{ IS_TMP_VAR, "IS_TMP_VAR" },
	{ IS_VAR, "IS_VAR" },
	{ IS_UNUSED, "IS_UNUSED" },
	{ 0, NULL }
};

static php_parsekit_define_list php_parsekit_opcode_names[] = {
	{ ZEND_NOP, "ZEND_NOP" },
	{ ZEND_ADD, "ZEND_ADD" },
	{ ZEND_SUB, "ZEND_SUB" },
	{ ZEND_MUL, "ZEND_MUL" },
	{ ZEND_DIV, "ZEND_DIV" },
	{ ZEND_MOD, "ZEND_MOD" },
	{ ZEND_SL, "ZEND_SL" },
	{ ZEND_SR, "ZEND_SR" },
	{ ZEND_CONCAT, "ZEND_CONCAT" },
	{ ZEND_BW_OR, "ZEND_BW_OR" },
	{ ZEND_BW_AND, "ZEND_BW_AND" },
	{ ZEND_BW_XOR, "ZEND_BW_XOR" },
	{ ZEND_BW_NOT, "ZEND_BW_NOT" },
	{ ZEND_BOOL_NOT, "ZEND_BOOL_NOT" },
	{ ZEND_BOOL_XOR, "ZEND_BOOL_XOR" },
	{ ZEND_IS_IDENTICAL, "ZEND_IS_IDENTICAL" },
	{ ZEND_IS_NOT_IDENTICAL, "ZEND_IS_NOT_IDENTICAL" },
	{ ZEND_IS_EQUAL, "ZEND_IS_EQUAL" },
	{ ZEND_IS_NOT_EQUAL, "ZEND_IS_NOT_EQUAL" },
	{ ZEND_IS_SMALLER, "ZEND_IS_SMALLER" },
	{ ZEND_IS_SMALLER_OR_EQUAL, "ZEND_IS_SMALLER_OR_EQUAL" },
	{ ZEND_CAST, "ZEND_CAST" },
	{ ZEND_QM_ASSIGN, "ZEND_QM_ASSIGN" },
	{ ZEND_ASSIGN_ADD, "ZEND_ASSIGN_ADD" },
	{ ZEND_ASSIGN_SUB, "ZEND_ASSIGN_SUB" },
	{ ZEND_ASSIGN_MUL, "ZEND_ASSIGN_MUL" },
	{ ZEND_ASSIGN_DIV, "ZEND_ASSIGN_DIV" },
	{ ZEND_ASSIGN_MOD, "ZEND_ASSIGN_MOD" },
	{ ZEND_ASSIGN_SL, "ZEND_ASSIGN_SL" },
	{ ZEND_ASSIGN_SR, "ZEND_ASSIGN_SR" },
	{ ZEND_ASSIGN_CONCAT, "ZEND_ASSIGN_CONCAT" },
	{ ZEND_ASSIGN_BW_OR, "ZEND_ASSIGN_BW_OR" },
	{ ZEND_ASSIGN_BW_AND, "ZEND_ASSIGN_BW_AND" },
	{ ZEND_ASSIGN_BW_XOR, "ZEND_ASSIGN_BW_XOR" },
	{ ZEND_PRE_INC, "ZEND_PRE_INC" },
	{ ZEND_PRE_DEC, "ZEND_PRE_DEC" },
	{ ZEND_POST_INC, "ZEND_POST_INC" },
	{ ZEND_POST_DEC, "ZEND_POST_DEC" },
	{ ZEND_ASSIGN, "ZEND_ASSIGN" },
	{ ZEND_ASSIGN_REF, "ZEND_ASSIGN_REF" },
	{ ZEND_ECHO, "ZEND_ECHO" },
	{ ZEND_PRINT, "ZEND_PRINT" },
	{ ZEND_JMP, "ZEND_JMP" },
	{ ZEND_JMPZ, "ZEND_JMPZ" },
	{ ZEND_JMPNZ, "ZEND_JMPNZ" },
	{ ZEND_JMPZNZ, "ZEND_JMPZNZ" },
	{ ZEND_JMPZ_EX, "ZEND_JMPZ_EX" },
	{ ZEND_JMPNZ_EX, "ZEND_JMPNZ_EX" },
	{ ZEND_CASE, "ZEND_CASE" },
	{ ZEND_SWITCH_FREE, "ZEND_SWITCH_FREE" },
	{ ZEND_BRK, "ZEND_BRK" },
	{ ZEND_CONT, "ZEND_CONT" },
	{ ZEND_BOOL, "ZEND_BOOL" },
	{ ZEND_INIT_STRING, "ZEND_INIT_STRING" },
	{ ZEND_ADD_CHAR, "ZEND_ADD_CHAR" },
	{ ZEND_ADD_STRING, "ZEND_ADD_STRING" },
	{ ZEND_ADD_VAR, "ZEND_ADD_VAR" },
	{ ZEND_BEGIN_SILENCE, "ZEND_BEGIN_SILENCE" },
	{ ZEND_END_SILENCE, "ZEND_END_SILENCE" },
	{ ZEND_INIT_FCALL_BY_NAME, "ZEND_INIT_FCALL_BY_NAME" },
	{ ZEND_DO_FCALL, "ZEND_DO_FCALL" },
	{ ZEND_DO_FCALL_BY_NAME, "ZEND_DO_FCALL_BY_NAME" },
	{ ZEND_RETURN, "ZEND_RETURN" },
	{ ZEND_RECV, "ZEND_RECV" },
	{ ZEND_RECV_INIT, "ZEND_RECV_INIT" },
	{ ZEND_SEND_VAL, "ZEND_SEND_VAL" },
	{ ZEND_SEND_VAR, "ZEND_SEND_VAR" },
	{ ZEND_SEND_REF, "ZEND_SEND_REF" },
	{ ZEND_NEW, "ZEND_NEW" },
	{ ZEND_JMP_NO_CTOR, "ZEND_JMP_NO_CTOR" },
	{ ZEND_FREE, "ZEND_FREE" },
	{ ZEND_INIT_ARRAY, "ZEND_INIT_ARRAY" },
	{ ZEND_ADD_ARRAY_ELEMENT, "ZEND_ADD_ARRAY_ELEMENT" },
	{ ZEND_INCLUDE_OR_EVAL, "ZEND_INCLUDE_OR_EVAL" },
	{ ZEND_UNSET_VAR, "ZEND_UNSET_VAR" },
	{ ZEND_UNSET_DIM_OBJ, "ZEND_UNSET_DIM_OBJ" },
	{ ZEND_FE_RESET, "ZEND_FE_RESET" },
	{ ZEND_FE_FETCH, "ZEND_FE_FETCH" },
	{ ZEND_EXIT, "ZEND_EXIT" },
	{ ZEND_FETCH_R, "ZEND_FETCH_R" },
	{ ZEND_FETCH_DIM_R, "ZEND_FETCH_DIM_R" },
	{ ZEND_FETCH_OBJ_R, "ZEND_FETCH_OBJ_R" },
	{ ZEND_FETCH_W, "ZEND_FETCH_W" },
	{ ZEND_FETCH_DIM_W, "ZEND_FETCH_DIM_W" },
	{ ZEND_FETCH_OBJ_W, "ZEND_FETCH_OBJ_W" },
	{ ZEND_FETCH_RW, "ZEND_FETCH_RW" },
	{ ZEND_FETCH_DIM_RW, "ZEND_FETCH_DIM_RW" },
	{ ZEND_FETCH_OBJ_RW, "ZEND_FETCH_OBJ_RW" },
	{ ZEND_FETCH_IS, "ZEND_FETCH_IS" },
	{ ZEND_FETCH_DIM_IS, "ZEND_FETCH_DIM_IS" },
	{ ZEND_FETCH_OBJ_IS, "ZEND_FETCH_OBJ_IS" },
	{ ZEND_FETCH_FUNC_ARG, "ZEND_FETCH_FUNC_ARG" },
	{ ZEND_FETCH_DIM_FUNC_ARG, "ZEND_FETCH_DIM_FUNC_ARG" },
	{ ZEND_FETCH_OBJ_FUNC_ARG, "ZEND_FETCH_OBJ_FUNC_ARG" },
	{ ZEND_FETCH_UNSET, "ZEND_FETCH_UNSET" },
	{ ZEND_FETCH_DIM_UNSET, "ZEND_FETCH_DIM_UNSET" },
	{ ZEND_FETCH_OBJ_UNSET, "ZEND_FETCH_OBJ_UNSET" },
	{ ZEND_FETCH_DIM_TMP_VAR, "ZEND_FETCH_DIM_TMP_VAR" },
	{ ZEND_FETCH_CONSTANT, "ZEND_FETCH_CONSTANT" },
	{ ZEND_EXT_STMT, "ZEND_EXT_STMT" },
	{ ZEND_EXT_FCALL_BEGIN, "ZEND_EXT_FCALL_BEGIN" },
	{ ZEND_EXT_FCALL_END, "ZEND_EXT_FCALL_END" },
	{ ZEND_EXT_NOP, "ZEND_EXT_NOP" },
	{ ZEND_TICKS, "ZEND_TICKS" },
	{ ZEND_SEND_VAR_NO_REF, "ZEND_SEND_VAR_NO_REF" },
	{ ZEND_CATCH, "ZEND_CATCH" },
	{ ZEND_THROW, "ZEND_THROW" },
	{ ZEND_FETCH_CLASS, "ZEND_FETCH_CLASS" },
	{ ZEND_CLONE, "ZEND_CLONE" },
	{ ZEND_INIT_CTOR_CALL, "ZEND_INIT_CTOR_CALL" },
	{ ZEND_INIT_METHOD_CALL, "ZEND_INIT_METHOD_CALL" },
	{ ZEND_INIT_STATIC_METHOD_CALL, "ZEND_INIT_STATIC_METHOD_CALL" },
	{ ZEND_ISSET_ISEMPTY_VAR, "ZEND_ISSET_ISEMPTY_VAR" },
	{ ZEND_ISSET_ISEMPTY_DIM_OBJ, "ZEND_ISSET_ISEMPTY_DIM_OBJ" },
	{ ZEND_IMPORT_FUNCTION, "ZEND_IMPORT_FUNCTION" },
	{ ZEND_IMPORT_CLASS, "ZEND_IMPORT_CLASS" },
	{ ZEND_IMPORT_CONST, "ZEND_IMPORT_CONST" },
	{ ZEND_PRE_INC_OBJ, "ZEND_PRE_INC_OBJ" },
	{ ZEND_PRE_DEC_OBJ, "ZEND_PRE_DEC_OBJ" },
	{ ZEND_POST_INC_OBJ, "ZEND_POST_INC_OBJ" },
	{ ZEND_POST_DEC_OBJ, "ZEND_POST_DEC_OBJ" },
	{ ZEND_ASSIGN_OBJ, "ZEND_ASSIGN_OBJ" },
	{ ZEND_OP_DATA, "ZEND_OP_DATA" },
	{ ZEND_INSTANCEOF, "ZEND_INSTANCEOF" },
	{ ZEND_DECLARE_CLASS, "ZEND_DECLARE_CLASS" },
	{ ZEND_DECLARE_INHERITED_CLASS, "ZEND_DECLARE_INHERITED_CLASS" },
	{ ZEND_DECLARE_FUNCTION, "ZEND_DECLARE_FUNCTION" },
	{ ZEND_RAISE_ABSTRACT_ERROR, "ZEND_RAISE_ABSTRACT_ERROR" },
	{ ZEND_ADD_INTERFACE, "ZEND_ADD_INTERFACE" },
	{ ZEND_VERIFY_ABSTRACT_CLASS, "ZEND_VERIFY_ABSTRACT_CLASS" },
	{ ZEND_ASSIGN_DIM, "ZEND_ASSIGN_DIM" },
	{ ZEND_ISSET_ISEMPTY_PROP_OBJ, "ZEND_ISSET_ISEMPTY_PROP_OBJ" },
	{ ZEND_HANDLE_EXCEPTION, "ZEND_HANDLE_EXCEPTION" },
	{ 0, NULL }
};

#else

#define phpext_parsekit_ptr &parsekit_module_entry

#endif	/* PHP_PARSEKIT_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

