
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2024 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * https://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * https://www.boost.org/LICENSE_1_0.txt
 * https://github.com/dlang/dmd/blob/master/src/dmd/dsymbol.h
 */

#pragma once

#include "root/port.h"
#include "ast_node.h"
#include "globals.h"
#include "arraytypes.h"
#include "visitor.h"

class CPPNamespaceDeclaration;
class Identifier;
struct Scope;
class DsymbolTable;
class Declaration;
class ThisDeclaration;
class BitFieldDeclaration;
class TypeInfoDeclaration;
class TupleDeclaration;
class AliasDeclaration;
class AggregateDeclaration;
class EnumDeclaration;
class ClassDeclaration;
class InterfaceDeclaration;
class StructDeclaration;
class UnionDeclaration;
class FuncDeclaration;
class FuncAliasDeclaration;
class OverDeclaration;
class FuncLiteralDeclaration;
class CtorDeclaration;
class PostBlitDeclaration;
class DtorDeclaration;
class StaticCtorDeclaration;
class StaticDtorDeclaration;
class SharedStaticCtorDeclaration;
class SharedStaticDtorDeclaration;
class InvariantDeclaration;
class UnitTestDeclaration;
class NewDeclaration;
class VarDeclaration;
class AttribDeclaration;
class VisibilityDeclaration;
class Package;
class Module;
class Import;
class Type;
class TypeTuple;
class WithStatement;
class LabelDsymbol;
class ScopeDsymbol;
class ForwardingScopeDsymbol;
class TemplateDeclaration;
class TemplateInstance;
class TemplateMixin;
class ForwardingAttribDeclaration;
class Nspace;
class EnumMember;
class WithScopeSymbol;
class ArrayScopeSymbol;
class SymbolDeclaration;
class Expression;
class ExpressionDsymbol;
class AliasAssign;
class OverloadSet;
class StaticAssert;
class StaticIfDeclaration;
class CAsmDeclaration;
struct AA;
#ifdef IN_GCC
typedef union tree_node Symbol;
#else
struct Symbol;
#endif

struct Ungag
{
    unsigned oldgag;

    Ungag(unsigned old) : oldgag(old) {}
    ~Ungag() { global.gag = oldgag; }
};

enum class ThreeState : uint8_t
{
    none,         // value not yet computed
    no,           // value is false
    yes,          // value is true
};

namespace dmd
{
    void dsymbolSemantic(Dsymbol *dsym, Scope *sc);
    void semantic2(Dsymbol *dsym, Scope *sc);
    void semantic3(Dsymbol *dsym, Scope* sc);
    // in iasm.d
    void asmSemantic(CAsmDeclaration *ad, Scope *sc);
    // in iasmgcc.d
    void gccAsmSemantic(CAsmDeclaration *ad, Scope *sc);
}

struct Visibility
{
    enum Kind
    {
        undefined,
        none,           // no access
        private_,
        package_,
        protected_,
        public_,
        export_
    };
    Kind kind;
    Package *pkg;
};

/* State of symbol in winding its way through the passes of the compiler
 */
enum class PASS : uint8_t
{
    initial,        // initial state
    semantic,       // semantic() started
    semanticdone,   // semantic() done
    semantic2,      // semantic2() started
    semantic2done,  // semantic2() done
    semantic3,      // semantic3() started
    semantic3done,  // semantic3() done
    inline_,         // inline started
    inlinedone,     // inline done
    obj             // toObjFile() run
};

/* Flags for symbol search
 */
typedef unsigned SearchOptFlags;
enum class SearchOpt : SearchOptFlags
{
    all                    = 0x00, // default
    ignorePrivateImports   = 0x01, // don't search private imports
    ignoreErrors           = 0x02, // don't give error messages
    ignoreAmbiguous        = 0x04, // return NULL if ambiguous
    localsOnly             = 0x08, // only look at locals (don't search imports)
    importsOnly            = 0x10, // only look in imports
    unqualifiedModule      = 0x20, // the module scope search is unqualified,
                                   // meaning don't search imports in that scope,
                                   // because qualified module searches search
                                   // their imports
    tagNameSpace           = 0x40, // search ImportC tag symbol table
    ignoreVisibility       = 0x80, // also find private and package protected symbols
};

struct FieldState
{
    unsigned offset;

