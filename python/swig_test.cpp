#include "swig_test.h"

#include "uhdm.h"
using namespace uhdm;

std::vector<vpiHandle> buildTestDesign(Serializer* s) {
  std::vector<vpiHandle> designs;

  Design* d = s->make<Design>();
  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  ModuleCollection* vm = s->makeCollection<Module>();
  d->setAllModules(vm);

  Module* m1 = s->make<Module>();
  m1->setName("module1");
  vm->push_back(m1);

  Module* m2 = s->make<Module>();
  m2->setName("module2");
  vm->push_back(m2);

  return designs;
}

std::vector<vpiHandle> buildTestTypedef(uhdm::Serializer* s) {
  std::vector<vpiHandle> designs;

  Design* d = s->make<Design>();
  vpiHandle dh = s->makeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  TypespecCollection* typespecs = s->makeCollection<Typespec>();
  d->setTypespecs(typespecs);

  StructTypespec* typespec1 = s->make<StructTypespec>();
  typespec1->setName("IR");
  typespecs->push_back(typespec1);

  TypespecMemberCollection* members = s->makeCollection<TypespecMember>();
  typespec1->setMembers(members);

  TypespecMember* member1 = s->make<TypespecMember>();
  member1->setName("opcode");
  member1->setParent(typespec1);
  members->push_back(member1);

  BitTypespec* btps = s->make<BitTypespec>();

  RefTypespec* btps_rt = s->make<RefTypespec>();
  btps_rt->setActualTypespec(btps);
  member1->setTypespec(btps_rt);

  RangeCollection* ranges = s->makeCollection<Range>();
  btps->setParent(member1);
  btps->setRanges(ranges);
  Range* range = s->make<Range>();
  range->setParent(btps);
  ranges->push_back(range);

  Constant* c1 = s->make<Constant>();
  c1->setParent(range);
  c1->setValue("UNIT:7");
  c1->setConstType(vpiUIntConst);
  c1->setDecompile("7");
  c1->setSize(64);
  range->setLeftExpr(c1);
  Constant* c2 = s->make<Constant>();
  c2->setParent(range);
  c2->setValue("UNIT:0");
  c2->setConstType(vpiUIntConst);
  c2->setDecompile("0");
  c2->setSize(64);
  range->setRightExpr(c2);

  TypespecMember* member2 = s->make<TypespecMember>();
  member2->setName("addr");
  member2->setParent(typespec1);
  members->push_back(member2);

  btps = s->make<BitTypespec>();

  btps_rt = s->make<RefTypespec>();
  btps_rt->setActualTypespec(btps);
  member2->setTypespec(btps_rt);

  ranges = s->makeCollection<Range>();
  btps->setParent(member2);
  btps->setRanges(ranges);
  range = s->make<Range>();
  range->setParent(btps);
  ranges->push_back(range);

  ranges = s->makeCollection<Range>();
  btps->setRanges(ranges);
  range = s->make<Range>();
  ranges->push_back(range);

  c1 = s->make<Constant>();
  c1->setParent(range);
  c1->setValue("UNIT:23");
  c1->setConstType(vpiUIntConst);
  c1->setDecompile("23");
  // c1->setSize(64);
  range->setLeftExpr(c1);
  c2 = s->make<Constant>();
  c2->setParent(range);
  c2->setValue("UNIT:0");
  c2->setConstType(vpiUIntConst);
  c2->setDecompile("0");
  // c2->setSize(64);
  range->setRightExpr(c2);

  return designs;
}
// debug
/*
void visit_designs(const std::vector<vpiHandle>& designs) {
  visit_designs(designs, std::cout);
}
*/
