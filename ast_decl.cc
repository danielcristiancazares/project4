/* File: ast_decl.cc
 * -----------------
 * Implementation of Decl node classes.
 */
#include "ast_decl.h"
#include "ast_type.h"
#include "ast_stmt.h"
#include "symtable.h"
#include "irgen.h"

Decl::Decl(Identifier *n) : Node(*n->GetLocation()) {
  Assert(n != NULL);
  (id = n)->SetParent(this);
}

VarDecl::VarDecl(Identifier *n, Type *t, Expr *e) : Decl(n) {
  Assert(n != NULL && t != NULL);
  (type = t)->SetParent(this);
  if(e) { (assignTo = e)->SetParent(this); }
  typeq = NULL;
}

VarDecl::VarDecl(Identifier *n, TypeQualifier *tq, Expr *e) : Decl(n) {
  Assert(n != NULL && tq != NULL);
  (typeq = tq)->SetParent(this);
  if(e) { (assignTo = e)->SetParent(this); }
  type = NULL;
}

VarDecl::VarDecl(Identifier *n, Type *t, TypeQualifier *tq, Expr *e) : Decl(n) {
  Assert(n != NULL && t != NULL && tq != NULL);
  (type = t)->SetParent(this);
  (typeq = tq)->SetParent(this);
  if(e) { (assignTo = e)->SetParent(this); }
}

void VarDecl::PrintChildren(int indentLevel) {
  if(typeq) { typeq->Print(indentLevel + 1); }
  if(type) { type->Print(indentLevel + 1); }
  if(id) { id->Print(indentLevel + 1); }
  if(assignTo) { assignTo->Print(indentLevel + 1, "(initializer) "); }
}

llvm::Value *VarDecl::Emit() {
  cerr << "VarDecl is called" << endl;
  llvm::Value *value = NULL;
  char *name;
  bool isConstant;
  llvm::Constant *constant;
  SymbolTable::DeclAssoc declassoc;

  // gets llvm type
  llvm::Type *type = Node::irgen->Converter(this->type);

  // sets the constant
  if(this->assignTo != NULL) {
    constant = llvm::cast<llvm::Constant>(this->assignTo->Emit());
  }
  else {
    constant = llvm::Constant::getNullValue(type);
  }

  if(this->typeq->constTypeQualifier != NULL) {
    isConstant = true;
  }
  else {
    isConstant = false;
  }

  name = this->GetIdentifier()->GetName();

  llvm::Module *mod = irgen->GetOrCreateModule("irgen.bc");

  if(Node::symtable->symTable.empty()) {
    map <string, SymbolTable::DeclAssoc> newMap;
    value = new llvm::GlobalVariable(*mod, type, isConstant, llvm::GlobalValue::ExternalLinkage, constant, name);
    declassoc.value = value;
    declassoc.decl = this;
    declassoc.isGlobal = true;
    //cerr << "TABLE IS EMPTY!!!" << endl;
    //cerr << "global variable declared" << endl;
    //cerr << "This is what the declass global is" << declassoc.isGlobal << endl;
    newMap.insert(pair<string, SymbolTable::DeclAssoc>(name, declassoc));
    Node::symtable->symTable.push_back(newMap);
  }
  else {
    map <string, SymbolTable::DeclAssoc> currentScope = Node::symtable->symTable.back();
    Node::symtable->symTable.pop_back();
    string currentVar = this->GetIdentifier()->GetName();

    // checks if this is a global scope
    SymbolTable::DeclAssoc currDeclAssoc = currentScope.rbegin()->second;
    //cerr << "TABLE IS NOT EMPTY!!!" << endl;
    //cerr << "This is what the declass global is" << currentScope.rbegin()->second.isGlobal << endl;
    if(currDeclAssoc.isGlobal == true) {
      //cerr << "global variable declared" << endl;
      value = new llvm::GlobalVariable(*mod, type, isConstant, llvm::GlobalValue::ExternalLinkage, constant, name);
      declassoc.isGlobal = true;
    }
    else {
      //cerr << "local var declared" << endl;
      value = new llvm::AllocaInst(type, name, irgen->GetBasicBlock());
      new llvm::StoreInst(constant, value, irgen->GetBasicBlock());
    }
    declassoc.value = value;
    declassoc.decl = this;
    currentScope.insert(map<string, SymbolTable::DeclAssoc>(name, declassoc));
    Node::symtable->symTable.push_back(currentScope);
  }

  return value;
}

FnDecl::FnDecl(Identifier *n, Type *r, List<VarDecl *> *d) : Decl(n) {
  Assert(n != NULL && r != NULL && d != NULL);
  (returnType = r)->SetParent(this);
  (formals = d)->SetParentAll(this);
  body = NULL;
  returnTypeq = NULL;
}

