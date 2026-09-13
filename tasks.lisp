; tasks.my — durable task plan for wsm-os. Same alist shape as sibling
; repos' tasks.my. Note: this repo deliberately has no repo.my (per its
; own README's non-invention rule -- confirmed absent, not an oversight;
; see ecosystem/knowledge/guard-reference-inbox.mylog topic
; wsm-os-scope-and-structure). tasks.my is added now on explicit owner
; instruction, not because the earlier no-registry finding was wrong.

((tasks . (
  ("WSM-OS-MINIX-HECI-PROBE-SUPPORT" . (
    (priority . 6.5)
    (capabilities . (qemu bios heci research))
    (origin . wsm-os)
    (context . "research/minix-heci-path.md already exists here, and the live PCI-8086:a13a discovery step (MINIX-1) is tracked in minix/tasks.my as the primary owner-driven step. wsm-os's own role per its stated rule is to verify capabilities minix/wsm actually ask for, not invent scope ahead of them.")
    (description . "Keep the QEMU/OVMF harness (scripts/qemu-uefi-selftest.sh, docs/QEMU-SETUP.md) ready to reproduce whatever MINIX-1 needs verified on real vs emulated hardware, once that work defines a concrete need -- do not build speculative HECI transport code here ahead of that need, per the repo's own explicit rule.")
    (done . ())
  ))

  ("WSM-OS-BLOCK-MIRROR-NOTE" . (
    (priority . 4.0)
    (capabilities . (documentation filesystem))
    (origin . ecosystem)
    (context . "Found 2026-09-04 (WSM-FS-CROSS-REPO-HARMONIZATION, ecosystem session): the FS harmonization plan names 'wsm-os-block' (opaque bounded bytes owner) -- that crate actually lives in wsm-os-lisp (crates/wsm-os-block/), NOT here, despite this repo's name being the closer match. Mirror of WSM-OS-BLOCK-OWNERSHIP-RESOLUTION, recorded in wsm-os-lisp's own tasks.my.")
    (description . "If the owner decides wsm-os-block should migrate here (breaking wsm-os-lisp non-continuation for this one crate), this is where it would land -- this entry exists so that decision, once made, has an obvious landing task on this side too. Do not migrate anything speculatively.")
    (done . ())))
)))
