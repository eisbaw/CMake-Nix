DONE: Fix this: The Nix generator doesn't support $<COMPILE_LANGUAGE:...> expressions yet

DONE: Look for other unsupported features in Nix generator. (See UNSUPPORTED_FEATURES.md)

DONE: Look for code smells in the cmake Nix generator. (See CODE_REVIEW_FINDINGS.md)

DONE: Look for assumptions in the cmake Nix generator. (See CODE_REVIEW_FINDINGS.md)

Improve the cmake Nix generator.

DONE: Fix what is reported in UNSUPPORTED_FEATURES.md - OBJECT library support fixed

Fix what is reported in CODE_REVIEW_FINDINGS.md

Search the web for cmake C/C++ open-source projects. Add a medium-sized popular cmake-based project as a new test case. Use cmake nix generator backend to build it.

Self-host cmake: adapt bootstrap so we can build cmake with Nix generator.
