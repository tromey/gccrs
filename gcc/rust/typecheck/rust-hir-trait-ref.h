// Copyright (C) 2020 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#ifndef RUST_HIR_TRAIT_REF_H
#define RUST_HIR_TRAIT_REF_H

#include "rust-hir-type-check-base.h"
#include "rust-hir-full.h"
#include "rust-tyty-visitor.h"

namespace Rust {
namespace Resolver {

// Data Objects
class TraitItemReference
{
public:
  enum TraitItemType
  {
    FN,
    CONST,
    TYPE,
    ERROR
  };

  TraitItemReference (std::string identifier, bool optional, TraitItemType type,
		      const HIR::TraitItem *hir_trait_item, TyTy::BaseType *ty)
    : identifier (identifier), optional_flag (optional), type (type),
      hir_trait_item (hir_trait_item), ty (ty)
  {}

  static TraitItemReference error ()
  {
    return TraitItemReference ("", false, ERROR, nullptr, nullptr);
  }

  bool is_error () const { return type == ERROR; }

  std::string as_string () const
  {
    return trait_item_type_as_string (type) + " " + identifier + " "
	   + hir_trait_item->as_string () + " " + ty->as_string ();
  }

  static std::string trait_item_type_as_string (TraitItemType ty)
  {
    switch (ty)
      {
      case FN:
	return "FN";
      case CONST:
	return "CONST";
      case TYPE:
	return "TYPE";
      case ERROR:
	return "ERROR";
      }
    return "ERROR";
  }

  bool is_optional () const { return optional_flag; }

private:
  std::string identifier;
  bool optional_flag;
  TraitItemType type;
  const HIR::TraitItem *hir_trait_item;
  TyTy::BaseType *ty;
};

class TraitReference
{
public:
  TraitReference (const HIR::Trait *hir_trait_ref,
		  std::vector<TraitItemReference> item_refs)
    : hir_trait_ref (hir_trait_ref), item_refs (item_refs)
  {}

  static TraitReference error () { return TraitReference (nullptr, {}); }

  bool is_error () const { return hir_trait_ref == nullptr; }

  std::string as_string () const { return ""; }

private:
  const HIR::Trait *hir_trait_ref;
  std::vector<TraitItemReference> item_refs;
};

// Resolve
class ResolveTraitItemToRef : public TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static TraitItemReference Resolve (HIR::TraitItem &item)
  {
    ResolveTraitItemToRef resolver;
    item.accept_vis (resolver);
    return resolver.resolved;
  }

  void visit (HIR::TraitItemFunc &fn) override
  {
    TyTy::BaseType *ty = nullptr;
    resolved
      = TraitItemReference (TraitItemReference::TraitItemType::FN, &fn, ty);
  }

private:
  ResolveTraitItemToRef ()
    : TypeCheckBase (), resolved (TraitItemReference::error ())
  {}

  TraitItemReference resolved;
};

class ResolveTraitRef : public TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static TraitReference Resolve (HIR::TypePath &path)
  {
    ResolveTraitRef resolver;
    return resolver.go (path);
  }

private:
  ResolveTraitRef () : TypeCheckBase () {}

  TraitReference go (HIR::TypePath &path)
  {
    NodeId ref;
    if (!resolver->lookup_resolved_type (path.get_mappings ().get_nodeid (),
					 &ref))
      {
	rust_fatal_error (path.get_locus (),
			  "Failed to resolve path to node-id");
	return TraitReference::error ();
      }

    rust_debug ("resolved type-path [%s] to node-id: [%u]",
		path.as_string ().c_str (), ref);

    HirId hir_node = UNKNOWN_HIRID;
    if (!mappings->lookup_node_to_hir (mappings->get_current_crate (), ref,
				       &hir_node))
      {
	rust_fatal_error (path.get_locus (),
			  "Failed to resolve path to hir-id");
	return TraitReference::error ();
      }

    rust_debug ("resolved type-path [%s] to hir-id: [%u]",
		path.as_string ().c_str (), hir_node);

    HIR::Item *resolved_item
      = mappings->lookup_hir_item (mappings->get_current_crate (), hir_node);

    rust_assert (resolved_item != nullptr);
    resolved_item->accept_vis (*this);
    rust_assert (trait_reference != nullptr);

    // keep going and resolve the trait items
    rust_debug ("%s", trait_reference->as_string ().c_str ());

    std::vector<TraitItemReference> item_refs;
    for (auto &item : trait_reference->get_trait_items ())
      {
	TraitItemReference trait_item_ref
	  = ResolveTraitItemToRef::Resolve (*item.get ());
	item_refs.push_back (std::move (trait_item_ref));
      }

    return TraitReference (trait_reference, item_refs);
  }

  HIR::Trait *trait_reference;

public:
  void visit (HIR::Trait &trait) override { trait_reference = &trait; }
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_HIR_TRAIT_REF_H
