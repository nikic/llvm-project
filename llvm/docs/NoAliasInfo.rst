========================================
Restrict and NoAlias Information in LLVM
========================================

.. contents::
   :local:
   :depth: 2

Introduction
============

LLVM provides a number of mechanisms to annotate that two accesses will not
alias. This document describes the provenance annotations on pointers with the
``noalias`` intrinsics, and ``noalias`` metadata on memory instructions
(load/store).

Together, they provide a means to decide if two memory instructions **will not
alias**, by looking at the pointer provenance and combining this with the active
scopes (specified by the ``noalias`` metadata) of the memory instructions.

All of C99 restrict can be mapped onto these annotations, resulting in a
powerful mechanism to provide extra alias information.


Relation with C99 ``restrict``
==============================

The noalias infrastructure can be used to fully support *C99 restrict* [#R1]_:
restrict pointers as function arguments, as local variables, as struct members.
(See [#R2]_: iso-C99-n1256, 6.7.3.1 'Formal definition of restrict').

Modeling ``restrict`` requires two pieces of information for a memory
instruction:

- the *depends on* relationship of the pointer path on a restrict
  pointer (pointer provenance)
- the *visibility* of that restrict pointer to the memory instruction

Every variable, that contains at least one restrict pointer, that is defined in
a function (function arguments and local variables), will get a noalias scope
that is associate d to this variable declaration. The ``!noalias`` metadata is
used to annotate every memory instruction in the function with the restrict
variables (noalias scopes) that are visible to that memory instruction.

Each restrict variable also gets a ``@llvm.noalias.decl`` at the place in the
control flow where it is defined. This identifies if restrict scopes must be
duplicated or not when loops are transformed.

Whenever a restrict pointer is read, a ``@llvm.noalias`` intrinsic is introduced
to indicate the dependency on a restrict pointer. This intrinsic depends on the
pointer value and on the address of the pointer. Different addresses represent
different restrict pointers. Different restrict pointers point to different sets
of objects.

A struct can contain multiple restrict pointers. As such, a single variable
definition (single scope) can contain multiple restrict pointers. The addresses
of the pointers ensure that they can be differentiated.

When a struct is copied using the internal ``@llvm.memcpy``, the ``@llvm.noalias``
intrinsic cannot be used. The ``@llvm.noalias.copy.guard`` provides information
on what parts of the struct represent restrict pointers. This ensures that the
correct dependencies can be reconstructed when the ``@llvm.memcpy`` is optimized
away.

When a pointer to a restrict pointer is dereferenced, there is no local scope
available. These restrict pointers are associated with an ``unknown function
scope``. It is sometimes possible to refine those scopes later to actual
variable definitions.

.. _noaliasinfo_basic_mechanism:

Basic Mechanism
===============

Describing the full set of intrinsics with all their arguments at once, might be
confusing and can make it difficult to understand. Therefore, we start with
explaining the basic technology. We then build upon it to add missing parts.

Throughout the explanation, C99 and LLVM-IR code fragments are used to provide
examples. This does not mean that the provided infrastructure can only be used
for implementing C99 restrict. It just shows that at least C99 restrict can be
mapped onto it. Other provenance based alias annotations will likely map just as
well onto it, without (or with only small) adaptations to the provided
infrastructure.


Single ``noalias`` Scope
------------------------

Following code fragment:

.. code-block:: C

     void foo(int *pA, int *pB, int i) {
       int * restrict rpA = pA;
       int * pX = &rpA[i];

       *rpA = 42;  // (1) based on rpA
       *pB = 43;   // (2) not based on rpA
       *pX = 44;   // (3) based on rpA
     }

contains one *restrict* pointer ``rpA``, one pointer ``pX`` depending on it, and
one pointer ``pB`` not depending on ``rpA``. Based on the C99 restrict
description, \*rpA and \*pX can alias with each other. They will not alias with
\*pB.

In pseudo LLVM-IR code, this can be represented as:

.. code-block:: llvm

    define void @foo(i32* %pA, i32* %pB, i64 %i) {
      %rpA = tail call i32* @llvm.noalias(i32* %pA, metadata !2)
      %arrayidx = getelementptr inbounds i32, i32* %pA, i64 %i
      store i32 42, i32* %rpA, !noalias !2      ; (1)
      store i32 43, i32* %pB, !noalias !2       ; (2)
      store i32 44, i32* %arrayidx, !noalias !2 ; (3)
      ret void
    }

    ; MetaData
    !2 = !{!3}                                  ; contains a single scope: !3
    !3 = distinct !{!3, !4, !"foo: rpA"}        ; this scope represents rpA
    !4 = distinct !{!4, !"foo"}

* Metadata !2 defines a list of a single scope ``!3`` that represents ``rpA``
* The ``@llvm.noalias`` intrinsic is associated with the single scope ``!3`` in
  ``metadata !2``. It indicates that accesses based on this pointer are depending
  on this ``!3`` scope. They will not alias with accesses *not* depending on the
  same ``!3`` scope, as long as the scope is visible to both accesses.
* For this example, the ``!3`` scope is visible to all three stores (``!noalias
  !2`` annotation on the stores). Because of this:

  * ``(1)`` and ``(3)`` may alias to each other: ``%rpA`` and ``%arrayidx``
    depend on the same ``!3`` scope.
  * ``(1)`` and ``(3)`` will not alias with ``(2)``: ``%pB`` does not depend on
    the ``!3`` scope.


Multiple ``noalias`` Scopes
---------------------------

Let's extend the example:

.. code-block:: C

     void foo(int *pA, int *pB, int *pC, int i) {
       int * restrict rpA = pA;
       int * pX = &rpA[i];

       *rpA = 42;  // (1) based on rpA
       *pB = 43;   // (2) not based on rpA
       *pX = 44;   // (3) based on rpA
       {
         int * restrict rpC = pC;
         // rpA and rpC visible

         *rpA = 45; // (4) based on rpA
         *pB = 46;  // (5) not based on rpA nor rpC
         *rpC = 47; // (6) based on rpC
       }
     }

with following pseudo LLVM-IR code:

.. code-block:: llvm

    define void @foo(i32* %pA, i32* %pB, i32* %pC, i64 %i) {
      %rpA = tail call i32* @llvm.noalias(i32* %pA, metadata !2)   ; rpA
      %arrayidx = getelementptr inbounds i32, i32* %pA, i64 %i
      %rpC = tail call i32* @llvm.noalias(i32* %pC, metadata !11)  ; rpC
      store i32 42, i32* %rpA, !noalias !2      ; (1)  rpA
      store i32 43, i32* %pB, !noalias !2       ; (2)  rpA
      store i32 44, i32* %arrayidx, !noalias !2 ; (3)  rpA
      store i32 45, i32* %rpA, !noalias !13     ; (4)  rpA and rpC
      store i32 46, i32* %pB, !noalias !13      ; (5)  rpA and rpC
      store i32 47, i32* %rpC, !noalias !13     ; (6)  rpA and rpC
      ret void
    }

    ; MetaData
    !2 = !{!3}                                        ; single scope: rpA
    !3 = distinct !{!3, !4, !"foo: rpA"}
    !4 = distinct !{!4, !"foo"}
    !11 = !{!12}                                      ; single scope: rpC
    !12 = distinct !{!12, !4, !"foo: rpC"}
    !13 = !{!12, !3}                                  ; scopes: rpA and rpC

In this fragment:

* ``%rpA`` is associated with scope ``!3``
* ``%rpC`` is associated with scope ``!12``
* ``(1)``, ``(2)`` and ``(3)`` only see ``rpA``. (scope ``!3``)
* ``(4)``, ``(5)`` and ``(6)`` see ``rpA`` and ``rpC`` (scopes ``!3`` and ``!12``)

Following C99 restrict:

* ``(4)``, ``(5)`` and ``(6)`` will not alias each other.
* ``(6)`` will not alias ``(3)``:

  * ``(6)`` is based on ``rpC``, which is visible to ``(6)``, but not to
    ``(3)`` => no conclusion.
  * ``(3)`` is based on ``rpA`` which is visible to both ``(6)`` and ``(3)`` =>
    will not alias

* ``(6)`` might alias with ``(2)``:

  * ``rpC`` is visible to ``(6)``, but not to ``(2)``.
  * There are no other dependencies for those accesses.


Location of the ``restrict`` Declaration
----------------------------------------

Some optimization passes need to know where a restrict variable has been
declared. Only when that information is known, they can perform the correct
transformations.

One of those transformations is *loop unrolling*. When restrict is applicable
across iterations, the loop can be unrolled without extra changes. But when
restrict is only applicable inside a single iteration, care must be taken to
also duplicate the noalias scopes while duplicating the loop body.

Following code example shows those two cases:

.. code-block:: c

    void restrictInLoop(int *pA, int *pB, int *pC, long N) {
      for (int i=0; i<N; ++i) {
        // stores can be reordered inside a single iterator, but not across
        // iterations
        int * restrict rpA = pA;
        int * restrict rpB = pB;
        rpB[i] = 2*pC[i];
        rpA[i] = 3*pC[i];
      }
    }

    void restrictOutOfLoop(int *pA, int *pB, int *pC, long N) {
      // stores through rpA and rpB will never alias and can be reordered,
      int * restrict rpA = pA;
      int * restrict rpB = pB;
      for (int i=0; i<N; ++i) {
        rpB[i] = 2*pC[i];
        rpA[i] = 3*pC[i];
      }
    }

The ``@llvm.noalias.decl`` intrinsic is used to track where in the control flow a
restrict variable was introduced. When it is found inside a loop body, it
indicates that the associated *noalias scope* must be duplicated during loop
unrolling.

For the example, the corresponding pieces of LLVM-IR look like:

.. code-block:: llvm

    define void @restrictInLoop(i32* %pA, i32* %pB, i32* %pC, i64 %N) {
    entry:
      %cmp18 = icmp sgt i64 %N, 0
      br i1 %cmp18, label %for.body, label %for.cond.cleanup

    for.body:                                         ; preds = %entry, %for.body
      %indvars.iv = phi i64 [ %indvars.iv.next, %for.body ], [ 0, %entry ]
      %0 = call i8* @llvm.noalias.decl(i32** null, i64 0, metadata !2) ; rpA - inside the loop
      %1 = call i8* @llvm.noalias.decl(i32** null, i64 0, metadata !5) ; rpB - inside the loop
    ...

and

.. code-block:: llvm

    define void @restrictOutOfLoop(i32* %pA, i32* %pB, i32* %pC, i64 %N) {
    entry:
      %0 = call i8* @llvm.noalias.decl(i32** null, i64 0, metadata !16) ; rpA - outside the loop
      %1 = call i8* @llvm.noalias.decl(i32** null, i64 0, metadata !19) ; rpB - outside the loop
      %cmp18 = icmp sgt i64 %N, 0
      br i1 %cmp18, label %for.body.lr.ph, label %for.cond.cleanup
    ...

Note: the ``restrictInLoop`` situation is something that can easily happen after
inlining a function with ``restrict`` arguments:

.. code-block:: C

    void doCompute(int * restrict rpA, int * restrict rpB, int * pC, long i) {
      rpB[i] = 2*pC[i];
      rpA[i] = 3*pC[i];
    }

    void restrictInLoop(int *pA, int *pB, int *pC, long N) {
      for (int i=0; i<N; ++i) {
        // stores can be reordered inside a single iterator, but not across
        // iterations
        doCompute(pA, pB, pC, i);
      }
    }

Provenance Based Alias Annotations
==================================

In principle, the two intrinsics we have seen so far, should be enough to
provide all necessary information. Now that the basic mechanism has been
explained, we can focus on the various arguments and extensions and why they are
needed.


The Basic Infrastructure
------------------------

In ``C99 restrict``, restrictness is associated with ``object P`` [#R2]_. It is
introduced when the pointer value is read from ``object P``. Different ``object
P`` point to different sets of objects. Because of this, the declaration of a
variable that contains multiple restrict pointers (like an array of restrict
pointers, or a struct that has multiple restrict member pointers) will result in
a single ``scope`` that contains multiple ``object P``.

* ``@llvm.noalias.decl %p.alloc, metadata !Scope``

  * indicates at what location in the control flow a restrict pointer has been
    declared.
  * ``%p.alloc`` refers to the ``alloca`` associated with the declaration.
  * ``!Scope`` metadata refers to the unique scope, associated with this
    declaration.
  * Note: the ``@llvm.noalias.decl`` intrinsic can normally not be moved outside
    loops. Its purpose is to identify the freedom that a restrict pointer has
    with respect to loop bodies.

* ``@llvm.noalias %p, %p.decl, %p.addr``

  * introduces ``noalias`` information to the instructions that (directly or
    indirectly) depend on this intrinsic. It is created when *reading a restrict
    pointer* and is used to track the 'based-on' relationship.
  * ``%p`` is the pointer value that was read. This is also the value that is
    returned by this intrinsic.
  * ``%p.decl`` refers to the ``@llvm.noalias.decl`` that is associated with
    this restrict pointer.
  * ``p.addr`` represents the address of ``object P``.
  * Note: sometimes the declaration is not known upfront. In that case,
    ``%p.decl`` is ``null``. After inlining and /or optimizations, it can be
    possible to infer the ``llvm.noalias.decl``.

* the tuple < ``%p.addr``, ``!Scope`` > defines the ``object P``.

Example A:

.. code-block:: C

    int foo(int* pA, int* pB) {
      int * restrict rpA=pA;
      *rpA=42;
      *pB=99;
      return *rpA;
    }

And in pseudo LLVM-IR as how clang would produce it:

.. code-block:: llvm

    define i32 @foo(i32* %pA, i32* %pB) {
      %rpA.address = alloca i32*
      %rpA.decl = call @llvm.noalias.decl %rpA.address, !metadata !10 ; declaration of a restrict pointer
      store i32* %pA, i32** %rpA.address, !noalias !10
      %rpA = load i32*, i32** %rpA.address, !noalias !10
      %rpA.1 = i32* call @llvm.noalias %rpA, %rpA.decl, %rpA.address ; reading of a restrict pointer
      store i32 42, i32* %rpA.1, !noalias !10
      store i32 99, i32* %pB, !noalias !10
      %1 = load i32, i32* %rpA.1, !noalias !10
      ret i32 %1
    }

With this representation, we have enough information to decide whether two
load/stores are not aliasing, based on the ``noalias`` annotations. But, the
added intrinsics must block optimizations. Later on we will see how the
infrastructure is expanded to allow for optimizations.

Summary:

* ``%p.decl = @llvm.noalias.decl %p.alloc, metadata !Scope``
* ``%p.val = @llvm.noalias %p, %p.decl, %p.addr``


Pointer Provenance
------------------

In order to keep track of the dependency on the ``@llvm.noalias`` intrinsics,
but still allow most optimization passes to do their work, an extra optional
operand for ``load``/``store`` instruction is introduced: the ``ptr_provenance``
operand.

The idea is that the *pointer operand* is used for normal pointer
computations. The ``ptr_provenance`` operand is used to track ``noalias``
related dependencies. Optimizations (like LSR) can modify the *pointer operand*
as they see fit. As long as the ``ptr_provenance operand`` is not touched, we
are still able to deduce the noalias related information.

When an optimization introduces a ``load``/``store`` without keeping the
``ptr_provenance`` operand and the ``!noalias`` metadata, we fall back to the
fail-safe *worst case*.

Although the actual pointer computations can be removed from the
``ptr_provenance``, it can still contain *PHI* nodes, *select* instructions and
*casts*.

For clang, it is hard to track the usage of a pointer and it will not generate
the ``ptr_provenance`` operand. At LLVM-IR level, this is much easier. Because
of that the annotations exist in two states and a conversion pass is introduced:

* Before *noalias propagation*:

  This state is produced by clang and sometimes by SROA. The ``@llvm.noalias``
  intrinsic is used in the computation path of the pointer. It is treated as a
  mostly opaque intrinsic and blocks most optimizations.


* After *noalias propagation*:

  A *noalias propagation and conversion* pass is introduced:

  * ``@llvm.noalias`` intrinsics are converted into ``@llvm.provenance.noalias``
    intrinsics.
  * their usage is removed from the main pointer computations of
    ``load``/``store`` instructions and moved to the ``ptr_provenance`` operand.
  * When a pointer depending on a ``@llvm.noalias`` intrinsic is passed as an
    argument, returned from a function or stored into memory, a
    ``@llvm.noalias.arg.guard`` is introduced.  This combines the original
    pointer computation with the provenance information. After inlining, it is
    also used to propagate the noalias information to the ``load``/``store``
    instructions.

So, we now have two extra intrinsics:

* ``@llvm.provenance.noalias`` %prov.p, %p.decl, %p.addr

  * provides restrict information to a ``ptr_provenance`` operand

  * ``%prov.p``: tracks the provenance information associated with the pointer
    value that was read.
  * ``%p.decl`` refers to the ``@llvm.noalias.decl`` that is associated with the
    restrict pointer.
  * ``%p.addr``: represents the address of ``object P``.

* ``@llvm.noalias.arg.guard %p, %prov.p``

  * combines pointer and ``ptr_provenance`` information when a pointer value
    with ``noalias`` dependencies escapes. It is normally used for function
    arguments, returns, or stores to memory.
  * ``%p`` tracks the pointer computation
  * ``%prov.p`` tracks the provenance of the pointer.

After noalias propagation and conversion, example A becomes:

.. code-block:: llvm

    define i32 @foo(i32* %pA, i32* %pB) {
      %rpA.address = alloca i32*
      %rpA.decl = i8* call @llvm.noalias.decl i32* %rpA.address, !metadata !10 ; declaration of a restrict pointer
      store i32* %pA, i32** %rpA.address, !noalias !10
      %rpA = load i32*, i32** %rpA.address, !noalias !10
      ; reading of a restrict pointer:
      %prov.rpA.1 = i32* call @llvm.provenance.noalias i32* %rpA, i8* %rpA.decl, i32* %rpA.address
      store i32 42, i32* %rpA, ptr_provenance i32* %prov.rpA.1, !noalias !10
      store i32 99, i32* %pB, !noalias !10
      %1 = load i32, i32* %rpA.1, !noalias !10
      ret i32 %1
    }

Summary:

* ``%p.decl = @llvm.noalias.decl %p.alloc, metadata !Scope``
* ``%p.noalias = @llvm.noalias %p, %p.decl, %p.addr``
* ``%prov.p = @llvm.provenance.noalias %prov.p.2, %p.decl, %p.addr``
* ``%p.guard = @llvm.noalias.arg.guard %p, %prov.p``


.. _noalias_vs_provenance_noalias:

``@llvm.noalias`` vs ``@llvm.provenance.noalias``
-------------------------------------------------

The ``@llvm.noalias`` intrinsic is a convenience shortcut for the combination of
``@llvm.provenance.noalias``, which can only reside on the ptr_provenance path,
and ``@llvm.noalias.arg.guard``, which combines the normal pointer with the
ptr_provenance path:

* This results in less initial code to be generated by ``clang``.
* It also helps during SROA when introducing ``noalias`` information for pointers
  inside a struct.
* The noalias propagation and conversion pass depends on the property of
  ``@llvm.provenance.noalias`` to only reside on the ``ptr_provenance`` path to
  reduce the amount of work.

.. code-block:: llvm

      ; Following:
      %rpA = load i32*, i32** %rpA.address, !noalias !10
      %rpA.1 = i32* call @llvm.noalias %rpA, %rpA.decl, %rpA.address
      store i32 42, i32* %rpA.1, !noalias !10

      ; is a shortcut for:
      %rpA = load i32*, i32** %rpA.address, !noalias !10
      %rpA.prov = i32* call @llvm.provenance.noalias %rpA, %rpA.decl, %rpA.address
      %rpA.guard = i32* call @llvm.noalias.arg.guard %rpA, %rpA.prov
      store i32 42, i32* %rpA.guard, !noalias !10

      ; and after noalias propagation and conversion, this becomes:
      %rpA = load i32*, i32** %rpA.address, !noalias !10
      %prov.rpA = i32* call @llvm.provenance.noalias %rpA, %rpA.decl, %rpA.address
      store i32 42, i32* %rpA, ptr_provenance i32* %prov.rpA, !noalias !10



SROA and Stack optimizations
----------------------------

When SROA eliminates a local variable, we do not have an address for ``object P``
anymore (the alloca is removed and ``%p.addr`` becomes ``null``). At that moment
we can only depend on the ``!Scope`` metadata to differentiate restrict
objects. For convenience, we also add this information to the ``@llvm.noalias``
and ``@llvm.provenance.noalias`` intrinsics.

It is also possible that a single variable declaration contains multiple
restrict pointers (think of a struct containing multiple restrict pointers, or
an array of restrict pointers). For correctness, SROA must introduce new scopes
when splitting it up. But cloning and adapting scopes can be very
expensive. Because of that, we introduce an extra *object ID* (``objId``)
parameter for ``@llvm.noalias.decl``, ``@llvm.noalias`` and
``llvm.provenance.noalias``. This can be thought of as the *offset in the
variable*. This allows us to differentiate *noalias* dependencies coming from
the same variable, but representing different *noalias* pointers.

Summary:

* ``%p.decl = @llvm.noalias.decl %p.alloc, i64 objId, metadata !Scope``
* ``%p.noalias = @llvm.noalias %p, %p.decl, %p.addr, i64 objId, metadata !Scope``
* ``%prov.p = @llvm.provenance.noalias %prov.p.2, %p.decl, %p.addr, i64 objId, metadata !Scope``
* ``%p.guard = @llvm.noalias.arg.guard %p, %prov.p``

For alias analysis, this means that two ``@llvm.provenance.noalias`` intrinsics represent a
different ``object P0`` and, ``object P1``, if:

* ``%p0.addr`` and ``%p1.addr`` are different
* or, ``objId0`` and ``objId1`` are different
* or, ``!Scope0`` and ``!Scope1`` are different


Optimizing a restrict pointer pointing to a restrict pointer
------------------------------------------------------------

Example:

.. code-block:: C

    int * restrict * restrict ppA = ...;
    int * restrict * restrict ppB = ...;

    **ppA=42;
    **ppB=99;
    return **ppA; // according to C99, 6.7.3.1 paragraph 4, **ppA and **ppB are not aliasing

In order to allow this optimization, we also need to track the ``!noalias`` scope
when the ``@llvm.noalias`` intrinsic is introduced.  The ``%p.addr`` parameter in the
``@llvm.provenance.noalias`` version will also get a ``ptr_provenance`` operand,
through the ``%prov.p.addr`` argument.

In short, the ``@llvm.noalias`` and ``@llvm.provenance.noalias`` intrinsics are
treated as if they are a memory operation.

Summary:

* ``%p.decl = @llvm.noalias.decl %p.alloc, i64 objId, metadata !Scope``
* ``%p.noalias = @llvm.noalias %p, %p.decl, %p.addr, i64 objId, metadata !Scope, !noalias !VisibleScopes``
* ``%prov.p = @llvm.provenance.noalias %prov.p.2, %p.decl, %p.addr, %prov.p.addr, i64 objId, metadata !Scope, !noalias !VisibleScopes``
* ``%p.guard = @llvm.noalias.arg.guard %p, %prov.p``

For alias analysis, this means that two ``@llvm.provenance.noalias`` intrinsics represent a
different ``object P0`` and ``object P1`` if:

* ``%p0.addr`` and ``%p1.addr`` are different
* or, ``objId0`` and ``objId1`` are different
* or, ``!Scope0`` and ``!Scope1`` are different
* or we can prove that { ``%p0.addr``, ``%prov.p0.addr``, ``!VisibleScopes0`` } and
  { ``%p1.addr``, ``%prov.p1.addr``, ``!VisibleScopes1`` } do not alias for both
  intrinsics. (As if we treat each of the two ``@llvm.provenance.noalias`` as a
  **store to ``%p.addr``**  and we must prove that the two stores do not alias;
  also see [#R8]_, question 2)


``Unknown function`` Scope
--------------------------

When the declaration of a restrict pointer is not visible, *C99, 6.7.3.1
paragraph 2*, says that the pointer is assumed to start living from ``main``.

This case can be handled by the ``unknown function`` scope, which is annotated
to the function itself. This can be treated as saying: the scope of this restrict
pointer starts somewhere outside this function. In such case, the
``@llvm.noalias`` and ``@llvm.provenance.noalias`` will not be associated with a
``@llvm.noalias.decl``. It is possible that after inlining, the scopes can be
refined to a declaration which became visible.

For convenience, each function can have its own ``unknown function`` scope
specified by a ``noalias !UnknownScope`` metadata attribute on the function itself.


Aggregate Copies
----------------

Restrictness is introduced by *reading a restrict pointer*. It is not always
possible to add the necessary ``@llvm.noalias`` annotation when this is done. An
aggregate containing one or more restrict pointers can be copied with a single
``load``/``store` pair or a ``@llvm.memcpy``. This makes it hard to track when a
restrict pointer is copied over. As long as this is treated as an memory escape,
there is no issue. At the moment that the copy is optimized away, we must be
able to reconstruct the ``noalias`` dependencies for correctness.

For this, a final intrinsic is introduced: ``@llvm.noalias.copy.guard``:

* ``@llvm.noalias.copy.guard %p.addr, %p.decl, metadata !Indices, metadata !Scope``

  * Guards a ``%p.addr`` object that is copied as a single aggregate or ``@llvm.memcpy``
  * ``%p.addr``: the object to guard
  * ``%p.decl``: (when available), the ``@llvm.noalias.decl`` associated with the object
  *  ``!Indices``: this refers to a metadata list. Each element of the list
     refers to a set of indices where a restrict pointer is located, similar to
     the indices for a ``getelementptr``.
  * ``!Scope``: the declaration scope of ``%p.decl``

This information allows *SROA* to introduce the needed ``@llvm.noalias`` intrinsics
when a struct is split up.

Summary:

* potential ``!noalias !UnknownScope`` annotation at function level
* ``%p.decl = @llvm.noalias.decl %p.alloc, i64 objId, metadata !Scope``
* ``%p.noalias = @llvm.noalias %p, %p.decl, %p.addr, i64 objId, metadata !Scope, !noalias !VisibleScopes``
* ``%prov.p = @llvm.provenance.noalias %prov.p.2, %p.decl, %p.addr, %prov.p.addr, i64 objId, metadata !Scope, !noalias !VisibleScopes``
* ``%p.guard = @llvm.noalias.arg.guard %p, %prov.p``
* ``%p.addr.guard = @llvm.noalias.copy.guard %p.addr, %p.decl, metadata !Indices, metadata !Scope, !noalias !VisibleScopes``

Optimization passes
-------------------

For correctness, some optimization passes must be aware of the *noalias intrinsics*:
inlining [#R7]_, unrolling [#R6]_, loop rotation, ...  Whenever a body is duplicated that
contains a ``@llvm.noalias.decl``, it must be decided how that duplication must be done.
Sometimes new unique scopes must be introduced, sometimes not.

Other optimization passes can perform better by knowing about the ``ptr_provenance``: when
new ``load``/``store`` instructions are introduced, adding ``ptr_provenance``
information can result in better alias analysis for those instructions.

It is possible that an optimization pass is doing a wrong optimization, by doing
a transformation that omits the ``ptr_provenance`` operand, but keeps the
``!noalias`` information. This can happen when the ``!noalias`` metadata is
copied directly, instead of using ``AAMetadata`` and
``getAAMetadata/setAAMetadata``:

.. code-block:: C

    AAMDNodes AAMD;
    OldLoad->getAAMetadata(AAMD);
    NewLoad->setAAMetadata(AAMD);

    // only do this if it is safe to copy over the 'ptr_provenance' info
    // The !noalias info will then also be copied over
    NewLoad->setAAMetadataNoAliasProvenance(AAMD);

Possible Future Enhancements
----------------------------

* c++ alias_set

With this framework in place, it should be easy to extend it to support the
*alias_set* proposal [#R3]_. This can be done by tracking a separate *universe
object*, instead of *object P*.


Detailed Description
====================

This section gives a detailed description of the various intrinsics and
metadata.

``!noalias`` Scope Metadata
---------------------------

The ``!noalias`` metadata consists of a *list of scopes*. Each scope is also
associated to the function to which it belongs.

.. code-block:: llvm

    ; MetaData
    !2 = !{!3}                                ; single scope: rpA
    !3 = distinct !{!3, !4, !"foo: rpA"}      ; variable 'rpA'
    !4 = distinct !{!4, !"foo"}               ; function 'foo'
    !5 = !{!6}
    !6 = distinct !{!6, !7, !"foo: unknown scope"}
    !7 = distinct !{!7, !"foo"}
    !11 = !{!12}                              ; single scope: rpC
    !12 = distinct !{!12, !4, !"foo: rpC"}    ; variable 'rpC'
    !13 = !{!12, !3}                          ; multiple scopes: rpA and rpC

This structure is used in following places:

* as a single scope:

  * used as *metavalue* argument by ``@llvm.noalias.decl``, ``@llvm.noalias``,
    ``@llvm.provenance.noalias``, ``@llvm.noalias.copy.guard``. (``!2, !11``) to
    describe the scope that is associated with the noalias intrinsic.
  * used as ``!noalias`` metadata on a function to describe the ``unknown
    function scope``. (``!5``)

* as one or more scopes:

  * used as ``!noalias`` metadata describingthe *visible scopes* on memory
    instructions (``load``/``store``) and ``@llvm.noalias`` and
    ``@llvm.provenance.noalias`` intrinsics.

.. note:: The ``Unknown Function Scope`` is a special scope that is attached
          through ``!noalias`` metadata on a function defintion. It identifies
          the scope that is used for *noalias* pointers for which the
          declaration is not known.


``ptr_provenance`` path
-----------------------

The ``ptr_provenance`` path is reserved for tracking *noalias* information that
is associated to pointers. Value computations should be omitted as much as
possible.

For memory instructions, this means that the actual pointer value and the
provenance information can be separated. This allows optimization passes to
rewrite the pointer computation and still keep the correct provenance information.

A ``ptr_provenance`` path normally starts:

* with the ``ptr_provenance`` operand of a ``load``/``store`` instruction
* with the ``ptr_provenance`` operand of the ``@llvm.noalias.arg.guard``
  intrinsic
* with the ``ptr.provenance`` operand of the ``@llvm.provenance.noalias``
  intrinsic

As the ``@llvm.provenance.noalias``, can only be part of a ``ptr_provenance``
path, its ``%p`` operand is also part of the ``ptr_provenance`` path.

Although all uses of a ``@llvm.provenance.noalias`` must be on a
``ptr_provenance`` path, following the *based on* path must end at a normal
pointer value. This can for example be the input argument of a
function. Optimizations like inlining can provide extra information for such a
pointer.

Examples
--------

This section contains some examples that are used in the description of the
intrinsics.

.. _noaliasinfo_local_restrict:

Example A: local restrict
"""""""""""""""""""""""""

.. _noaliasinfo_local_restrict_C:

C99 code with local restrict variables:

.. code-block:: C

   int foo(int * pA, int i, int *pC) {
     int * restrict rpA = pA;
     int * restrict rpB = pA+i;

     // The three accesses are promised to not alias each other
     *rpA = 10;
     *rpB = 20;
     *pC = 30;

     return *rpA+*rpB+*pC;
   }

.. _noaliasinfo_local_restrict_llvm_0:

LLVM-IR code as produced by clang:

.. code-block:: llvm

    ; Function Attrs: nounwind
    define dso_local i32 @foo(i32* %pA, i32 %i, i32* %pC) #0 {
    entry:
      %pA.addr = alloca i32*, align 4
      %i.addr = alloca i32, align 4
      %pC.addr = alloca i32*, align 4
      %rpA = alloca i32*, align 4
      %rpB = alloca i32*, align 4
      store i32* %pA, i32** %pA.addr, align 4, !tbaa !3, !noalias !7
      store i32 %i, i32* %i.addr, align 4, !tbaa !11, !noalias !7
      store i32* %pC, i32** %pC.addr, align 4, !tbaa !3, !noalias !7
      %0 = bitcast i32** %rpA to i8*
      call void @llvm.lifetime.start.p0i8(i64 4, i8* %0) #4, !noalias !7
      %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** %rpA, i64 0, metadata !13), !noalias !7
      %2 = load i32*, i32** %pA.addr, align 4, !tbaa !3, !noalias !7
      store i32* %2, i32** %rpA, align 4, !tbaa !3, !noalias !7
      %3 = bitcast i32** %rpB to i8*
      call void @llvm.lifetime.start.p0i8(i64 4, i8* %3) #4, !noalias !7
      %4 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** %rpB, i64 0, metadata !14), !noalias !7
      %5 = load i32*, i32** %pA.addr, align 4, !tbaa !3, !noalias !7
      %6 = load i32, i32* %i.addr, align 4, !tbaa !11, !noalias !7
      %add.ptr = getelementptr inbounds i32, i32* %5, i32 %6
      store i32* %add.ptr, i32** %rpB, align 4, !tbaa !3, !noalias !7
      %7 = load i32*, i32** %rpA, align 4, !tbaa !3, !noalias !7
      %8 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %7, i8* %1, i32** %rpA, i64 0, metadata !13),
                                                                                           !tbaa !3, !noalias !7
      store i32 10, i32* %8, align 4, !tbaa !11, !noalias !7
      %9 = load i32*, i32** %rpB, align 4, !tbaa !3, !noalias !7
      %10 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %9, i8* %4, i32** %rpB, i64 0, metadata !14),
                                                                                           !tbaa !3, !noalias !7
      store i32 20, i32* %10, align 4, !tbaa !11, !noalias !7
      %11 = load i32*, i32** %pC.addr, align 4, !tbaa !3, !noalias !7
      store i32 30, i32* %11, align 4, !tbaa !11, !noalias !7
      %12 = load i32*, i32** %rpA, align 4, !tbaa !3, !noalias !7
      %13 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %12, i8* %1, i32** %rpA, i64 0, metadata !13),
                                                                                           !tbaa !3, !noalias !7
      %14 = load i32, i32* %13, align 4, !tbaa !11, !noalias !7
      %15 = load i32*, i32** %rpB, align 4, !tbaa !3, !noalias !7
      %16 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %15, i8* %4, i32** %rpB, i64 0, metadata !14),
                                                                                           !tbaa !3, !noalias !7
      %17 = load i32, i32* %16, align 4, !tbaa !11, !noalias !7
      %add = add nsw i32 %14, %17
      %18 = load i32*, i32** %pC.addr, align 4, !tbaa !3, !noalias !7
      %19 = load i32, i32* %18, align 4, !tbaa !11, !noalias !7
      %add1 = add nsw i32 %add, %19
      %20 = bitcast i32** %rpB to i8*
      call void @llvm.lifetime.end.p0i8(i64 4, i8* %20) #4
      %21 = bitcast i32** %rpA to i8*
      call void @llvm.lifetime.end.p0i8(i64 4, i8* %21) #4
      ret i32 %add1
    }

    ; ....

    !7 = !{!15, !17}
    !13 = !{!15}
    !14 = !{!17}
    !15 = distinct !{!15, !16, !"foo: rpA"}
    !16 = distinct !{!16, !"foo"}
    !17 = distinct !{!17, !16, !"foo: rpB"}

.. _noaliasinfo_local_restrict_llvm_1:

LLVM-IR code during optimization: stack objects have already been optimized
away, ``@llvm.noalias`` has been converted into ``@llvm.provenance.noalias`` and
propagated to the ``ptr_provenance`` path.

.. code-block:: llvm

    ; Function Attrs: nounwind
    define dso_local i32 @foo(i32* %pA, i32 %i, i32* %pC) #0 {
    entry:
      %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !3)
      %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !6)
      %add.ptr = getelementptr inbounds i32, i32* %pA, i32 %i
      %2 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %pA, i8* %0,
                                           i32** null, i32** undef, i64 0, metadata !3), !tbaa !8, !noalias !12
      store i32 10, i32* %pA, ptr_provenance i32* %2, align 4, !tbaa !13, !noalias !12
      %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %add.ptr, i8* %1,
                                           i32** null, i32** undef, i64 0, metadata !6), !tbaa !8, !noalias !12
      store i32 20, i32* %add.ptr, ptr_provenance i32* %3, align 4, !tbaa !13, !noalias !12
      store i32 30, i32* %pC, align 4, !tbaa !13, !noalias !12
      %4 = load i32, i32* %pA, ptr_provenance i32* %2, align 4, !tbaa !13, !noalias !12
      %5 = load i32, i32* %add.ptr, ptr_provenance i32* %3, align 4, !tbaa !13, !noalias !12
      %add = add nsw i32 %4, %5
      %add1 = add nsw i32 %add, 30
      ret i32 %add1
    }

    ; ...

    !3 = !{!4}
    !4 = distinct !{!4, !5, !"foo: rpA"}
    !5 = distinct !{!5, !"foo"}
    !6 = !{!7}
    !7 = distinct !{!7, !5, !"foo: rpB"}
    !8 = !{!9, !9, i64 0}
    !12 = !{!4, !7}

.. _noaliasinfo_local_restrict_llvm_2:

And LLVM-IR code after optimizations: alias analysis found the the stores do not
alias to each other and the values have been propagated.

.. code-block:: llvm

    ; Function Attrs: nounwind
    define dso_local i32 @foo(i32* nocapture %pA, i32 %i, i32* nocapture %pC) local_unnamed_addr #0 {
    entry:
      %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !3)
      %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !6)
      %add.ptr = getelementptr inbounds i32, i32* %pA, i32 %i
      %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %pA, i8* %0,
                                       i32** null, i32** undef, i64 0, metadata !3), !tbaa !8, !noalias !12
      store i32 10, i32* %pA, ptr_provenance i32* %2, align 4, !tbaa !13, !noalias !12
      %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* nonnull %add.ptr, i8* %1,
                                       i32** null, i32** undef, i64 0, metadata !6), !tbaa !8, !noalias !12
      store i32 20, i32* %add.ptr, ptr_provenance i32* %3, align 4, !tbaa !13, !noalias !12
      store i32 30, i32* %pC, align 4, !tbaa !13, !noalias !12
      ret i32 60
    }

    ; ....

    !3 = !{!4}
    !4 = distinct !{!4, !5, !"foo: rpA"}
    !5 = distinct !{!5, !"foo"}
    !6 = !{!7}
    !7 = distinct !{!7, !5, !"foo: rpB"}

    !12 = !{!4, !7}

.. _noaliasinfo_pass_restrict:

Example B: pass a restrict pointer
""""""""""""""""""""""""""""""""""

.. _noaliasinfo_pass_restrict_C:

C99 code with local restrict variables:

.. code-block:: C

    int fum(int * p);

    int foo(int * pA) {
       int * restrict rpA = pA;
       *rpA = 10;

       return fum(rpA);
     }


.. _noaliasinfo_pass_restrict_llvm_0:

LLVM-IR code as produced by clang:

.. code-block:: llvm

    ; Function Attrs: nounwind
    define dso_local i32 @foo(i32* %pA) #0 {
    entry:
      %pA.addr = alloca i32*, align 4
      %rpA = alloca i32*, align 4
      store i32* %pA, i32** %pA.addr, align 4, !tbaa !3, !noalias !7
      %0 = bitcast i32** %rpA to i8*
      call void @llvm.lifetime.start.p0i8(i64 4, i8* %0) #5, !noalias !7
      %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** %rpA, i64 0, metadata !7), !noalias !7
      %2 = load i32*, i32** %pA.addr, align 4, !tbaa !3, !noalias !7
      store i32* %2, i32** %rpA, align 4, !tbaa !3, !noalias !7
      %3 = load i32*, i32** %rpA, align 4, !tbaa !3, !noalias !7
      %4 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %3, i8* %1, i32** %rpA, i64 0, metadata !7),
                                                                                        !tbaa !3, !noalias !7
      store i32 10, i32* %4, align 4, !tbaa !10, !noalias !7
      %5 = load i32*, i32** %rpA, align 4, !tbaa !3, !noalias !7
      %6 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %5, i8* %1, i32** %rpA, i64 0, metadata !7),
                                                                                        !tbaa !3, !noalias !7
      %call = call i32 @fum(i32* %6), !noalias !7
      %7 = bitcast i32** %rpA to i8*
      call void @llvm.lifetime.end.p0i8(i64 4, i8* %7) #5
      ret i32 %call
    }


.. _noaliasinfo_pass_restrict_llvm_1:

And LLVM-IR code after optimizations: stack objects have been optimized
away; ``@llvm.noalias`` has been converted into ``@llvm.provenance.noalias`` and
propagated to the ``ptr_provenance`` path. A ``@llvm.noalias.arg.guard`` has
been introduced to combine the ``ptr_provenance`` and the pointer value before
passing it to ``@fum``.

.. code-block:: llvm

    ; Function Attrs: nounwind
    define dso_local i32 @foo(i32* %pA) local_unnamed_addr #0 {
    entry:
      %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !3)
      %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %pA, i8* %0,
                                        i32** null, i32** undef, i64 0, metadata !3), !tbaa !6, !noalias !3
      store i32 10, i32* %pA, ptr_provenance i32* %1, align 4, !tbaa !10, !noalias !3
      %.guard.guard.guard.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* nonnull %pA, i32* %1)
      %call = tail call i32 @fum(i32* nonnull %.guard.guard.guard.guard) #4, !noalias !3
      ret i32 %call
    }

``@llvm.noalias.decl`` Intrinsic
--------------------------------

Syntax:
"""""""

.. code-block:: llvm

    %p.decl =
        i8* call @llvm.noalias.decl
             T* %p.alloca, i64 objId, metadata !Scope


Overview:
"""""""""

Identify where in the control flow a *noalias* declaration happened.

Arguments:
""""""""""

* ``%p.alloca``: points to the ``alloca`` to which this declaration is
  associated. Or ``null`` when the ``alloca`` was optimized away.
* ``objId``: an ID that is associated to this declaration. *SROA* treats this as
  an offset wrt to the original ``alloca``.
* ``!Scope``: a single scope that is associated with this declaration.

Semantics:
""""""""""

Identify where in the control flow a *noalias* declaration happened. When this
intrinsic is duplicated, care must be taken to decide if the associated
``!Scope`` metadata must be duplicated as well (in case of loop unrolling) or
not (in case of code hoisting over then/else paths).

The function returns a handle to the *noalias* declaration.

Examples:
=========
See :ref:`Example A: local restrict<noaliasinfo_local_restrict>` and
:ref:`Example B: pass a restrict pointer<noaliasinfo_pass_restrict>`.


``@llvm.noalias`` Intrinsic
---------------------------

Syntax:
"""""""

.. code-block:: llvm

    %p.noalias =
        T* call @llvm.noalias
            T* %p, i8* %p.decl,
            T** %p.addr, i64 objId, metadata !Scope,
            !noalias !VisibleScopes

Overview:
"""""""""

Adds *noalias* provenance information to a pointer.

Arguments:
""""""""""

* ``%p``: the original value of the pointer.
* ``%p.decl``: the associated *noalias* declaration (or ``null`` if the
  declaration is not available).
* ``%p.addr``: the address of the pointer.
* ``objId``: the ID that is associated to the noalisa declaration. *SROA* treats
  this as an offset wrt to the original ``alloca``.
* ``!Scope``: a single scope that is associated with the noalias declaration.
* ``!VisibleScopes``: the scopes related to *noalias* declarations that are
  visible to location in the control flow where the noalias pointer is read from
  memory.

Semantics:
""""""""""

Adds *noalias* provenance information so that all memory instructions that
depend on ``%p.noalias`` are known to be based on a pointer with extra *noalias*
info. This is a mostly opaque intrinsic for optimizations. In order to not block
optimizations, it will be converted into a ``@llvm.provenance.noalias`` and
moved to the ``ptr_provenance`` path of memory instructions.

When a ``%p.decl`` is available, following arguments must match the ones in that
declaration: ``objId``, ``!Scope``.

When ``!Scope`` points to the *unknown function scope*, ``%p.decl`` must be
``null``.

.. note::
   ``@llvm.noalias`` can be seen as a shortcut for ``@llvm.provenance.noalias``
   and ``@llvm.noalias.arg.guard``. See
   :ref:`@llvm.noalias vs @llvm.provenance.noalias<noalias_vs_provenance_noalias>`.

Examples:
=========
See :ref:`Example A: local restrict<noaliasinfo_local_restrict_llvm_0>` and
:ref:`Example B: pass a restrict pointer<noaliasinfo_pass_restrict_llvm_0>`.



``@llvm.provenance.noalias`` Intrinsic
--------------------------------------

Syntax:
"""""""

.. code-block:: llvm

    %prov.p =
        T* call @llvm.provenance.noalias
            T* %p, i8* %p.decl,
            T** %p.addr, T** %prov.p.addr, i64 objId, metadata !Scope,
            !noalias !VisibleScopes``

Overview:
"""""""""

Adds *noalias* provenance information to a pointer. This version, which is
similar to ``@llvm.noalias``, must only be found on the ``ptr_provenance`` path.

Arguments:
""""""""""

* ``%p``: the original value of the pointer, or a depending
  ``@llvm.provenance.noalias``.
* ``%p.decl``: the associated *noalias* declaration (or ``null`` if the
  declaration is not available).
* ``%p.addr``: the address of the pointer.
* ``%prov.p.addr``: the ``ptr_provenance`` associated to ``%p.addr``. If this is
  ``Undef``, the original ``%p.addr`` must be followed.
* ``objId``: the ID that is associated to the noalisa declaration. *SROA* treats
  this as an offset wrt to the original ``alloca``.
* ``!Scope``: a single scope that is associated with the noalias declaration.
* ``!VisibleScopes``: the scopes related to *noalias* declarations that are
  visible to location in the control flow where the noalias pointer is read from
  memory.

Semantics:
""""""""""

Adds *noalias* provenance information to a pointer. This is similar to
``@llvm.noalias``, but this version must only be found on the ``ptr_provenance``
path of memory instructions or of the ``@llvm.noalias.arg.guard`` intrinsic.

It can also be found on the path of the ``%prov.p.addr`` and on the ``%p``
arguments of another ``@llvm.provenance.noalias`` intrinsic.

When a ``%p.decl`` is available, following arguments must match the ones in that
declaration: ``objId``, ``!Scope``.

When ``!Scope`` points to the *unknown function scope*, ``%p.decl`` must be
``null``.

Examples:
=========
See :ref:`Example A: local restrict<noaliasinfo_local_restrict_llvm_1>` and
:ref:`Example B: pass a restrict pointer<noaliasinfo_pass_restrict_llvm_1>`.


``@llvm.noalias.arg.guard`` Intrinsic
-------------------------------------

Syntax:
"""""""

.. code-block:: llvm

    %p.guard =
        T* call @llvm.noalias.arg.guard
            T* %p, T* %prov.p

Overview:
"""""""""

Combines the value of a pointer with its *noalias* provenance information.

Arguments:
""""""""""

* ``%p``: the value of the pointer
* ``%prov.p``: the provenance information associated to ``%p``


Semantics:
""""""""""

Combines the value of a pointer with its *noalias* provenance information. This
is normally introduced when converting ``@llvm.noalias`` into
``@llvm.provenance.noalias`` and the pointer is passed as a function
argument, returned from a function or stored to memory. This intrinsic ensures
that at a later time (after inlining and/or other optimizations), the provenance
information can be propagated to the memory instructions depending on the guard.

Examples:
=========
See :ref:`Example B: pass a restrict pointer<noaliasinfo_pass_restrict_llvm_1>`.


``@llvm.noalias.copy.guard`` Intrinsic
--------------------------------------

Syntax:
"""""""

.. code-block:: llvm

    %p.addr.guard =
        T* call @llvm.noalias.copy.guard
            T* %p.addr, i8* %p.decl,
            metadata !Indices,
            metadata !Scope,
            !noalias !VisibleScopes

Overview:
"""""""""

Annotates that the memory block pointed to by ``%p.addr`` contains *noalias
annotated pointers* (restrict pointers).

Arguments:
""""""""""

* ``%p.addr``: points to the block of memory that will be copied
* ``%p.decl``: the associated *noalias* declaration (or ``null`` if the
  declaration is not available).
* ``!Indices``: the set of indices, describing on what locations a *noalias*
  pointer can be found.
* ``!Scope``: a single scope that is associated with the noalias declaration.
* ``!VisibleScopes``: the scopes related to *noalias* declarations that are
  visible to location in the control flow where the noalias pointer is read from
  memory.

Semantics:
""""""""""

Annotates that the memory block pointed to by ``%p.addr`` contains *noalias
annotated pointers* (restrict pointers). The ``!Indices`` indicate where in
memory the *noalias* pointers are located.

When a block copy (aggregate load/store or ``@llvm.memcpy``) uses
``%p.addr.guard`` as a source, *SROA* is able to reconstruct the implied
``@llvm.noalias`` intrinsics. This ensure that the *noalias* information for
those pointers is tracked.

When a ``%p.decl`` is available, the ``!Scope`` argument must match the one in
that declaration.

When ``!Scope`` points to the *unknown function scope*, ``%p.decl`` must be
``null``.


``!Indices`` points to a list of metadata. Each entry in that list contains a
set of ``i32`` values, corresponding to the indices that would be past to
``getelementptr`` to retrieve a field in the struct. When the ``i32`` value is
**-1**, it indicates that any possible value should be checked (0, 1, 2, ...),
as long as the resulting address fits the size of the memory copy.

Examples:
"""""""""

Code example with a ``llvm.noalias.copy.guard``:

* Note the **-1** to represent ``a[i]`` in the indices of ``!15``.
* After optimization, the ``alloca`` is gone. The ``llvm.memcpy`` is also gone,
  but the remaining dependency on restrict pointers is kept in the
  ``llvm.noalias.provenance``. Two are needed for this example: one related to
  the declaration of ``struct B tmp``. One related to the ``unknown function
  scope``.

.. code-block:: C

    struct B {
      int * restrict p;
      struct A {
        int m;
        int * restrict p;
      } a[5];
    };


    void FOO(struct B* b) {
      struct B tmp = *b;

      *tmp.a[1].p=32;
    }

Results in following code:

.. code-block:: llvm

    %struct.B = type { i32*, [5 x %struct.A] }
    %struct.A = type { i32, i32* }

    ; Function Attrs: nounwind
    define dso_local void @FOO(%struct.B* %b) #0 !noalias !3 {
    entry:
      %b.addr = alloca %struct.B*, align 4
      %tmp = alloca %struct.B, align 4
      store %struct.B* %b, %struct.B** %b.addr, align 4, !tbaa !6, !noalias !10
      %0 = bitcast %struct.B* %tmp to i8*
      call void @llvm.lifetime.start.p0i8(i64 44, i8* %0) #5, !noalias !10
      %1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.Bs.i64(%struct.B* %tmp, i64 0, metadata !12),
                                !noalias !10
      %2 = load %struct.B*, %struct.B** %b.addr, align 4, !tbaa !6, !noalias !10
      %3 = call %struct.B* @llvm.noalias.copy.guard.p0s_struct.Bs.p0i8(%struct.B* %2,
                                i8* null, metadata !13, metadata !3)
      %4 = bitcast %struct.B* %tmp to i8*
      %5 = bitcast %struct.B* %3 to i8*
      call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %4, i8* align 4 %5, i32 44, i1 false),
                                !tbaa.struct !16, !noalias !10
      %a = getelementptr inbounds %struct.B, %struct.B* %tmp, i32 0, i32 1
      %arrayidx = getelementptr inbounds [5 x %struct.A], [5 x %struct.A]* %a, i32 0, i32 1
      %p = getelementptr inbounds %struct.A, %struct.A* %arrayidx, i32 0, i32 1
      %6 = load i32*, i32** %p, align 4, !tbaa !18, !noalias !10
      %7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %6,
                                i8* %1, i32** %p, i64 0, metadata !12), !tbaa !18, !noalias !10
      store i32 32, i32* %7, align 4, !tbaa !21, !noalias !10
      %8 = bitcast %struct.B* %tmp to i8*
      call void @llvm.lifetime.end.p0i8(i64 44, i8* %8) #5
      ret void
    }

    ...

    !3 = !{!4}
    !4 = distinct !{!4, !5, !"FOO: unknown scope"}
    !5 = distinct !{!5, !"FOO"}
    !10 = !{!11, !4}
    !11 = distinct !{!11, !5, !"FOO: tmp"}
    !12 = !{!11}
    !13 = !{!14, !15}
    !14 = !{i32 -1, i32 0}
    !15 = !{i32 -1, i32 1, i32 -1, i32 1}

And after optimizations:

.. code-block:: llvm

    ; Function Attrs: nounwind
    define dso_local void @FOO(%struct.B* nocapture %b) local_unnamed_addr #0 !noalias !3 {
    entry:
      %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 16, metadata !6)
      %tmp.sroa.69.0..sroa_idx10 = getelementptr inbounds %struct.B, %struct.B* %b, i32 0, i32 1, i32 1, i32 1
      %tmp.sroa.69.0.copyload = load i32*, i32** %tmp.sroa.69.0..sroa_idx10, align 4, !tbaa.struct !8, !noalias !14
      %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %tmp.sroa.69.0.copyload,
                                i8* null, i32** nonnull %tmp.sroa.69.0..sroa_idx10, i32** undef, i64 0, metadata !3)
      %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %1,
                                i8* %0, i32** null, i32** undef, i64 16, metadata !6), !tbaa !15, !noalias !14
      store i32 32, i32* %tmp.sroa.69.0.copyload, ptr_provenance i32* %2, align 4, !tbaa !18, !noalias !14
      ret void
    }

    ...

    !3 = !{!4}
    !4 = distinct !{!4, !5, !"FOO: unknown scope"}
    !5 = distinct !{!5, !"FOO"}
    !6 = !{!7}
    !7 = distinct !{!7, !5, !"FOO: tmp"}


Other usages of ``noalias`` inside LLVM
=======================================


``noalias`` attribute on parameters or function
-----------------------------------------------

This indicates that memory locations accessed via pointer values
:ref:`based <pointeraliasing>` on the argument or return value are not also
accessed, during the execution of the function, via pointer values not
*based* on the argument or return value.

See :ref:`noalias attribute<noalias>`

``noalias`` and ``alias.scope`` Metadata
----------------------------------------

``noalias`` and ``alias.scope`` metadata provide the ability to specify generic
noalias memory-access sets.

See :ref:`noalias and alias.scope Metadata <noalias_and_aliasscope>`

The usage of this construct is not recommended, as it can result in wrong code
when inlining and loop unrolling optimizations are applied.


References
==========

.. rubric:: References

.. [#R1] https://en.wikipedia.org/wiki/Restrict
.. [#R2] WG14 N1256: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf (Chapter 6.7.3.1 Formal definition of restrict)
.. [#R3] WG21 N4150: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4150.pdf
.. [#R4] https://reviews.llvm.org/D9375   Hal Finkel's local restrict patches
.. [#R5] https://bugs.llvm.org/show_bug.cgi?id=39240 "clang/llvm looses restrictness, resulting in wrong code"
.. [#R6] https://bugs.llvm.org/show_bug.cgi?id=39282 "Loop unrolling incorrectly duplicates noalias metadata"
.. [#R7] https://www.godbolt.org/z/cUk6To "testcase showing that LLVM-IR is not able to differentiate if restrict is done inside or outside the loop"
.. [#R8] DR294: http://www.open-std.org/jtc1/sc22/wg14/www/docs/dr_294.htm
.. [#R9] WG14 N2250: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2260.pdf  Clarifying the restrict Keyword v2
.. [#R10] RFC: Full 'restrict' support in LLVM https://lists.llvm.org/pipermail/llvm-dev/2019-October/135672.html