    unsigned fieldOffset;
    unsigned fieldSize;
    unsigned fieldAlign;
    unsigned bitOffset;

    d_bool inFlight;
};

struct DsymbolAttributes;

class Dsymbol : public ASTNode
{
public:
    Identifier *ident;
    Dsymbol *parent;
    Symbol *csym;               // symbol for code generator
    Scope *_scope;               // !=NULL means context to use for semantic()
private:
    DsymbolAttributes* atts;
public:
    Loc loc;                    // where defined
    d_bool errors;                // this symbol failed to pass semantic()
    PASS semanticRun;
    unsigned short localNum;        // perturb mangled name to avoid collisions with those in FuncDeclaration.localsymtab
    static Dsymbol *create(Identifier *);
    const char *toChars() const final override;
    DeprecatedDeclaration* depdecl();
    CPPNamespaceDeclaration* cppnamespace();
    UserAttributeDeclaration* userAttribDecl();
    DeprecatedDeclaration* depdecl(DeprecatedDeclaration* dd);
    CPPNamespaceDeclaration* cppnamespace(CPPNamespaceDeclaration* ns);
    UserAttributeDeclaration* userAttribDecl(UserAttributeDeclaration* uad);
    virtual const char *toPrettyCharsHelper(); // helper to print fully qualified (template) arguments
    bool equals(const RootObject * const o) const override;
    bool isAnonymous() const;
    Module *getModule();
    bool isCsymbol();
    Module *getAccessModule();
    Dsymbol *pastMixin();
    Dsymbol *toParent();
    Dsymbol *toParent2();
    Dsymbol *toParentDecl();
    Dsymbol *toParentLocal();
    Dsymbol *toParentP(Dsymbol *p1, Dsymbol *p2 = nullptr);
    TemplateInstance *isInstantiated();
    bool followInstantiationContext(Dsymbol *p1, Dsymbol *p2 = nullptr);
    TemplateInstance *isSpeculative();
    Ungag ungagSpeculative();

    // kludge for template.isSymbol()
    DYNCAST dyncast() const override final { return DYNCAST_DSYMBOL; }

    virtual Identifier *getIdent();
    virtual const char *toPrettyChars(bool QualifyTypes = false);
    virtual const char *kind() const;
    virtual Dsymbol *toAlias();                 // resolve real symbol
    virtual Dsymbol *toAlias2();
    virtual bool overloadInsert(Dsymbol *s);
    virtual uinteger_t size(Loc loc);
    virtual bool isforwardRef();
    virtual AggregateDeclaration *isThis();     // is a 'this' required to access the member
    virtual bool isExport() const;              // is Dsymbol exported?
    virtual bool isImportedSymbol() const;      // is Dsymbol imported?
    virtual bool isDeprecated() const;                // is Dsymbol deprecated?
    virtual bool isOverloadable() const;
    virtual LabelDsymbol *isLabel();            // is this a LabelDsymbol?
    AggregateDeclaration *isMember();           // is toParent() an AggregateDeclaration?
    AggregateDeclaration *isMember2();          // is toParent2() an AggregateDeclaration?
    AggregateDeclaration *isMemberDecl();       // is toParentDecl() an AggregateDeclaration?
    AggregateDeclaration *isMemberLocal();      // is toParentLocal() an AggregateDeclaration?
    ClassDeclaration *isClassMember();          // isMember() is a ClassDeclaration?
    virtual Type *getType();                    // is this a type?
    virtual bool needThis();                    // need a 'this' pointer?
    virtual Visibility visible();
    virtual Dsymbol *syntaxCopy(Dsymbol *s);    // copy only syntax trees
    virtual bool oneMember(Dsymbol *&ps, Identifier *ident);
    virtual bool hasPointers();
    virtual bool hasStaticCtorOrDtor();
    virtual void addObjcSymbols(ClassDeclarations *, ClassDeclarations *) { }

    virtual void addComment(const utf8_t *comment);
    const utf8_t *comment();                      // current value of comment

    UnitTestDeclaration *ddocUnittest();
    void ddocUnittest(UnitTestDeclaration *);

    bool inNonRoot();

