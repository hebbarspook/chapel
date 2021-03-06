/*
 * Copyright 2004-2018 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "InitNormalize.h"

#include "ForallStmt.h"
#include "initializerRules.h"
#include "stmt.h"

static bool mightBeSyncSingleExpr(DefExpr* field);

static bool isAssignment(CallExpr* callExpr);
static bool isSimpleAssignment(CallExpr* callExpr);
static bool isCompoundAssignment(CallExpr* callExpr);

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

InitNormalize::InitNormalize(FnSymbol* fn) {
  mFn            = fn;
  mCurrField     = firstField(fn);
  mPhase         = startPhase(fn);
  mBlockType     = cBlockNormal;
  mPrevBlockType = cBlockNormal;
}

InitNormalize::InitNormalize(BlockStmt* block, const InitNormalize& curr) {
  mFn            = curr.mFn;
  mCurrField     = curr.mCurrField;
  mPhase         = curr.mPhase;

  if (CallExpr* blockInfo = block->blockInfoGet()) {
    if        (blockInfo->isPrimitive(PRIM_BLOCK_BEGIN)       == true ||
               blockInfo->isPrimitive(PRIM_BLOCK_BEGIN_ON)    == true) {
      mBlockType = cBlockBegin;

    } else if (blockInfo->isPrimitive(PRIM_BLOCK_COBEGIN)     == true) {
      // Lydia NOTE 2017/07/26: If PRIM_BLOCK_COBEGIN_ONs are ever made, we
      // should match against them here
      mBlockType = cBlockCobegin;

    } else if (blockInfo->isPrimitive(PRIM_BLOCK_COFORALL)    == true ||
               blockInfo->isPrimitive(PRIM_BLOCK_COFORALL_ON) == true) {
      mBlockType = cBlockCoforall;

    } else if (blockInfo->isPrimitive(PRIM_BLOCK_ON) == true) {
      mBlockType = cBlockOn;

    } else {
      INT_ASSERT(false);
    }

  } else {
    mBlockType = curr.mBlockType;
  }

  if (mBlockType != curr.mBlockType) {
    mPrevBlockType = curr.mBlockType;
  } else {
    mPrevBlockType = curr.mPrevBlockType;
  }
}

InitNormalize::InitNormalize(CondStmt* cond, const InitNormalize& curr) {
  mFn            = curr.mFn;
  mCurrField     = curr.mCurrField;
  mPhase         = curr.mPhase;
  mBlockType     = cBlockCond;

  if (mBlockType != curr.mBlockType) {
    mPrevBlockType = curr.mBlockType;
  } else {
    mPrevBlockType = curr.mPrevBlockType;
  }
}

InitNormalize::InitNormalize(LoopStmt* loop, const InitNormalize& curr) {
  mFn            = curr.mFn;
  mCurrField     = curr.mCurrField;
  mPhase         = curr.mPhase;
  mBlockType     = cBlockLoop;

  if (mBlockType != curr.mBlockType) {
    mPrevBlockType = curr.mBlockType;

  } else {
    mPrevBlockType = curr.mPrevBlockType;
  }
}

InitNormalize::InitNormalize(ForallStmt* loop, const InitNormalize& curr) {
  mFn            = curr.mFn;
  mCurrField     = curr.mCurrField;
  mPhase         = curr.mPhase;
  mBlockType     = cBlockForall;

  if (mBlockType != curr.mBlockType) {
    mPrevBlockType = curr.mBlockType;

  } else {
    mPrevBlockType = curr.mPrevBlockType;
  }
}

void InitNormalize::merge(const InitNormalize& fork) {
  mCurrField = fork.mCurrField;
  mPhase     = fork.mPhase;
}

AggregateType* InitNormalize::type() const {
  return mFn != NULL ? toAggregateType(mFn->_this->type) : NULL;
}

FnSymbol* InitNormalize::theFn() const {
  return mFn;
}

InitNormalize::InitPhase InitNormalize::currPhase() const {
  return mPhase;
}

bool InitNormalize::isPhase0() const {
  return mPhase == cPhase0;
}

bool InitNormalize::isPhase1() const {
  return mPhase == cPhase1;
}

bool InitNormalize::isPhase2() const {
  return mPhase == cPhase2;
}

DefExpr* InitNormalize::currField() const {
  return mCurrField;
}

bool InitNormalize::isFieldReinitialized(DefExpr* field) const {
  AggregateType* at     = toAggregateType(mFn->_this->type);
  Expr*          ptr    = at->fields.head;
  bool           retval = false;

  while (ptr != NULL && ptr != mCurrField && retval == false) {
    if (field == ptr) {
      retval = true;
    } else {
      ptr    = ptr->next;
    }
  }

  INT_ASSERT(ptr != NULL);

  return retval;
}

bool InitNormalize::inLoopBody() const {
  return mBlockType == cBlockLoop;
}

bool InitNormalize::inCondStmt() const {
  return mBlockType == cBlockCond;
}

bool InitNormalize::inParallelStmt() const {
  return mBlockType == cBlockBegin   ||
         mBlockType == cBlockCobegin  ;
}

bool InitNormalize::inCoforall() const {
  return mBlockType == cBlockCoforall;
}

bool InitNormalize::inForall() const {
  return mBlockType == cBlockForall;
}

bool InitNormalize::inOn() const {
  return mBlockType == cBlockOn;
}

bool InitNormalize::inOnInLoopBody() const {
  return inOn() && mPrevBlockType == cBlockLoop;
}

bool InitNormalize::inOnInCondStmt() const {
  return inOn() && mPrevBlockType == cBlockCond;
}

bool InitNormalize::inOnInParallelStmt() const {
  return inOn() && (mPrevBlockType == cBlockBegin ||
                    mPrevBlockType == cBlockCobegin);
}

bool InitNormalize::inOnInCoforall() const {
  return inOn() && mPrevBlockType == cBlockCoforall;
}

bool InitNormalize::inOnInForall() const {
  return inOn() && mPrevBlockType == cBlockForall;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::completePhase1(CallExpr* initStmt) {
  if        (isThisInit(initStmt)  == true) {
    mCurrField = NULL;

  } else if (isSuperInit(initStmt) == true) {
    initializeFieldsBefore(initStmt);

  } else if (isInitDone(initStmt)  == true) {
    initializeFieldsBefore(initStmt);

  } else {
    INT_ASSERT(false);
  }

  mPhase = cPhase2;
}

void InitNormalize::initializeFieldsAtTail(BlockStmt* block) {
  if (mCurrField != NULL) {
    Expr* noop = new CallExpr(PRIM_NOOP);

    block->insertAtTail(noop);

    initializeFieldsBefore(noop);

    noop->remove();
  }
}

void InitNormalize::initializeFieldsBefore(Expr* insertBefore) {
  while (mCurrField != NULL) {
    DefExpr* field = mCurrField;

    if (isOuterField(field) == true) {
      // The outer field is a compiler generated field.  Handle it specially.
      makeOuterArg();

    } else {
      if (field->exprType == NULL && field->init == NULL) {
        USR_FATAL_CONT(insertBefore,
                       "can't omit initialization of field \"%s\", "
                       "no type or default value provided",
                       field->sym->name);

      } else if (field->sym->hasFlag(FLAG_PARAM)         == true ||
                 field->sym->hasFlag(FLAG_TYPE_VARIABLE) == true) {
        if        (field->exprType != NULL && field->init == NULL) {
          genericFieldInitTypeWoutInit (insertBefore, field);

        } else if (field->exprType != NULL && field->init != NULL) {
          genericFieldInitTypeWithInit (insertBefore,
                                        field,
                                        field->init->copy());

        } else if (field->exprType == NULL && field->init != NULL) {
          genericFieldInitTypeInference(insertBefore,
                                        field,
                                        field->init->copy());

        } else {
          INT_ASSERT(false);
        }

      } else if (field->init != NULL) {
        Expr* initCopy    = field->init->copy();
        bool  isTypeKnown = mCurrField->sym->type != dtUnknown;

        if (isTypeKnown == true) {
          fieldInitTypeWithInit (insertBefore, field, initCopy);

        } else if (field->exprType != NULL) {
          fieldInitTypeWithInit (insertBefore, field, initCopy);

        } else {
          fieldInitTypeInference(insertBefore, field, initCopy);
        }

      } else {
        fieldInitTypeWoutInit(insertBefore, field);
      }
    }

    mCurrField = toDefExpr(mCurrField->next);
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::genericFieldInitTypeWoutInit(Expr*    insertBefore,
                                                 DefExpr* field) const {
  INT_ASSERT(field->sym->hasFlag(FLAG_PARAM));

  SET_LINENO(insertBefore);

  Type* type = field->sym->type;

  if (isPrimitiveScalar(type) == true ||
      isNonGenericClass(type) == true) {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpExpr  = new CallExpr("_defaultOf", type->symbol);
    CallExpr*  tmpInit  = new CallExpr(PRIM_MOVE, tmp, tmpExpr);

    tmp->addFlag(FLAG_PARAM);

    Symbol*    name     = new_CStringSymbol(field->sym->name);
    Symbol*    _this    = mFn->_this;
    CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);

  } else if (isNonGenericRecordWithInitializers(type) == true) {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpInit  = new CallExpr("init", gMethodToken, tmp);

    tmp->addFlag(FLAG_PARAM);

    Symbol*    name     = new_CStringSymbol(field->sym->name);
    Symbol*    _this    = mFn->_this;
    CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);

  } else {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpExpr  = new CallExpr(PRIM_INIT, field->exprType->copy());
    CallExpr*  tmpInit  = new CallExpr(PRIM_MOVE, tmp, tmpExpr);

    tmp->addFlag(FLAG_PARAM);

    Symbol*    _this    = mFn->_this;
    Symbol*    name     = new_CStringSymbol(field->sym->name);
    CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

    if (isFieldAccessible(tmpExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(tmpExpr);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

static bool isNewExpr(Expr* expr);

void InitNormalize::genericFieldInitTypeWithInit(Expr*    insertBefore,
                                                 DefExpr* field,
                                                 Expr*    initExpr) const {
  INT_ASSERT(field->sym->hasFlag(FLAG_PARAM));

  SET_LINENO(insertBefore);

  CallExpr* cast     = createCast(initExpr, field->exprType->copy());

  Symbol*   name     = new_CStringSymbol(field->sym->name);
  Symbol*   _this    = mFn->_this;

  CallExpr* fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, cast);

  if (isFieldAccessible(initExpr) == false) {
    INT_ASSERT(false);
  }

  updateFieldsMember(initExpr);

  insertBefore->insertBefore(fieldSet);
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::genericFieldInitTypeInference(Expr*    insertBefore,
                                                  DefExpr* field,
                                                  Expr*    initExpr) const {
  bool isParam   = field->sym->hasFlag(FLAG_PARAM);
  bool isTypeVar = field->sym->hasFlag(FLAG_TYPE_VARIABLE);

  SET_LINENO(insertBefore);

  // e.g.
  //   var x = <immediate>;
  //   var y = <identifier>;
  if (SymExpr* initSym = toSymExpr(initExpr)) {
    Type* type = initSym->symbol()->type;

    if (isTypeVar == true) {
      VarSymbol* tmp = NULL;

      if (type == dtAny) {
        tmp = newTemp("tmp");
      } else {
        tmp = newTemp("tmp", type);
      }

      DefExpr*  tmpDefn  = new DefExpr(tmp);
      CallExpr* tmpInit  = new CallExpr(PRIM_MOVE, tmp, initExpr);
      Symbol*   _this    = mFn->_this;
      Symbol*   name     = new_CStringSymbol(field->sym->name);
      CallExpr* fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

      tmp->addFlag(FLAG_TYPE_VARIABLE);

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);

    } else if (isPrimitiveScalar(type) == true) {
      VarSymbol* tmp      = newTemp("tmp", type);
      DefExpr*   tmpDefn  = new DefExpr(tmp);
      CallExpr*  tmpInit  = new CallExpr(PRIM_MOVE, tmp, initExpr);

      Symbol*    name     = new_CStringSymbol(field->sym->name);
      Symbol*    _this    = mFn->_this;
      CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

      if (isParam == true) {
        tmp->addFlag(FLAG_PARAM);
      }

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);

    } else {
      VarSymbol* tmp      = newTemp("tmp");
      DefExpr*   tmpDefn  = new DefExpr(tmp);
      CallExpr*  tmpInit  = new CallExpr(PRIM_INIT_VAR, tmp, initExpr);

      Symbol*    _this    = mFn->_this;
      Symbol*    name     = new_CStringSymbol(field->sym->name);
      CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

      if (isParam == true) {
        tmp->addFlag(FLAG_PARAM);
      }

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);
    }

  // e.g.
  //   var x = f(...);
  //   var y = new MyRecord(...);
  } else if (CallExpr* initCall = toCallExpr(initExpr)) {
    if ((isParam || isTypeVar) && initCall->isPrimitive(PRIM_NEW) == true) {
      if (isTypeVar == true) {
        USR_FATAL(initExpr,
                  "Cannot initialize type field '%s' with 'new' expression",
                  field->sym->name);

      } else {
        INT_ASSERT(isParam == true);

        USR_FATAL(initExpr,
                  "Cannot initialize param field '%s' with 'new' expression",
                  field->sym->name);
      }

    } else if (isTypeVar == true) {
      VarSymbol* tmp      = newTemp("tmp");
      DefExpr*   tmpDefn  = new DefExpr(tmp);
      CallExpr*  tmpInit  = new CallExpr(PRIM_MOVE, tmp, initExpr);

      tmp->addFlag(FLAG_TYPE_VARIABLE);

      Symbol*    _this    = mFn->_this;
      Symbol*    name     = new_CStringSymbol(field->sym->name);
      CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);

    } else {
      VarSymbol* tmp      = newTemp("tmp");
      DefExpr*   tmpDefn  = new DefExpr(tmp);
      CallExpr*  tmpInit  = new CallExpr(PRIM_INIT_VAR, tmp, initExpr);

      Symbol*    _this    = mFn->_this;
      Symbol*    name     = new_CStringSymbol(field->sym->name);
      CallExpr*  fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, tmp);

      if (isParam == true) {
        tmp->addFlag(FLAG_PARAM);
      }

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);
    }

  } else if (isUnresolvedSymExpr(initExpr)) {
    // Don't worry too much about it, resolution will handle this.
    Symbol*   _this    = mFn->_this;
    Symbol*   name     = new_CStringSymbol(field->sym->name);
    CallExpr* fieldSet = new CallExpr(PRIM_INIT_FIELD, _this, name, initExpr);

    insertBefore->insertBefore(fieldSet);

  } else {
    INT_ASSERT(false);
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::fieldInitTypeWoutInit(Expr*    insertBefore,
                                          DefExpr* field) const {
  SET_LINENO(insertBefore);

  Type* type = field->sym->type;

  if (isPrimitiveScalar(type) == true ||
      isNonGenericClass(type) == true) {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpExpr  = new CallExpr("_defaultOf", type->symbol);
    CallExpr*  tmpInit  = new CallExpr(PRIM_MOVE, tmp, tmpExpr);

    Symbol*    name     = new_CStringSymbol(field->sym->name);
    Symbol*    _this    = mFn->_this;
    CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);

  } else if (isNonGenericRecordWithInitializers(type) == true) {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpInit  = new CallExpr("init", gMethodToken, tmp);

    Symbol*    name     = new_CStringSymbol(field->sym->name);
    Symbol*    _this    = mFn->_this;
    CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);

  } else {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpExpr  = new CallExpr(PRIM_INIT, field->exprType->copy());
    CallExpr*  tmpInit  = new CallExpr(PRIM_MOVE, tmp, tmpExpr);

    Symbol*    _this    = mFn->_this;
    Symbol*    name     = new_CStringSymbol(field->sym->name);
    CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    if (isFieldAccessible(tmpExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(tmpExpr);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::fieldInitTypeWithInit(Expr*    insertBefore,
                                          DefExpr* field,
                                          Expr*    initExpr) const {
  SET_LINENO(insertBefore);

  Type* type = field->sym->type;

  if (isPrimitiveScalar(type) == true ||
      isNonGenericClass(type) == true) {
    VarSymbol* tmp      = newTemp("tmp", type);
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpInit  = new CallExpr("=", tmp, initExpr);

    Symbol*    name     = new_CStringSymbol(field->sym->name);
    Symbol*    _this    = mFn->_this;
    CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    if (isFieldAccessible(initExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(initExpr);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);

  } else if (isNonGenericRecordWithInitializers(type) == true) {
    if (isNewExpr(initExpr) == true) {
      VarSymbol* tmp      = newTemp("tmp", type);
      DefExpr*   tmpDefn  = new DefExpr(tmp);

      Expr*      arg      = toCallExpr(initExpr)->get(1)->remove();
      CallExpr*  argExpr  = toCallExpr(arg);

      Symbol*    name     = new_CStringSymbol(field->sym->name);
      Symbol*    _this    = mFn->_this;
      CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

      insertBefore->insertBefore(tmpDefn);

      // This call must be in tree before extending argExpr
      insertBefore->insertBefore(argExpr);

      // Convert it in to a use of the init method
      argExpr->baseExpr->replace(new UnresolvedSymExpr("init"));

      // Add _mt and _this (insert at head in reverse order)
      argExpr->insertAtHead(tmp);
      argExpr->insertAtHead(gMethodToken);

      if (isFieldAccessible(argExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(argExpr);

      insertBefore->insertBefore(fieldSet);

    } else {
      VarSymbol* tmp      = newTemp("tmp", type);
      DefExpr*   tmpDefn  = new DefExpr(tmp);
      CallExpr*  tmpInit  = new CallExpr("init", gMethodToken, tmp, initExpr);

      Symbol*    name     = new_CStringSymbol(field->sym->name);
      Symbol*    _this    = mFn->_this;
      CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);
    }

  } else if (theFn()->hasFlag(FLAG_COMPILER_GENERATED) == true &&
             field->init                               == NULL &&
             mightBeSyncSingleExpr(field)              == true) {
    // The type of the field depends on something that hasn't been determined
    // yet.  It is entirely possible that the type will end up as a sync or
    // single and so we need to flag this field initialization for resolution
    // to handle
    Symbol*   _this    = mFn->_this;
    Symbol*   name     = new_CStringSymbol(field->sym->name);
    CallExpr* fieldSet = new CallExpr(PRIM_INIT_MAYBE_SYNC_SINGLE_FIELD,
                                      _this,
                                      name,
                                      initExpr);

    if (isFieldAccessible(initExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(initExpr);

    insertBefore->insertBefore(fieldSet);

  } else if (field->exprType == NULL) {
    VarSymbol* tmp       = newTemp("tmp", type);
    DefExpr*   tmpDefn   = new DefExpr(tmp);

    // Set the value for TMP
    CallExpr*  tmpAssign = new CallExpr("=", tmp,  initExpr);

    Symbol*    _this     = mFn->_this;
    Symbol*    name      = new_CStringSymbol(field->sym->name);
    CallExpr*  fieldSet  = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    if (isFieldAccessible(initExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(initExpr);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpAssign);
    insertBefore->insertBefore(fieldSet);

  } else {
    VarSymbol* tmp       = newTemp("tmp", type);
    DefExpr*   tmpDefn   = new DefExpr(tmp);

    // Applies a type to TMP
    CallExpr*  tmpExpr   = new CallExpr(PRIM_INIT, field->exprType->copy());
    CallExpr*  tmpMove   = new CallExpr(PRIM_MOVE, tmp,  tmpExpr);

    // Set the value for TMP
    CallExpr*  tmpAssign = new CallExpr("=",       tmp,  initExpr);

    Symbol*    _this     = mFn->_this;
    Symbol*    name      = new_CStringSymbol(field->sym->name);
    CallExpr*  fieldSet  = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    if (isFieldAccessible(tmpExpr) == false) {
      INT_ASSERT(false);
    }

    if (isFieldAccessible(initExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(tmpExpr);

    updateFieldsMember(initExpr);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpMove);
    insertBefore->insertBefore(tmpAssign);
    insertBefore->insertBefore(fieldSet);
  }
}

static bool isNewExpr(Expr* expr) {
  bool retval = false;

  if (CallExpr* callExpr = toCallExpr(expr)) {
    retval = callExpr->isPrimitive(PRIM_NEW);
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::fieldInitTypeInference(Expr*    insertBefore,
                                           DefExpr* field,
                                           Expr*    initExpr) const {
  SET_LINENO(insertBefore);

  // e.g.
  //   var x = <immediate>;
  //   var y = <identifier>;
  if (SymExpr* initSym = toSymExpr(initExpr)) {
    Type* type = initSym->symbol()->type;

    if (isPrimitiveScalar(type) == true) {
      VarSymbol*  tmp       = newTemp("tmp", type);
      DefExpr*    tmpDefn   = new DefExpr(tmp);
      CallExpr*   tmpInit   = new CallExpr(PRIM_MOVE, tmp, initExpr);

      Symbol*     name      = new_CStringSymbol(field->sym->name);
      Symbol*     _this     = mFn->_this;
      CallExpr*   fieldSet  = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);

    } else {
      VarSymbol* tmp      = newTemp("tmp");
      DefExpr*   tmpDefn  = new DefExpr(tmp);
      CallExpr*  tmpInit  = new CallExpr(PRIM_INIT_VAR, tmp, initExpr);

      Symbol*    _this    = mFn->_this;
      Symbol*    name     = new_CStringSymbol(field->sym->name);
      CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

      if (isFieldAccessible(initExpr) == false) {
        INT_ASSERT(false);
      }

      updateFieldsMember(initExpr);

      insertBefore->insertBefore(tmpDefn);
      insertBefore->insertBefore(tmpInit);
      insertBefore->insertBefore(fieldSet);
    }

  // e.g.
  //   var x = f(...);
  //   var y = new MyRecord(...);
  } else if (isCallExpr(initExpr) == true) {
    VarSymbol* tmp      = newTemp("tmp");
    DefExpr*   tmpDefn  = new DefExpr(tmp);
    CallExpr*  tmpInit  = new CallExpr(PRIM_INIT_VAR, tmp, initExpr);

    Symbol*    _this    = mFn->_this;
    Symbol*    name     = new_CStringSymbol(field->sym->name);
    CallExpr*  fieldSet = new CallExpr(PRIM_SET_MEMBER, _this, name, tmp);

    if (isFieldAccessible(initExpr) == false) {
      INT_ASSERT(false);
    }

    updateFieldsMember(initExpr);

    insertBefore->insertBefore(tmpDefn);
    insertBefore->insertBefore(tmpInit);
    insertBefore->insertBefore(fieldSet);

  } else {
    INT_ASSERT(false);
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

bool InitNormalize::isFieldAccessible(Expr* expr) const {
  AggregateType* at      = type();
  bool           initNew = hasInitDone(mFn->body);
  bool           retval  = true;

  if (SymExpr* symExpr = toSymExpr(expr)) {
    Symbol* sym = symExpr->symbol();

    if (sym->isImmediate() == true) {
      retval = true;

    } else if (DefExpr* field = at->toLocalField(symExpr)) {
      if (isFieldInitialized(field) == true) {
        retval = true;

      } else {
        USR_FATAL(expr,
                  "'%s' used before defined (first used here)",
                  field->sym->name);
      }

    } else if (DefExpr* field = at->toSuperField(symExpr)) {
      if (initNew == true || isPhase2() == true) {
        retval = true;

      } else {
        USR_FATAL(expr,
                  "Cannot access parent field '%s' during phase 1",
                  field->sym->name);
      }

    } else {
      retval = true;
    }

  } else if (CallExpr* callExpr = toCallExpr(expr)) {
    if (DefExpr* field = at->toLocalField(callExpr)) {
      if (isFieldInitialized(field) == true) {
        retval = true;

      } else {
        USR_FATAL(expr,
                  "'%s' used before defined (first used here)",
                  field->sym->name);
      }

    } else if (DefExpr* field = at->toSuperField(callExpr)) {
      if (initNew == true || isPhase2() == true) {
        retval = true;

      } else {
        USR_FATAL(expr,
                  "Cannot access parent field '%s' during phase 1",
                  field->sym->name);
      }

    } else {
      for_actuals(actual, callExpr) {
        if (isFieldAccessible(actual) == false) {
          retval = false;
          break;
        }
      }
    }

  } else if (isNamedExpr(expr) == true) {
    retval = true;

  } else if (isUnresolvedSymExpr(expr) == true) {
    // Resolution will handle this case better.
    retval = true;

  } else {
    INT_ASSERT(false);
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

void InitNormalize::updateFieldsMember(Expr* expr) const {
  if (SymExpr* symExpr = toSymExpr(expr)) {
    Symbol* sym = symExpr->symbol();

    if (DefExpr* field = toLocalField(symExpr)) {
      if (isFieldInitialized(field) == true) {
        SymExpr* _this = new SymExpr(mFn->_this);
        SymExpr* field = new SymExpr(new_CStringSymbol(sym->name));

        symExpr->replace(new CallExpr(PRIM_GET_MEMBER, _this, field));

      } else {
        USR_FATAL(expr,
                  "'%s' used before defined (first used here)",
                  field->sym->name);
      }

    } else if (DefExpr* field = toSuperField(symExpr)) {
      bool initNew = hasInitDone(mFn->body);

      if (initNew == true) {
        SymExpr* _this = new SymExpr(mFn->_this);
        SymExpr* field = new SymExpr(new_CStringSymbol(sym->name));

        symExpr->replace(new CallExpr(PRIM_GET_MEMBER, _this, field));

      } else {
        USR_FATAL(expr,
                  "Cannot access parent field '%s' during phase 1",
                  field->sym->name);
      }
    }

  } else if (CallExpr* callExpr = toCallExpr(expr)) {
    if (isFieldAccess(callExpr) == false) {
      handleInsertedMethodCall(callExpr);

      for_actuals(actual, callExpr) {
        updateFieldsMember(actual);
      }
    }

  } else if (isNamedExpr(expr) == true) {

  } else if (isUnresolvedSymExpr(expr) == true) {

  } else {
    INT_ASSERT(false);
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

bool InitNormalize::isFieldAccess(CallExpr* callExpr) const {
  bool retval = false;

  if (callExpr->isNamed(".") == true) {
    if (SymExpr* lhs = toSymExpr(callExpr->get(1))) {
      if (ArgSymbol* arg = toArgSymbol(lhs->symbol())) {
        retval = arg->hasFlag(FLAG_ARG_THIS);
      }
    }
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
* If the call is to a method on our type, we need to transform it into        *
*  something we'll recognize as a method call.                                *
*                                                                             *
* This is necessary so that later we can see the if and loop expr "method     *
* calls" written for field initialization and let them work properly.         *
*                                                                             *
************************************** | *************************************/

