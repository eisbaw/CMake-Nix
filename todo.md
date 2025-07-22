Fix this: The Nix generator doesn't support $<COMPILE_LANGUAGE:...> expressions yet

Look for other unsupported features in Nix generator.

Look for code smells in the cmake Nix generator.

Look for asusmptions in the cmake Nix generator.

Improve the cmake Nix generator.

Search the web for cmake C/C++ open-source projects. Add a medium-sized popular cmake-based project as a new test case. Use cmake nix generator backend to build it.

Self-host cmake: adapt bootstrap so we can build cmake with Nix generator.
