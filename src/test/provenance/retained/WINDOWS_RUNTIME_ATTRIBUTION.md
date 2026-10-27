# Microsoft Static-Runtime Attribution

- Source identity: the Microsoft C/C++ static runtime selected by the Windows build toolchain.
- Applies to: Microsoft runtime portions statically linked into the Windows native DLL and the Windows
  wheel's native DLL; corresponding publication destinations are D05, D09, and D12-D15.
- Build policy: Release uses static `/MT`; debug `/MTd` artifacts are not publication inputs.
- Evidence: `toolbox/windows_toolchain_evidence.py` has capture paths for both Windows producers and
  records the actual linker, expanded response files, selected libraries, archive paths/hashes/members,
  imports, runner image, and tool versions. Approved evidence and standing contracts are pinned for
  both producers.
- Terms capture: `terms_text_captured=true`; each approved profile records 25 Visual Studio Enterprise
  2026 terms documents with their paths and SHA-256 digests. The terms are not reproduced or interpreted
  here.
- Notice boundary: no conclusion about application-local notice or redistribution obligations is drawn
  from the captured text. This is not a substantive compatibility or legal conclusion.
- Disposition: retained under owner risk acceptance, subject to the applicable Microsoft terms and
  outside the project MIT grant. Permission is not claimed.

This project-authored record identifies applicability and known evidence boundaries. It does not copy
or interpret Microsoft licence text.
