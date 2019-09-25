//===--- SourceFile.hpp - Source File AST -----------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "Sora/AST/ASTAlignement.hpp"
#include "Sora/AST/DeclContext.hpp"
#include "Sora/AST/Identifier.hpp"
#include "Sora/Common/LLVM.hpp"
#include "Sora/Common/SourceManager.hpp"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace sora {
class ASTWalker;
class ASTContext;
class Decl;

/// Represents a source file.
class alignas(SourceFileAlignement) SourceFile final : public DeclContext {
  SmallVector<Decl *, 4> members;
  BufferID bufferID;

  SourceFile(ASTContext &astContext, BufferID bufferID, DeclContext *parent)
      : DeclContext(DeclContextKind::SourceFile, parent), bufferID(bufferID),
        astContext(astContext) {}

  SourceFile(const SourceFile &) = delete;
  SourceFile &operator=(const SourceFile &) = delete;

public:
  /// \param ctxt the ASTContext in which the members of this source file
  /// are allocated (and also the one that'll be used to allocate this
  /// SourceFile's memory)
  /// \param bufferID the BufferID of this SourceFile in the SourceManager
  /// \param parent the parent DeclContext of this SourceFile. Can be nullptr.
  static SourceFile *create(ASTContext &ctxt, BufferID bufferID,
                            DeclContext *parent);

  /// \returns the members of this source file
  ArrayRef<Decl *> getMembers() const { return members; }
  /// Adds a member to this source file
  void addMember(Decl *decl) { return members.push_back(decl); }
  /// \returns the buffer id of this SourceFile
  BufferID getBufferID() const { return bufferID; }

  /// Traverse this SourceFile using \p walker.
  /// \returns true if the walk completed successfully, false if it ended
  /// prematurely.
  bool walk(ASTWalker &walker);

  static bool classof(const DeclContext *dc) {
    return dc->getDeclContextKind() == DeclContextKind::SourceFile;
  }

  /// The ASTContext in which the members of this source file
  /// are allocated.
  ASTContext &astContext;
};
} // namespace sora