FnDecl::FnDecl(Identifier *n, Type *r, TypeQualifier *rq, List<VarDecl *> *d) : Decl(n) {
  Assert(n != NULL && r != NULL && rq != NULL && d != NULL);
  (returnType = r)->SetParent(this);
  (returnTypeq = rq)->SetParent(this);
  (formals = d)->SetParentAll(this);
  body = NULL;
}

void FnDecl::SetFunctionBody(Stmt *b) {
  (body = b)->SetParent(this);
}

void FnDecl::PrintChildren(int indentLevel) {
  if(returnType) { returnType->Print(indentLevel + 1, "(return type) "); }
  if(id) { id->Print(indentLevel + 1); }
  if(formals) { formals->PrintAll(indentLevel + 1, "(formals) "); }
  if(body) { body->Print(indentLevel + 1, "(body) "); }
}

llvm::Value *FnDecl::Emit() {
  // TODO
  //cerr << "FnDecl is called" << endl;
  // storing the return type
  llvm::Type *returnType = irgen->Converter(this->returnType);

  // argtypes
  vector <llvm::Type *> argTypes;
  llvm::Type *tempType;
  llvm::Value *value;
  SymbolTable::DeclAssoc declassoc;

  //go through the formals
  for (int i = 0; i < this->formals->NumElements(); i++) {
    //cerr << "Populing the formals1" << endl;
    VarDecl *v = this->formals->Nth(i);
    tempType = irgen->Converter(v->GetType());
    argTypes.push_back(tempType);
  }

  // make an arrayRef
  llvm::ArrayRef<llvm::Type *> argArray(argTypes);
  llvm::FunctionType *funcTy = llvm::FunctionType::get(returnType, argArray, false);

  // Create the function and insert it into module
  string name = this->GetIdentifier()->GetName();
  llvm::Module *mod = irgen->GetOrCreateModule("irgen.bc");
  llvm::StringRef s = llvm::StringRef(name);
  llvm::Function *f = llvm::cast<llvm::Function>(mod->getOrInsertFunction(s, funcTy));

  // starting to loop through function pointer
  string argName;
  llvm::Function::arg_iterator it = f->arg_begin();
  for (int i = 0; i < this->formals->NumElements(); i++) {
    //cerr << "Populing the formals2" << endl;
    VarDecl *v = this->formals->Nth(i);
    v->Emit();
    argName = v->GetIdentifier()->GetName();
    // set the name
    it->setName(argName);
    it++;
  }

  // insert a block into the function
  // create a basicBlock
  llvm::LLVMContext *context = irgen->GetContext();
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(*context, "entry", f, irgen->GetBasicBlock());
  irgen->SetBasicBlock(bb);
  if (Node::symtable->symTable.empty()) {
    //cerr << "FnDecl, EMPTY TABLE" << endl;
    string name = this->GetIdentifier()->GetName();
    map <string, SymbolTable::DeclAssoc> newScope;
    declassoc.decl = this;
    declassoc.value = f;
    newScope.insert(pair<string, SymbolTable::DeclAssoc>(name, declassoc));
    Node::symtable->symTable.push_back(newScope);
  }
  else {
    // inserting the function name to the current scope
    //cerr << "FnDecl, NOT EMPTY TABLE" << endl;
    map <string, SymbolTable::DeclAssoc> currentScope = Node::symtable->symTable.back();
    Node::symtable->symTable.pop_back();
    declassoc.decl = this;
    declassoc.value = f;
    currentScope.insert(pair<string, SymbolTable::DeclAssoc>(name, declassoc));
    Node::symtable->symTable.push_back(currentScope);
  }


  // creating a new scope for the formals
  map<string, SymbolTable::DeclAssoc> newScope;
  llvm::Function::arg_iterator arg = f->arg_begin();
  for(int i = 0; i < this->formals->NumElements(); i++) {
    VarDecl *v = this->formals->Nth(i);
    name = v->GetIdentifier()->GetName();
    tempType = irgen->Converter(v->GetType());
    value = new llvm::AllocaInst(tempType, name, irgen->GetBasicBlock());
    new llvm::StoreInst(arg, value, irgen->GetBasicBlock());
    declassoc.value = value;
    declassoc.decl = v;
    declassoc.isGlobal = false;
    newScope.insert(pair<string, SymbolTable::DeclAssoc>(name, declassoc));
    it++;
  }
  // pushing new scope
  Node::symtable->symTable.push_back(newScope);

  StmtBlock *stmtblock = dynamic_cast<StmtBlock *>(this->body);
  stmtblock->EmitFromFunc();

  return f;

// create a return instruction
//    llvm::Value *val = llvm::ConstantInt::get(intTy, 1);
//    llvm::Value *sum = llvm::BinaryOperator::CreateAdd(arg, val, "", bb);
//    llvm::ReturnInst::Create(*context, sum, bb);

}