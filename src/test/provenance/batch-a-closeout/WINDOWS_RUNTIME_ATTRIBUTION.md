# Microsoft Static-Runtime Attribution

- Source identity: the Microsoft C/C++ static runtime selected by the Windows build toolchain.
- Applies to: Microsoft runtime portions statically linked into the Windows native DLL and the Windows
  wheel's native DLL; corresponding publication destinations are D05, D09, and D12-D15.
- Build policy: Release uses static `/MT`; debug `/MTd` artifacts are not publication inputs.
- Evidence: `toolbox/windows_toolchain_evidence.py` has capture paths for both Windows producers and
  records the actual linker, expanded response files, selected libraries, archive paths/hashes/members,
  imports, runner image, and tool versions. Approved evidence and standing contracts remain pending the
  branch captures.
- Terms capture: `terms_text_captured=false`; the Visual Studio Enterprise 2026 terms text and exact
  selected runtime identity were unrecovered from the earlier builds.
- Notice boundary: no application-local notice-reproduction condition was inferred. This statement is
  not a substantive compatibility conclusion.
- Disposition: retained under owner risk acceptance, subject to the applicable Microsoft terms and
  outside the project MIT grant. Permission is not claimed.

This project-authored record identifies applicability and known evidence boundaries. It does not copy
or invent Microsoft licence text.
