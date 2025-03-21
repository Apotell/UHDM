// -*- c++ -*-

/*

 Copyright 2019-2022 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   SynthSubset.cpp
 * Author: alaindargelas
 *
 * Created on Feb 16, 2022, 9:03 PM
 */
#include <string.h>
#include <uhdm/ElaboratorListener.h>
#include <uhdm/ExprEval.h>
#include <uhdm/Serializer.h>
#include <uhdm/SynthSubset.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

#include <algorithm>

namespace uhdm {

SynthSubset::SynthSubset(Serializer* serializer,
                         std::set<const Any*>& nonSynthesizableObjects,
                         Design* des, bool reportErrors, bool allowFormal)
    : m_serializer(serializer),
      m_nonSynthesizableObjects(nonSynthesizableObjects),
      m_design(des),
      m_reportErrors(reportErrors),
      m_allowFormal(allowFormal) {
  constexpr std::string_view kDollar("$");
  for (auto s :
       {// "display",
        "write", "strobe", "monitor", "monitoron", "monitoroff", "displayb",
        "writeb", "strobeb", "monitorb", "displayo", "writeo", "strobeo",
        "monitoro", "displayh", "writeh", "strobeh", "monitorh", "fopen",
        "fclose", "frewind", "fflush", "fseek", "ftell", "fdisplay", "fwrite",
        "swrite", "fstrobe", "fmonitor", "fread", "fscanf", "fdisplayb",
        "fwriteb", "swriteb", "fstrobeb", "fmonitorb", "fdisplayo", "fwriteo",
        "swriteo", "fstrobeo", "fmonitoro", "fdisplayh", "fwriteh", "swriteh",
        "fstrobeh", "fmonitorh", "sscanf", "sdf_annotate", "sformat",
        // "cast",
        "assertkill", "assertoff", "asserton",
        // "bits",
        // "bitstoshortreal",
        "countones", "coverage_control", "coverage_merge", "coverage_save",
        // "dimensions",
        // "error",
        "exit",
        // "fatal",
        "fell", "get_coverage", "coverage_get", "coverage_get_max",
        // "high",
        //"increment",
        "info", "isunbounded", "isunknown",
        // "left",
        "load_coverage_db",
        // "low",
        "onehot", "past",
        // "readmemb",
        // "readmemh",
        //"right",
        "root", "rose", "sampled", "set_coverage_db_name",
        // "shortrealtobits",
        // "size",
        "stable",
        // "typename",
        // "typeof",
        "unit", "urandom", "srandom", "urandom_range", "set_randstate",
        "get_randstate", "dist_uniform", "dist_normal", "dist_exponential",
        "dist_poisson", "dist_chi_square", "dist_t", "dist_erlang",
        // "warning",
        // "writememb",
        // "writememh",
        "value$plusargs"}) {
    m_nonSynthSysCalls.emplace(std::move(std::string(kDollar).append(s)));
  }
}

void SynthSubset::report(std::ostream& out) {
  for (auto object : m_nonSynthesizableObjects) {
    VpiVisitor::visited_t visitedObjects;

    vpiHandle dh =
        object->getSerializer()->makeUhdmHandle(object->getUhdmType(), object);

    visit_object(dh, out, true);
    vpi_release_handle(dh);
  }
}

void SynthSubset::reportError(const Any* object) {
  const Any* tmp = object;
  while (tmp && tmp->getFile().empty()) {
    tmp = tmp->getParent();
  }
  if (tmp) object = tmp;
  if (m_reportErrors && !reportedParent(object)) {
    if (!object->getFile().empty()) {
      const std::string errMsg(object->getName());
      m_serializer->getErrorHandler()(ErrorType::UHDM_NON_SYNTHESIZABLE, errMsg,
                                      object, nullptr);
    }
  }
  mark(object);
}

void SynthSubset::leaveAny(const Any* object, vpiHandle handle) {
  switch (object->getUhdmType()) {
    case UhdmType::FinalStmt:
    case UhdmType::DelayControl:
    case UhdmType::DelayTerm:
    case UhdmType::Thread:
    case UhdmType::WaitStmt:
    case UhdmType::WaitFork:
    case UhdmType::OrderedWait:
    case UhdmType::Disable:
    case UhdmType::DisableFork:
    case UhdmType::Force:
    case UhdmType::Deassign:
    case UhdmType::Release:
    case UhdmType::SequenceInst:
    case UhdmType::SeqFormalDecl:
    case UhdmType::SequenceDecl:
    case UhdmType::PropFormalDecl:
    case UhdmType::PropertyInst:
    case UhdmType::PropertySpec:
    case UhdmType::PropertyDecl:
    case UhdmType::ClockedProperty:
    case UhdmType::CasePropertyItem:
    case UhdmType::CaseProperty:
    case UhdmType::MulticlockSequenceExpr:
    case UhdmType::ClockedSeq:
    case UhdmType::RealVar:
    case UhdmType::TimeVar:
    case UhdmType::ChandleVar:
    case UhdmType::CheckerPort:
    case UhdmType::CheckerInstPort:
    case UhdmType::SwitchTran:
    case UhdmType::Udp:
    case UhdmType::ModPath:
    case UhdmType::Tchk:
    case UhdmType::UdpDefn:
    case UhdmType::TableEntry:
    case UhdmType::ClockingBlock:
    case UhdmType::ClockingIODecl:
    case UhdmType::ProgramArray:
    case UhdmType::SwitchArray:
    case UhdmType::UdpArray:
    case UhdmType::TchkTerm:
    case UhdmType::TimeNet:
    case UhdmType::NamedEvent:
    case UhdmType::VirtualInterfaceVar:
    case UhdmType::Extends:
    case UhdmType::ClassDefn:
    case UhdmType::ClassObj:
    case UhdmType::Program:
    case UhdmType::CheckerDecl:
    case UhdmType::CheckerInst:
    case UhdmType::ShortRealTypespec:
    case UhdmType::RealTypespec:
    case UhdmType::TimeTypespec:
    case UhdmType::ChandleTypespec:
    case UhdmType::SequenceTypespec:
    case UhdmType::PropertyTypespec:
    case UhdmType::UserSystf:
    case UhdmType::MethodFuncCall:
    case UhdmType::MethodTaskCall:
    case UhdmType::ConstraintOrdering:
    case UhdmType::Constraint:
    case UhdmType::Distribution:
    case UhdmType::DistItem:
    case UhdmType::Implication:
    case UhdmType::ConstrIf:
    case UhdmType::ConstrIfElse:
    case UhdmType::ConstrForeach:
    case UhdmType::SoftDisable:
    case UhdmType::ForkStmt:
    case UhdmType::EventStmt:
    case UhdmType::EventTypespec:
      reportError(object);
      break;
    case UhdmType::ExpectStmt:
    case UhdmType::Cover:
    case UhdmType::Assume:
    case UhdmType::Restrict:
    case UhdmType::ImmediateAssume:
    case UhdmType::ImmediateCover:
      if (!m_allowFormal) reportError(object);
      break;
    default:
      break;
  }
}

void SynthSubset::leaveTask(const Task* topobject, vpiHandle handle) {
  // Give specific error for non-synthesizable tasks
  std::function<void(const Any*, const Any*)> inst_visit =
      [&inst_visit, this](const Any* stmt, const Any* top) {
        UhdmType type = stmt->getUhdmType();
        AnyCollection* stmts = nullptr;
        if (type == UhdmType::Begin) {
          Begin* b = (Begin*)stmt;
          stmts = b->getStmts();
        }
        if (stmts) {
          for (auto st : *stmts) {
            UhdmType sttype = st->getUhdmType();
            switch (sttype) {
              case UhdmType::WaitStmt:
              case UhdmType::WaitFork:
              case UhdmType::OrderedWait:
              case UhdmType::Disable:
              case UhdmType::DisableFork:
              case UhdmType::Force:
              case UhdmType::Deassign:
              case UhdmType::Release:
              case UhdmType::SoftDisable:
              case UhdmType::ForkStmt:
              case UhdmType::EventStmt: {
                reportError(top);
                break;
              }
              default: {
              }
            }
            inst_visit(st, top);
          }
        }
      };

  if (const Any* stmt = topobject->getStmt()) {
    inst_visit(stmt, topobject);
  }
}

void SynthSubset::leaveSysTaskCall(const SysTaskCall* object,
                                   vpiHandle handle) {
  const std::string_view name = object->getName();
  if (m_nonSynthSysCalls.find(name) != m_nonSynthSysCalls.end()) {
    reportError(object);
  }
}

void removeFromCollection(AnyCollection* vec, const Any* object) {
  AnyCollection::const_iterator it =
      std::find(vec->cbegin(), vec->cend(), object);
  if (it != vec->cend()) vec->erase(it);
}

void SynthSubset::filterNonSynthesizable() {
  for (auto p : m_scheduledFilteredObjects) {
    removeFromCollection(p.first, p.second);
  }
}

void SynthSubset::leaveSysFuncCall(const SysFuncCall* object,
                                   vpiHandle handle) {
  const std::string_view name = object->getName();
  if (m_nonSynthSysCalls.find(name) != m_nonSynthSysCalls.end()) {
    reportError(object);
  }
  // Filter out sys func calls stmt from initial block
  if (name == "$error" || name == "$finish" || name == "$display") {
    bool inInitialBlock = false;
    const Any* parent = object->getParent();
    while (parent) {
      if (parent->getUhdmType() == UhdmType::Initial) {
        inInitialBlock = true;
        break;
      }
      parent = parent->getParent();
    }
    if (inInitialBlock) {
      parent = object->getParent();
      if (parent->getUhdmType() == UhdmType::Begin) {
        Begin* st = (Begin*)parent;
        if (st->getStmts()) {
          m_scheduledFilteredObjects.emplace_back(st->getStmts(), object);
        }
      }
    }
  }
}

void SynthSubset::leaveClassTypespec(const ClassTypespec* object,
                                     vpiHandle handle) {
  if (const Any* def = object->getClassDefn())
    reportError(def);
  else
    reportError(object);
}

void SynthSubset::leaveClassVar(const ClassVar* object, vpiHandle handle) {
  if (const RefTypespec* rt = object->getTypespec()) {
    if (const ClassTypespec* spec = rt->getActual<ClassTypespec>()) {
      if (const ClassDefn* def = spec->getClassDefn()) {
        if (reportedParent(def)) {
          mark(object);
          return;
        }
      }
    }
  }
  reportError(object);
}

void SynthSubset::mark(const Any* object) {
  m_nonSynthesizableObjects.emplace(object);
}

bool SynthSubset::reportedParent(const Any* object) {
  if (object->getUhdmType() == UhdmType::Package) {
    if (object->getName() == "builtin") return true;
  } else if (object->getUhdmType() == UhdmType::ClassDefn) {
    if (object->getName() == "work@semaphore" ||
        object->getName() == "work@process" ||
        object->getName() == "work@mailbox")
      return true;
  }
  if (m_nonSynthesizableObjects.find(object) !=
      m_nonSynthesizableObjects.end()) {
    return true;
  }
  if (const Any* parent = object->getParent()) {
    return reportedParent(parent);
  }
  return false;
}

// Apply some rewrite rule for Synlig limitations, namely Synlig handles aliased
// typespec incorrectly.
void SynthSubset::leaveRefTypespec(const RefTypespec* object,
                                   vpiHandle handle) {
  if (const TypedefTypespec* actual = object->getActual<TypedefTypespec>()) {
    if (const RefTypespec* ref_alias = actual->getTypedefAlias()) {
      // Make the typespec point to the aliased typespec if they are of the same
      // type:
      //   typedef lc_tx_e lc_tx_t;
      // When extra dimensions are added using a packed_array_typespec like in:
      //  typedef lc_tx_e [1:0] lc_tx_t;
      //  We will need to uniquify and create a new typespec instance
      if ((ref_alias->getActual()->getUhdmType() == actual->getUhdmType()) &&
          !ref_alias->getActual()->getName().empty()) {
        ((RefTypespec*)object)->setActual((Typespec*)ref_alias->getActual());
      }
    }
  }
}

void SynthSubset::leaveForStmt(const ForStmt* object, vpiHandle handle) {
  if (const Expr* cond = object->getCondition()) {
    if (cond->getUhdmType() == UhdmType::Operation) {
      Operation* topOp = (Operation*)cond;
      AnyCollection* operands = topOp->getOperands();
      const Any* parent = object->getParent();
      if (topOp->getOpType() == vpiLogAndOp) {
        // Rewrite rule for Yosys (Cannot handle non-constant expression in for
        // loop condition besides loop var)
        // Transforms:
        //  for (int i=0; i<32 && found==0; i++) begin
        //  end
        // Into:
        //  for (int i=0; i<32; i++) begin
        //    if (found==0) break;
        //  end
        //
        // Assumes lhs is comparator over loop var
        // rhs is Any expression
        Any* lhs = operands->at(0);
        Any* rhs = operands->at(1);
        ((ForStmt*)object)->setCondition((Expr*)lhs);
        AnyCollection* stlist = nullptr;
        if (const Any* stmt = object->getStmt()) {
          if (stmt->getUhdmType() == UhdmType::Begin) {
            Begin* st = (Begin*)stmt;
            stlist = st->getStmts();
          }
          if (stlist) {
            IfStmt* ifstmt = m_serializer->make<IfStmt>();
            stlist->insert(stlist->begin(), ifstmt);
            ifstmt->setCondition((Expr*)rhs);
            BreakStmt* brk = m_serializer->make<BreakStmt>();
            ifstmt->setStmt(brk);
          }
        }
      } else {
        if (isInUhdmAllIterator()) return;
        // Rewrite rule for Yosys (Cannot handle non-constant expression as a
        // bound for loop var) Transforms:
        //   logic [1:0] bound;
        //   for(j=0;j<bound;j=j+1) Q = 1'b1;
        // Into:
        //   case (i)
        //     0 :
        //       for(j=0;j<0;j=j+1) Q = 1'b1;
        //     1 :
        //       for(j=0;j<1;j=j+1) Q = 1'b1;
        //   endcase
        bool needsTransform = false;
        LogicNet* var = nullptr;
        if (operands->size() == 2) {
          Any* op = operands->at(1);
          if (op->getUhdmType() == UhdmType::RefObj) {
            RefObj* ref = (RefObj*)op;
            Any* actual = ref->getActual();
            if (actual) {
              if (actual->getUhdmType() == UhdmType::LogicNet) {
                needsTransform = true;
                var = (LogicNet*)actual;
              }
            }
          }
        }
        if (needsTransform) {
          // Check that we are in an always stmt
          needsTransform = false;
          const Any* tmp = parent;
          while (tmp) {
            if (tmp->getUhdmType() == UhdmType::Always) {
              needsTransform = true;
              break;
            }
            tmp = tmp->getParent();
          }
        }
        if (needsTransform) {
          ExprEval eval;
          bool invalidValue = false;
          uint32_t size = eval.size(var, invalidValue, parent->getParent(),
                                    parent, true, true);
          CaseStmt* case_st = m_serializer->make<CaseStmt>();
          case_st->setCaseType(vpiCaseExact);
          case_st->setParent((Any*)parent);
          AnyCollection* stmts = nullptr;
          if (parent->getUhdmType() == UhdmType::Begin) {
            stmts = any_cast<Begin>(parent)->getStmts();
          }
          if (stmts) {
            // Substitute the for loop with a case stmt
            for (AnyCollection::iterator itr = stmts->begin();
                 itr != stmts->end(); itr++) {
              if ((*itr) == object) {
                stmts->insert(itr, case_st);
                break;
              }
            }
            for (AnyCollection::iterator itr = stmts->begin();
                 itr != stmts->end(); itr++) {
              if ((*itr) == object) {
                stmts->erase(itr);
                break;
              }
            }
          }
          // Construct the case stmt
          RefObj* ref = m_serializer->make<RefObj>();
          ref->setName(var->getName());
          ref->setActual(var);
          ref->setParent(case_st);
          case_st->setCondition(ref);
          CaseItemCollection* items = m_serializer->makeCollection<CaseItem>();
          case_st->setCaseItems(items);
          for (uint32_t i = 0; i < size; i++) {
            CaseItem* item = m_serializer->make<CaseItem>();
            item->setParent(case_st);
            Constant* c = m_serializer->make<Constant>();
            c->setConstType(vpiUIntConst);
            c->setValue("UINT:" + std::to_string(i));
            c->setDecompile(std::to_string(i));
            c->setParent(item);
            AnyCollection* exprs = m_serializer->makeCollection<Any>();
            exprs->push_back(c);
            item->setExprs(exprs);
            items->push_back(item);
            ElaboratorContext elaboratorContext(m_serializer);
            ForStmt* clone = (ForStmt*)clone_tree(object, &elaboratorContext);
            clone->setParent(item);
            Operation* cond_op = any_cast<Operation>(clone->getCondition());
            AnyCollection* operands = cond_op->getOperands();
            for (uint32_t ot = 0; ot < operands->size(); ot++) {
              if (operands->at(ot)->getName() == var->getName()) {
                operands->at(ot) = c;
                break;
              }
            }
            item->setStmt(clone);
          }
        }
      }
    }
  }
}

void SynthSubset::leavePort(const Port* object, vpiHandle handle) {
  if (isInUhdmAllIterator()) return;
  bool signedLowConn = false;
  if (const Any* lc = object->getLowConn()) {
    if (const RefObj* ref = any_cast<RefObj>(lc)) {
      if (const Any* actual = ref->getActual()) {
        if (actual->getUhdmType() == UhdmType::LogicVar) {
          LogicVar* var = (LogicVar*)actual;
          if (var->getSigned()) {
            signedLowConn = true;
          }
        }
        if (actual->getUhdmType() == UhdmType::LogicNet) {
          LogicNet* var = (LogicNet*)actual;
          if (var->getSigned()) {
            signedLowConn = true;
          }
        }
      }
    }
  }
  if (signedLowConn) return;

  std::string highConnSignal;
  const Any* reportObject = object;
  if (const Any* hc = object->getHighConn()) {
    if (const RefObj* ref = any_cast<RefObj>(hc)) {
      reportObject = ref;
      if (const Any* actual = ref->getActual()) {
        if (actual->getUhdmType() == UhdmType::LogicVar) {
          LogicVar* var = (LogicVar*)actual;
          if (var->getSigned()) {
            highConnSignal = actual->getName();
            var->setSigned(false);
            if (const RefTypespec* tps = var->getTypespec()) {
              if (const LogicTypespec* ltps =
                      any_cast<LogicTypespec>(tps->getActual())) {
                ((LogicTypespec*)ltps)->setSigned(false);
              }
            }
          }
        }
        if (actual->getUhdmType() == UhdmType::LogicNet) {
          LogicNet* var = (LogicNet*)actual;
          if (var->getSigned()) {
            highConnSignal = actual->getName();
            var->setSigned(false);
            if (const RefTypespec* tps = var->getTypespec()) {
              if (const LogicTypespec* ltps =
                      any_cast<LogicTypespec>(tps->getActual())) {
                ((LogicTypespec*)ltps)->setSigned(false);
              }
            }
          }
        }
      }
    }
  }
  if (!highConnSignal.empty()) {
    const std::string errMsg(highConnSignal);
    m_serializer->getErrorHandler()(ErrorType::UHDM_FORCING_UNSIGNED_TYPE,
                                    errMsg, reportObject, nullptr);
  }
}

// Transform 3 vars sensitivity list into 2 vars sensitivity list because of a
// Yosys limitation
void SynthSubset::leaveAlways(const Always* object, vpiHandle handle) {
  // Transform: always @ (posedge clk or posedge rst or posedge start)
  //              if (rst | start) ...
  // Into:
  //            wire \synlig_tmp = rst | start;
  //            always @ (posedge clk or posedge \synlig_tmp )
  //               if (\synlig_tmp ) ...
  if (const Any* stmt = object->getStmt()) {
    if (const EventControl* ec = any_cast<EventControl>(stmt)) {
      if (const Operation* cond_op = any_cast<Operation>(ec->getCondition())) {
        AnyCollection* operands_top = cond_op->getOperands();
        AnyCollection* operands_op0 = nullptr;
        AnyCollection* operands_op1 = nullptr;
        Any* opLast = nullptr;
        int totalOperands = 0;
        if (operands_top->size() > 1) {
          if (operands_top->at(0)->getUhdmType() == UhdmType::Operation) {
            Operation* op = (Operation*)operands_top->at(0);
            operands_op0 = op->getOperands();
            totalOperands += operands_op0->size();
          }
          if (operands_top->at(1)->getUhdmType() == UhdmType::Operation) {
            Operation* op = (Operation*)operands_top->at(1);
            opLast = op;
            operands_op1 = op->getOperands();
            totalOperands += operands_op1->size();
          }
        }
        if (totalOperands != 3) {
          return;
        }
        Any* opMiddle = operands_op0->at(1);
        if (opMiddle->getUhdmType() == UhdmType::Operation &&
            opLast->getUhdmType() == UhdmType::Operation) {
          Operation* opM = (Operation*)opMiddle;
          Operation* opL = (Operation*)opLast;
          Any* midVar = opM->getOperands()->at(0);
          std::string_view var2Name = midVar->getName();
          std::string_view var3Name = opL->getOperands()->at(0)->getName();
          if (opM->getOpType() == opL->getOpType()) {
            AnyCollection* stmts = nullptr;
            if (const Scope* st = any_cast<Scope>(ec->getStmt())) {
              if (st->getUhdmType() == UhdmType::Begin) {
                stmts = any_cast<Begin>(st)->getStmts();
              }
            } else if (const Any* st = any_cast<Any>(ec->getStmt())) {
              stmts = m_serializer->makeCollection<Any>();
              stmts->push_back((Any*)st);
            }
            if (!stmts) return;
            for (auto stmt : *stmts) {
              Expr* cond = nullptr;
              if (stmt->getUhdmType() == UhdmType::IfElse) {
                cond = any_cast<IfElse>(stmt)->getCondition();
              } else if (stmt->getUhdmType() == UhdmType::IfStmt) {
                cond = any_cast<IfStmt>(stmt)->getCondition();
              } else if (stmt->getUhdmType() == UhdmType::CaseStmt) {
                cond = any_cast<CaseStmt>(stmt)->getCondition();
              }
              if (cond->getUhdmType() == UhdmType::Operation) {
                Operation* op = (Operation*)cond;
                // Check that the sensitivity vars are used as a or-ed
                // condition
                if (op->getOpType() == vpiBitOrOp) {
                  AnyCollection* operands = op->getOperands();
                  if (operands->at(0)->getName() == var2Name &&
                      operands->at(1)->getName() == var3Name) {
                    // All conditions are met, perform the transformation

                    // Remove: "posedge rst" from that part of the tree
                    operands_op0->pop_back();

                    // Create expression: rst | start
                    Operation* orOp = m_serializer->make<Operation>();
                    orOp->setOpType(vpiBitOrOp);
                    orOp->setOperands(m_serializer->makeCollection<Any>());
                    orOp->getOperands()->push_back(midVar);
                    orOp->getOperands()->push_back(opL->getOperands()->at(0));

                    // Move up the tree: posedge clk
                    operands_top->at(0) = operands_op0->at(0);

                    // Create: wire \synlig_tmp = rst | start;
                    ContAssign* ass = m_serializer->make<ContAssign>();
                    LogicNet* lhs = m_serializer->make<LogicNet>();
                    std::string tmpName = std::string("synlig_tmp_") +
                                          std::string(var2Name) + "_or_" +
                                          std::string(var3Name);
                    lhs->setName(tmpName);
                    ass->setLhs(lhs);
                    RefObj* ref = m_serializer->make<RefObj>();
                    ref->setName(tmpName);
                    ref->setActual(lhs);
                    ass->setRhs(orOp);
                    const Any* instance = object->getParent();
                    if (instance->getUhdmType() == UhdmType::Module) {
                      Module* mod = (Module*)instance;
                      if (mod->getContAssigns() == nullptr) {
                        mod->setContAssigns(
                            m_serializer->makeCollection<ContAssign>());
                      }
                      bool found = false;
                      for (ContAssign* ca : *mod->getContAssigns()) {
                        if (ca->getLhs()->getName() == tmpName) {
                          found = true;
                          break;
                        }
                      }
                      if (!found) mod->getContAssigns()->push_back(ass);
                    }

                    // Redirect condition to: if (\synlig_tmp ) ...
                    if (stmt->getUhdmType() == UhdmType::IfElse) {
                      any_cast<IfElse>(stmt)->setCondition(ref);
                    } else if (stmt->getUhdmType() == UhdmType::IfStmt) {
                      any_cast<IfStmt>(stmt)->setCondition(ref);
                    } else if (stmt->getUhdmType() == UhdmType::CaseStmt) {
                      any_cast<CaseStmt>(stmt)->setCondition(ref);
                    }

                    // Redirect 2nd sensitivity list signal to: posedge
                    // \synlig_tmp
                    opL->getOperands()->at(0) = ref;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void SynthSubset::leaveArrayVar(const ArrayVar* object, vpiHandle handle) {
  VariablesCollection* vars = object->getVariables();
  if (!vars) return;
  if (vars->empty()) return;
  Variables* var = vars->at(0);
  const RefTypespec* ref_tps = var->getTypespec();
  if (!ref_tps) return;
  const Typespec* tps = ref_tps->getActual();
  if (tps->getUhdmType() == UhdmType::LogicTypespec) {
    LogicTypespec* ltps = (LogicTypespec*)tps;
    if ((tps->getName().empty())) {
      if (ltps->getRanges() && ltps->getRanges()->size() == 1) {
        ((ArrayVar*)object)->setTypespec((RefTypespec*)ref_tps);
      }
    } else {
      if (ltps->getRanges() && ltps->getRanges()->size() == 1) {
        ElaboratorContext elaboratorContext(m_serializer);
        LogicTypespec* clone =
            (LogicTypespec*)clone_tree(ltps, &elaboratorContext);
        clone->setName("");
        ((RefTypespec*)ref_tps)->setActual(clone);
        ((ArrayVar*)object)->setTypespec((RefTypespec*)ref_tps);
      }
    }
  }
}

void SynthSubset::leaveLogicNet(const LogicNet* object, vpiHandle handle) {
  if (!isInUhdmAllIterator()) return;
  ((LogicNet*)object)->setTypespec(nullptr);
}

}  // namespace uhdm
