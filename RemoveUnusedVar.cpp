//===----------------------------------------------------------------------===//
// 
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RemoveUnusedVar.h"

#include <sstream>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"

#include "RewriteUtils.h"
#include "TransformationManager.h"

using namespace clang;

static const char *DescriptionMsg =
"Remove unused local/global variable declarations. \n";

static RegisterTransformation<RemoveUnusedVar>
         Trans("remove-unused-var", DescriptionMsg);

class RemoveUnusedVarAnalysisVisitor : public 
  RecursiveASTVisitor<RemoveUnusedVarAnalysisVisitor> {
public:

  explicit RemoveUnusedVarAnalysisVisitor(RemoveUnusedVar *Instance)
    : ConsumerInstance(Instance)
  { }

  bool VisitVarDecl(VarDecl *VD);

  bool VisitDeclStmt(DeclStmt *DS);

private:

  RemoveUnusedVar *ConsumerInstance;
};

bool RemoveUnusedVarAnalysisVisitor::VisitVarDecl(VarDecl *VD)
{
  if (VD->isReferenced() || dyn_cast<ParmVarDecl>(VD))
    return true;

  ConsumerInstance->ValidInstanceNum++;
  if (ConsumerInstance->ValidInstanceNum == 
      ConsumerInstance->TransformationCounter) {
    ConsumerInstance->TheVarDecl = VD;
  }
  return true;
}

bool RemoveUnusedVarAnalysisVisitor::VisitDeclStmt(DeclStmt *DS)
{   
  for (DeclStmt::decl_iterator I = DS->decl_begin(),
       E = DS->decl_end(); I != E; ++I) {
    VarDecl *CurrDecl = dyn_cast<VarDecl>(*I);
    if (CurrDecl) {
      DeclGroupRef DGR = DS->getDeclGroup();
      ConsumerInstance->VarToDeclGroup[CurrDecl] = DGR;
    }
  }
  return true;
}

void RemoveUnusedVar::Initialize(ASTContext &context)
{
  Context = &context;
  SrcManager = &Context->getSourceManager();
  AnalysisVisitor = new RemoveUnusedVarAnalysisVisitor(this);
  TheRewriter.setSourceMgr(Context->getSourceManager(), 
                           Context->getLangOptions());
}

void RemoveUnusedVar::HandleTopLevelDecl(DeclGroupRef D) 
{
  for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I) {
    VarDecl *VD = dyn_cast<VarDecl>(*I);
    if (VD)
      VarToDeclGroup[VD] = D;
  }
}
 
void RemoveUnusedVar::HandleTranslationUnit(ASTContext &Ctx)
{
  AnalysisVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

  if (QueryInstanceOnly)
    return;

  if (TransformationCounter > ValidInstanceNum) {
    TransError = TransMaxInstanceError;
    return;
  }

  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  TransAssert(TheVarDecl && "NULL TheFunctionDecl!");

  removeVarDecl();

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

void RemoveUnusedVar::removeVarDecl(void)
{
  llvm::DenseMap<const VarDecl *, DeclGroupRef>::iterator DI = 
    VarToDeclGroup.find(TheVarDecl);
  TransAssert((DI != VarToDeclGroup.end()) &&
              "Cannot find VarDeclGroup!");

  RewriteUtils::removeVarDecl(TheVarDecl, (*DI).second, 
                              &TheRewriter, SrcManager);
}

RemoveUnusedVar::~RemoveUnusedVar(void)
{
  if (AnalysisVisitor)
    delete AnalysisVisitor;
}