    // Eliminate need for dynamic_cast
    virtual Package *isPackage() { return nullptr; }
    virtual Module *isModule() { return nullptr; }
    virtual EnumMember *isEnumMember() { return nullptr; }
    virtual TemplateDeclaration *isTemplateDeclaration() { return nullptr; }
    virtual TemplateInstance *isTemplateInstance() { return nullptr; }
    virtual TemplateMixin *isTemplateMixin() { return nullptr; }
    virtual ForwardingAttribDeclaration *isForwardingAttribDeclaration() { return nullptr; }
    virtual Nspace *isNspace() { return nullptr; }
    virtual Declaration *isDeclaration() { return nullptr; }
    virtual StorageClassDeclaration *isStorageClassDeclaration(){ return nullptr; }
    virtual ExpressionDsymbol *isExpressionDsymbol() { return nullptr; }
    virtual AliasAssign *isAliasAssign() { return nullptr; }
    virtual ThisDeclaration *isThisDeclaration() { return nullptr; }
    virtual BitFieldDeclaration *isBitFieldDeclaration() { return nullptr; }
    virtual TypeInfoDeclaration *isTypeInfoDeclaration() { return nullptr; }
    virtual TupleDeclaration *isTupleDeclaration() { return nullptr; }
    virtual AliasDeclaration *isAliasDeclaration() { return nullptr; }
    virtual AggregateDeclaration *isAggregateDeclaration() { return nullptr; }
    virtual FuncDeclaration *isFuncDeclaration() { return nullptr; }
    virtual FuncAliasDeclaration *isFuncAliasDeclaration() { return nullptr; }
    virtual OverDeclaration *isOverDeclaration() { return nullptr; }
    virtual FuncLiteralDeclaration *isFuncLiteralDeclaration() { return nullptr; }
    virtual CtorDeclaration *isCtorDeclaration() { return nullptr; }
    virtual PostBlitDeclaration *isPostBlitDeclaration() { return nullptr; }
    virtual DtorDeclaration *isDtorDeclaration() { return nullptr; }
    virtual StaticCtorDeclaration *isStaticCtorDeclaration() { return nullptr; }
    virtual StaticDtorDeclaration *isStaticDtorDeclaration() { return nullptr; }
    virtual SharedStaticCtorDeclaration *isSharedStaticCtorDeclaration() { return nullptr; }
    virtual SharedStaticDtorDeclaration *isSharedStaticDtorDeclaration() { return nullptr; }
    virtual InvariantDeclaration *isInvariantDeclaration() { return nullptr; }
    virtual UnitTestDeclaration *isUnitTestDeclaration() { return nullptr; }
    virtual NewDeclaration *isNewDeclaration() { return nullptr; }
    virtual VarDeclaration *isVarDeclaration() { return nullptr; }
    virtual VersionSymbol *isVersionSymbol() { return nullptr; }
    virtual DebugSymbol *isDebugSymbol() { return nullptr; }
    virtual ClassDeclaration *isClassDeclaration() { return nullptr; }
    virtual StructDeclaration *isStructDeclaration() { return nullptr; }
    virtual UnionDeclaration *isUnionDeclaration() { return nullptr; }
    virtual InterfaceDeclaration *isInterfaceDeclaration() { return nullptr; }
    virtual ScopeDsymbol *isScopeDsymbol() { return nullptr; }
    virtual ForwardingScopeDsymbol *isForwardingScopeDsymbol() { return nullptr; }
    virtual WithScopeSymbol *isWithScopeSymbol() { return nullptr; }
    virtual ArrayScopeSymbol *isArrayScopeSymbol() { return nullptr; }
    virtual Import *isImport() { return nullptr; }
    virtual EnumDeclaration *isEnumDeclaration() { return nullptr; }
    virtual SymbolDeclaration *isSymbolDeclaration() { return nullptr; }
    virtual AttribDeclaration *isAttribDeclaration() { return nullptr; }
    virtual AnonDeclaration *isAnonDeclaration() { return nullptr; }
    virtual CPPNamespaceDeclaration *isCPPNamespaceDeclaration() { return nullptr; }
    virtual VisibilityDeclaration *isVisibilityDeclaration() { return nullptr; }
    virtual OverloadSet *isOverloadSet() { return nullptr; }
    virtual MixinDeclaration *isMixinDeclaration() { return nullptr; }
    virtual StaticAssert *isStaticAssert() { return nullptr; }
    virtual StaticIfDeclaration *isStaticIfDeclaration() { return nullptr; }
    virtual CAsmDeclaration *isCAsmDeclaration() { return nullptr; }
    void accept(Visitor *v) override { v->visit(this); }
};