void InitNormalize::handleInsertedMethodCall(CallExpr* call) const {
  if (UnresolvedSymExpr* us = toUnresolvedSymExpr(call->baseExpr)) {
    bool alreadyMethod = false;
    if (call->numActuals() > 0) {
      SymExpr* firstArg = toSymExpr(call->get(1));
      if (firstArg && firstArg->symbol() == gMethodToken) {
        alreadyMethod = true;
      }
    }

    if (alreadyMethod == false) {
      AggregateType* at      = type();
      bool           matches = false;

      // Note: doesn't handle inherited methods.
      forv_Vec(FnSymbol, fn, at->methods) {
        if (strcmp(us->unresolved, fn->name) == 0) {
          matches = true;
          break;
        }
      }

      if (matches) {
        CallExpr* replacement = new CallExpr(astrSdot, mFn->_this);
        replacement->insertAtTail(us);
        call->baseExpr = replacement;
      }
    }
  }
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

DefExpr* InitNormalize::toLocalField(SymExpr* expr) const {
  return type()->toLocalField(expr);
}

DefExpr* InitNormalize::toLocalField(CallExpr* expr) const {
  return type()->toLocalField(expr);
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

DefExpr* InitNormalize::toSuperField(SymExpr* expr) const {
  return type()->toSuperField(expr);
}

DefExpr* InitNormalize::toSuperField(AggregateType* at,
                                     const char*    name) const {
  forv_Vec(Type, t, at->dispatchParents) {
    if (AggregateType* pt = toAggregateType(t)) {
      if (DefExpr* field = pt->toLocalField(name)) {
        return field;
      }
    }
  }

  return NULL;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

bool InitNormalize::isFieldInitialized(const DefExpr* field) const {
  const DefExpr* ptr    = mCurrField;
  bool           retval = true;

  while (ptr != NULL && retval == true) {
    if (ptr == field) {
      retval = false;
    } else {
      ptr = toConstDefExpr(ptr->next);
    }
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

InitNormalize::InitPhase InitNormalize::startPhase(FnSymbol*  fn)    const {
  return startPhase(fn->body);
}

InitNormalize::InitPhase InitNormalize::startPhase(BlockStmt* block) const {
  Expr*     stmt   = block->body.head;
  InitPhase retval = cPhase2;

  while (stmt != NULL && retval == cPhase2) {
    if (isDefExpr(stmt) == true) {
      stmt = stmt->next;

    } else if (CallExpr* callExpr = toCallExpr(stmt)) {
      if        (isThisInit(callExpr)  == true) {
        retval = cPhase0;

      } else if (isSuperInit(callExpr) == true) {
        retval = cPhase1;

      } else if (isInitDone(callExpr)  == true) {
        retval = cPhase1;

      } else {
        stmt   = stmt->next;
      }

    } else if (CondStmt* cond = toCondStmt(stmt)) {
      if (cond->elseStmt == NULL) {
        InitPhase thenPhase = startPhase(cond->thenStmt);

        if (thenPhase != cPhase2) {
          retval = thenPhase;
        } else {
          stmt   = stmt->next;
        }

      } else {
        InitPhase thenPhase = startPhase(cond->thenStmt);
        InitPhase elsePhase = startPhase(cond->elseStmt);

        if        (thenPhase == cPhase0 || elsePhase == cPhase0) {
          retval = cPhase0;

        } else if (thenPhase == cPhase1 || elsePhase == cPhase1) {
          retval = cPhase1;

        } else {
          stmt   = stmt->next;
        }
      }

    } else if (BlockStmt* block = toBlockStmt(stmt)) {
      InitPhase phase = startPhase(block);

      if (phase != cPhase2) {
        retval = phase;
      } else {
        stmt   = stmt->next;
      }

    } else if (ForallStmt* block = toForallStmt(stmt)) {
      InitPhase phase = startPhase(block->loopBody());

      if (phase != cPhase2) {
        retval = phase;
      } else {
        stmt   = stmt->next;
      }

    } else {
      stmt = stmt->next;
    }
  }

  return retval;
}

void InitNormalize::checkPhase(BlockStmt* block) {
  if (mPhase == cPhase0) {
    InitPhase newPhase = startPhase(block);

    if (newPhase == cPhase1) {
      mPhase = newPhase;
    }
  }
}

DefExpr* InitNormalize::firstField(FnSymbol* fn) const {
  AggregateType* at     = toAggregateType(fn->_this->type);
  DefExpr*       retval = toDefExpr(at->fields.head);

  // Skip the pseudo-field "super"
  if (at->isClass() == true) {

    if (at->isGeneric() == true) {
      AggregateType* pt = toAggregateType(retval->sym->type);

      INT_ASSERT(pt);

      if (pt->isGeneric() == true) {
        // If the super type is generic, label it so that we can
        // handle that appropriately during initializer resolution
        retval->sym->addFlag(FLAG_DELAY_GENERIC_EXPANSION);
      }
    }

    retval = toDefExpr(retval->next);
  }

  return retval;
}

bool InitNormalize::isOuterField(DefExpr* field) const {
  return type()->outer == field->sym;
}

void InitNormalize::makeOuterArg() {
  AggregateType* at        = type();
  Type*          outerType = at->outer->type;

  outerType->methods.add(mFn);

  mFn->_outer = new ArgSymbol(INTENT_BLANK, "outer", outerType);

  mFn->_outer->addFlag(FLAG_GENERIC);

  mFn->_this->defPoint->insertAfter(new DefExpr(mFn->_outer));

  mFn->insertAtHead(new CallExpr(PRIM_SET_MEMBER,
                                 mFn->_this,
                                 new_CStringSymbol("outer"),
                                 mFn->_outer));
}

Expr* InitNormalize::fieldInitFromInitStmt(DefExpr*  field,
                                           CallExpr* initStmt) {
  Expr* retval = NULL;

  if (field != mCurrField) {
    INT_ASSERT(isFieldReinitialized(field) == false);

    while (field != mCurrField) {
      fieldInitFromField(initStmt);

      mCurrField = toDefExpr(mCurrField->next);
    }
  }

  // Now that omitted fields have been handled, see if RHS is OK
  if (fieldUsedBeforeInitialized(initStmt) == true) {
    USR_FATAL(initStmt, "Field used before it is initialized");
  }

  retval     = fieldInitFromStmt(initStmt, field);
  mCurrField = toDefExpr(mCurrField->next);

  return retval;
}


Expr* InitNormalize::fieldInitFromStmt(CallExpr* stmt, DefExpr* field) const {
  Expr* insertBefore = stmt;
  Expr* initExpr     = stmt->get(2)->remove();
  Expr* retval       = stmt->next;

  // Initialize the field using the RHS of the source stmt
  if (field->sym->hasFlag(FLAG_PARAM)) {
    if (field->exprType != NULL) {
      genericFieldInitTypeWithInit(insertBefore, field, initExpr);

    } else {
      genericFieldInitTypeInference(insertBefore, field, initExpr);

    }

  } else if (field->sym->hasFlag(FLAG_TYPE_VARIABLE)) {
    genericFieldInitTypeInference(insertBefore, field, initExpr);

  } else if (field->exprType == NULL && field->init == NULL) {
    // Field is a generic var or const
    genericFieldInitTypeInference(insertBefore, field, initExpr);

  } else if (field->exprType != NULL) {
    // Field is concrete
    fieldInitTypeWithInit (insertBefore, field, initExpr);

  } else {
    // Field is concrete
    fieldInitTypeInference(insertBefore, field, initExpr);
  }

  // Remove the (degenerate) source version of the field assignment
  stmt->remove();

  return retval;
}

void InitNormalize::fieldInitFromField(Expr* insertBefore) {
  DefExpr* field = mCurrField;

  if        (field->exprType == NULL && field->init == NULL) {
    USR_FATAL_CONT(insertBefore,
                   "can't omit initialization of field \"%s\", "
                   "no type or default value provided",
                   field->sym->name);

  } else if (field->sym->hasFlag(FLAG_PARAM) ||
             field->sym->hasFlag(FLAG_TYPE_VARIABLE)) {

    if (field->exprType != NULL && field->init == NULL) {
      genericFieldInitTypeWoutInit (insertBefore, field);

    } else if ((field->exprType != NULL  && field->init != NULL)) {
      genericFieldInitTypeWithInit (insertBefore, field, field->init->copy());

    } else if (field->exprType == NULL && field->init != NULL) {
      genericFieldInitTypeInference(insertBefore, field, field->init->copy());

    } else {
      INT_ASSERT(false);
    }

  } else if (field->exprType != NULL && field->init == NULL) {
    fieldInitTypeWoutInit (insertBefore, field);

  } else if (field->exprType != NULL && field->init != NULL) {
    Expr* initCopy = field->init->copy();

    fieldInitTypeWithInit (insertBefore, field, initCopy);

  } else if (field->exprType == NULL && field->init != NULL) {
    Expr* initCopy = field->init->copy();

    fieldInitTypeInference(insertBefore, field, initCopy);

  } else {
    INT_ASSERT(false);
  }
}

bool InitNormalize::fieldUsedBeforeInitialized(Expr* expr) const {
  bool retval = false;

  if (DefExpr* defExpr = toDefExpr(expr)) {
    if (defExpr->init != NULL) {
      retval = fieldUsedBeforeInitialized(defExpr->init);
    }

  } else if (CallExpr* callExpr = toCallExpr(expr)) {
    retval = fieldUsedBeforeInitialized(callExpr);
  }

  return retval;
}

bool InitNormalize::fieldUsedBeforeInitialized(CallExpr* callExpr) const {
  bool retval = false;

  if (isAssignment(callExpr) == true) {
    if (CallExpr* LHS = toCallExpr(callExpr->get(1))) {
      if (isFieldAccess(LHS)) {
        // Want to watch out for array-like accesses that appear as field
        // accesses: x[1] = 1;
        retval = LHS->square;
      } else {
        // Look for expressions like: x.foo = 1;
        retval = fieldUsedBeforeInitialized(callExpr->get(1));
      }
    }
    retval = retval || fieldUsedBeforeInitialized(callExpr->get(2));

  } else if (DefExpr* field = type()->toLocalField(callExpr)) {
    retval = isFieldInitialized(field) == true ? false : true;

  } else {
    // Need to check the baseExpr in cases like:
    //   myField.set(1)
    // Because the baseExpr is a field access of 'this.myField'
    retval = fieldUsedBeforeInitialized(callExpr->baseExpr);
    if (retval == false) {
      for_actuals(actual, callExpr) {
        if (fieldUsedBeforeInitialized(actual) == true) {
          retval = true;
          break;
        }
      }
    }
  }

  return retval;
}

void InitNormalize::describe(int offset) const {
  char pad[512];

  for (int i = 0; i < offset; i++) {
    pad[i] = ' ';
  }

  pad[offset] = '\0';

  printf("%s#<InitNormalize\n", pad);

  printf("%s  Phase: %s\n", pad, phaseToString(mPhase));

  printf("%s  Block: ",       pad);

  switch (mBlockType) {
    case cBlockNormal:
      printf("normal\n");
      break;

    case cBlockCond:
      printf("cond\n");
      break;

    case cBlockLoop:
      printf("loop\n");
      break;

    case cBlockBegin:
      printf("begin\n");
      break;

    case cBlockCobegin:
      printf("cobegin\n");
      break;

    case cBlockCoforall:
      printf("coforall\n");
      break;

    case cBlockForall:
      printf("forall\n");
      break;

    case cBlockOn:
      printf("on\n");
      break;
  }

  printf("%s>\n", pad);
}

const char*
InitNormalize::phaseToString(InitNormalize::InitPhase phase) const {
  const char* retval = "?";

  switch (phase) {
    case cPhase0:
      retval = "Phase0";
      break;

    case cPhase1:
      retval = "Phase1";
      break;

    case cPhase2:
      retval = "Phase2";
      break;
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

// The type of the field is not yet determined either due to being entirely a
// type alias, or due to being a call to a function that returns a type.
// Therefore, we must be cautious and marking this field initialization as
// potentially a sync or single, so that when we know its type at resolution,
// we can respond appropriately.
static bool mightBeSyncSingleExpr(DefExpr* field) {
  bool retval = false;

  if (SymExpr* typeSym = toSymExpr(field->exprType)) {
    if (typeSym->symbol()->hasFlag(FLAG_TYPE_VARIABLE)) {
      retval = true;
    }

  } else if (CallExpr* typeCall = toCallExpr(field->exprType)) {
    /*if (typeCall->isPrimitive(PRIM_QUERY_TYPE_FIELD)) { // might be necessary
      retval = true;
    } else */

    if (typeCall->isPrimitive() == false) {
      // The call is not a known primitive.
      // We have to assume that it is a type function being called,
      // and type functions could return a sync or single type.
      retval = true;
    }
  }

  return retval;
}

static bool isAssignment(CallExpr* callExpr) {
  bool retval = false;

  if (isSimpleAssignment(callExpr)   == true ||
      isCompoundAssignment(callExpr) == true) {
    retval = true;
  }

  return retval;
}

static bool isSimpleAssignment(CallExpr* callExpr) {
  bool retval = false;

  if (callExpr->isNamedAstr(astrSequals) == true) {
    retval = true;
  }

  return retval;
}

static bool isCompoundAssignment(CallExpr* callExpr) {
  bool retval = false;

  if (callExpr->isNamed("+=")  == true ||
      callExpr->isNamed("-=")  == true ||
      callExpr->isNamed("*=")  == true ||
      callExpr->isNamed("/=")  == true ||
      callExpr->isNamed("**=") == true ||
      callExpr->isNamed("%=")  == true ||
      callExpr->isNamed("&=")  == true ||
      callExpr->isNamed("|=")  == true ||
      callExpr->isNamed("^=")  == true ||
      callExpr->isNamed("&&=") == true ||
      callExpr->isNamed("||=") == true ||
      callExpr->isNamed("<<=") == true ||
      callExpr->isNamed(">>=") == true) {
    retval = true;
  }

  return retval;
}
