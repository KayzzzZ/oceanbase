/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SQL_ENG

#include "sql/engine/expr/ob_expr_field.h"

#include "sql/engine/expr/ob_expr_equal.h"
#include "sql/session/ob_sql_session_info.h"

namespace oceanbase
{
using namespace oceanbase::common;
namespace sql
{

ObExprField::ObExprField(ObIAllocator &alloc) : ObVectorExprOperator(alloc, T_FUN_SYS_FIELD, N_FIELD, MORE_THAN_ONE,
                                                      NOT_ROW_DIMENSION), need_cast_(true) {}

int ObExprField::assign(const ObExprOperator &other)
{
  int ret = OB_SUCCESS;
  const ObExprField *tmp_other = dynamic_cast<const ObExprField *>(&other);
  if (OB_UNLIKELY(NULL == tmp_other)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument. wrong type for other", K(ret), K(other));
  } else if (OB_LIKELY(this != tmp_other)) {
    if (OB_FAIL(ObVectorExprOperator::assign(other))) {
      LOG_WARN("copy in Base class ObVectorExprOperator failed", K(ret));
    } else {
      this->need_cast_ = tmp_other->need_cast_;
    }
  }
  return ret;
}

int ObExprField::calc_result_typeN(ObExprResType &type,
                                   ObExprResType *types_stack,
                                   int64_t param_num,
                                   ObExprTypeCtx &type_ctx) const
{
  int ret = OB_SUCCESS;
  bool has_num = false;
  bool has_string = false;
  bool has_real = false;
  CK(NULL != type_ctx.get_session());
  if (OB_FAIL(ret)) {
  } else if (param_num < 2) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(param_num));
  } else {
    ObObjTypeClass type_class = ObNullTC;
    for (int64_t i = 0; OB_SUCC(ret) && i < param_num; ++i) {
      type_class = types_stack[i].get_type_class();
      if (ObNullTC == type_class || ObEnumSetTC == type_class) {
        // nothing to do
      } else if (ObFloatTC == type_class || ObDoubleTC == type_class) {
        has_real = true;
      } else if (type_class >= ObDateTimeTC && type_class < ObMaxTC && type_class != ObYearTC
                 && ObEnumSetInnerTC != type_class) {
        has_string = true;
      } else if ((type_class < ObDateTimeTC && type_class > ObNullTC) || type_class == ObYearTC) {
        has_num = true;
      } else {
        ret = OB_ERR_ILLEGAL_TYPE;
        LOG_WARN("invalid type", K(ret), K(type_class));
      }
    }
    if (OB_SUCC(ret)) {
      if (has_real || (has_num && has_string)) {
        type.set_calc_type(ObDoubleType);
      } else if (has_string) {
        type.set_calc_type(ObVarcharType);
      } else if (has_num) {
        type.set_calc_type(ObNumberType);
      }

      ObObjType calc_type = type.get_calc_type();

      if (ObVarcharType == calc_type) {
        for (int64_t i = 0; OB_SUCC(ret) && i < param_num; ++i) {
          if (ob_is_enumset_tc(types_stack[i].get_type())) {
            types_stack[i].set_calc_type(calc_type);
          }
        }
      }

      type.set_int();
      //调研mysql precision为3
      type.set_precision(3);
      //calc comparison collation,etc
      type.set_scale(DEFAULT_SCALE_FOR_INTEGER);
      if (ob_is_string_type(type.get_calc_type())) {
        ret = aggregate_charsets_for_comparison(type, types_stack, param_num, type_ctx.get_coll_type());
      }
      if (OB_SUCC(ret)) {
        for (int64_t i = 0; OB_SUCC(ret) && i < param_num; ++i) {
          types_stack[i].set_calc_meta(type.get_calc_meta());
        }
      }
    }
  }
  return ret;
}

int ObExprField::deserialize(const char *buf, const int64_t data_len, int64_t &pos)
{
  int ret = OB_SUCCESS;
  need_cast_ = true;//defensive code
  if (OB_FAIL(ObVectorExprOperator::deserialize(buf, data_len, pos))) {
    LOG_WARN("deserialize in BASE class failed", K(ret));
  } else {
    OB_UNIS_DECODE(need_cast_);
  }
  return ret;
}

int ObExprField::serialize(char *buf, const int64_t buf_len, int64_t &pos) const
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(ObVectorExprOperator::serialize(buf, buf_len, pos))) {
    LOG_WARN("serialize in BASE class failed", K(ret));
  } else {
    OB_UNIS_ENCODE(need_cast_);
  }
  return ret;
}

int64_t ObExprField::get_serialize_size() const
{
  int64_t len = 0;
  BASE_ADD_LEN((ObExprField, ObVectorExprOperator));
  OB_UNIS_ADD_LEN(need_cast_);
  return len;
}

int ObExprField::cg_expr(ObExprCGCtx &, const ObRawExpr &, ObExpr &expr) const
{
  int ret = OB_SUCCESS;
  CK(expr.arg_cnt_ > 1);
  expr.eval_func_ = eval_field;
  return ret;
}

int ObExprField::eval_field(const ObExpr &expr, ObEvalCtx &ctx, ObDatum &expr_datum)
{
  int ret = OB_SUCCESS;
  ObDatum *first = NULL;
  if (OB_FAIL(expr.args_[0]->eval(ctx, first))) {
    LOG_WARN("evaluate parameter failed", K(ret));
  } else {
    expr_datum.set_int(0);
    // 0 == field(NULL, ...);
    if (!first->is_null()) {
      for (int64_t pos = 1; OB_SUCC(ret) && pos < expr.arg_cnt_; pos++) {
        ObDatum *d = NULL;
        if (OB_FAIL(expr.args_[pos]->eval(ctx, d))) {
          LOG_WARN("evaluate parameter failed", K(ret));
        } else if (0 == expr.args_[0]->basic_funcs_->null_first_cmp_(*first, *d)) {
          expr_datum.set_int(pos);
          break;
        }
      }
    }
  }
  return ret;
}

} // namespace sql
} // namespace oceanbase
