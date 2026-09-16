; tasks.my — durable task plan for wsm-os. Same alist shape as sibling
; repos' tasks.my. Note: this repo deliberately has no repo.my (per its
; own README's non-invention rule -- confirmed absent, not an oversight;
; see ecosystem/knowledge/guard-reference-inbox.mylog topic
; wsm-os-scope-and-structure). tasks.my is added now on explicit owner
; instruction, not because the earlier no-registry finding was wrong.

((tasks . (
  ("WSM-OS-CANON0-OBSERVATION-SINK" . (
    (priority . 10.0)
    (capabilities . (probe observation provenance canon-zero execution-boundary hardware))
    (origin . wsm-os)
    (context . "Owner decision, 2026-09-16, formulated in wsm as CANON-0-ACCUMULATOR-SEMANTICS: () is intentionally the accumulation sink for outcomes that did not become a stronger justified answer. wsm-os must preserve what the machine actually exposed without inventing a cause for non-result.")
    (description . "Prepare the external-witness side of Canon-0 accumulation. When wsm asks for a concrete operation/result, distinguish at least: specialized result observed; specialized result not observed; and any independently observed execution events around that absence (timeout, reset, watchdog, OOM indication, disconnect, silence, trap, etc.) without automatically equating any event with the cause. A missing specialized result may legitimately feed () even when the underlying mathematical/domain answer is determinate. Raw evidence must retain provenance: channel, STATIC/LIVE class, VIRTUAL/PHYSICAL environment where applicable, and the exact observation. Do not replace () with an invented operational enum, and do not claim 'overload', 'OOM', 'deadlock', or another cause unless the probe actually witnessed evidence sufficient for that claim. Cause-of-non-result is a separate query and may itself resolve to (). Build no scheduler/runtime/driver framework ahead of an explicit wsm capability request." )
    (done . ())))

  ("WSM-OS-CANON0-ACCUMULATION-WITNESS" . (
    (priority . 9.8)
    (capabilities . (probe serial witness qemu physical-machine provenance))
    (origin . wsm-os)
    (context . "The repository already has the external-observer discipline around ExitBootServices/RAW_CONTROL_REACHED. Extend that discipline only when wsm supplies an executable Canon-0 accumulation contract.")
    (description . "Design a bounded witness showing that several physically different histories can legitimately converge to the same Canon-0 outcome while remaining distinguishable in evidence. Candidate histories may include an observed timeout, an observed reset, no response on the observation channel, and a normal specialized result path. The witness must prove only what was observed; it must not infer common cause from common (). Keep () as the semantic sink owned by wsm, while wsm-os owns the external evidence trail. Run in QEMU first and on physical hardware only where the existing safety/boot procedure already authorizes it." )
    (done . ())))

  ("WSM-OS-MINIX-HECI-PROBE-SUPPORT" . (
    (priority . 6.5)
    (capabilities . (qemu bios heci research))
    (origin . wsm-os)
    (context . "research/minix-heci-path.md already exists here, and the live PCI-8086:a13a discovery step (MINIX-1) is tracked in minix/tasks.my as the primary owner-driven step. wsm-os's own role per its stated rule is to verify capabilities minix/wsm actually ask for, not invent scope ahead of them.")
    (description . "Keep the QEMU/OVMF harness (scripts/qemu-uefi-selftest.sh, docs/QEMU-SETUP.md) ready to reproduce whatever MINIX-1 needs verified on real vs emulated hardware, once that work defines a concrete need -- do not build speculative HECI transport code here ahead of that need, per the repo's own explicit rule.")
    (done . ())))

  ("WSM-OS-BLOCK-MIRROR-NOTE" . (
    (priority . 4.0)
    (capabilities . (documentation filesystem))
    (origin . ecosystem)
    (context . "Found 2026-09-04 (WSM-FS-CROSS-REPO-HARMONIZATION, ecosystem session): the FS harmonization plan names 'wsm-os-block' (opaque bounded bytes owner) -- that crate actually lives in wsm-os-lisp (crates/wsm-os-block/), NOT here, despite this repo's name being the closer match. Mirror of WSM-OS-BLOCK-OWNERSHIP-RESOLUTION, recorded in wsm-os-lisp's own tasks.my.")
    (description . "If the owner decides wsm-os-block should migrate here (breaking wsm-os-lisp non-continuation for this one crate), this is where it would land -- this entry exists so that decision, once made, has an obvious landing task on this side too. Do not migrate anything speculatively.")
    (done . ())))
)))