// Dsymbol that generates a scope

class ScopeDsymbol : public Dsymbol
{
public:
    Dsymbols *members;          // all Dsymbol's in this scope
    DsymbolTable *symtab;       // members[] sorted into table
    unsigned endlinnum;         // the linnumber of the statement after the scope (0 if unknown)
    Dsymbols *importedScopes;   // imported Dsymbol's
    Visibility::Kind *visibilities;   // array of `Visibility.Kind`, one for each import

private:
    BitArray accessiblePackages, privateAccessiblePackages;

public:
    ScopeDsymbol *syntaxCopy(Dsymbol *s) override;
    virtual void importScope(Dsymbol *s, Visibility visibility);
    virtual bool isPackageAccessible(Package *p, Visibility visibility, SearchOptFlags flags = (SearchOptFlags)SearchOpt::all);
    bool isforwardRef() override final;
    static void multiplyDefined(Loc loc, Dsymbol *s1, Dsymbol *s2);
    const char *kind() const override;
    virtual Dsymbol *symtabInsert(Dsymbol *s);
    virtual Dsymbol *symtabLookup(Dsymbol *s, Identifier *id);
    bool hasStaticCtorOrDtor() override;

    ScopeDsymbol *isScopeDsymbol() override final { return this; }
    void accept(Visitor *v) override { v->visit(this); }
};

// With statement scope

class WithScopeSymbol final : public ScopeDsymbol
{
public:
    WithStatement *withstate;


    WithScopeSymbol *isWithScopeSymbol() override { return this; }
    void accept(Visitor *v) override { v->visit(this); }
};

// Array Index/Slice scope

class ArrayScopeSymbol final : public ScopeDsymbol
{
public:
    RootObject *arrayContent;

    ArrayScopeSymbol *isArrayScopeSymbol() override { return this; }
    void accept(Visitor *v) override { v->visit(this); }
};

// Overload Sets

class OverloadSet final : public Dsymbol
{
public:
    Dsymbols a;         // array of Dsymbols

    void push(Dsymbol *s);
    OverloadSet *isOverloadSet() override { return this; }
    const char *kind() const override;
    void accept(Visitor *v) override { v->visit(this); }
};

// Forwarding ScopeDsymbol

class ForwardingScopeDsymbol final : public ScopeDsymbol
{
public:
    Dsymbol *symtabInsert(Dsymbol *s) override;
    Dsymbol *symtabLookup(Dsymbol *s, Identifier *id) override;
    void importScope(Dsymbol *s, Visibility visibility) override;
    const char *kind() const override;

    ForwardingScopeDsymbol *isForwardingScopeDsymbol() override { return this; }
};

class ExpressionDsymbol final : public Dsymbol
{
public:
    Expression *exp;

    ExpressionDsymbol *isExpressionDsymbol() override { return this; }
};

class CAsmDeclaration final : public Dsymbol
{
public:
    Expression *code;   // string expression

    CAsmDeclaration *isCAsmDeclaration() override { return this; }
    void accept(Visitor *v) override { v->visit(this); }
};

// Table of Dsymbol's

class DsymbolTable final : public RootObject
{
public:
    AA *tab;

    // Look up Identifier. Return Dsymbol if found, NULL if not.
    Dsymbol *lookup(Identifier const * const ident);

    // Look for Dsymbol in table. If there, return it. If not, insert s and return that.
    void update(Dsymbol *s);

    // Insert Dsymbol in table. Return NULL if already there.
    Dsymbol *insert(Dsymbol *s);
    Dsymbol *insert(Identifier const * const ident, Dsymbol *s);     // when ident and s are not the same

    // Number of symbols in symbol table
    size_t length() const;
};

namespace dmd
{
    void addMember(Dsymbol *dsym, Scope *sc, ScopeDsymbol *sds);
    Dsymbol *search(Dsymbol *d, Loc loc, Identifier *ident, SearchOptFlags flags = (SearchOptFlags)SearchOpt::localsOnly);
    Dsymbols *include(Dsymbol *d, Scope *sc);
    void setScope(Dsymbol *d, Scope *sc);
    void importAll(Dsymbol *d, Scope *sc);
    void addComment(Dsymbol *d, const char *comment);
}
