## Symbol Parser

A small class to parse debug info from PEs, download their respective PDBs from the Microsoft Public Symbol Server 
and calculate RVAs of functions.

----

x86 builds are compatible with x64 PEs

x64 builds are compatible with x86 PEs

Keep in mind to use SysWOW64 or Sysnative (instead of System32) when trying to load a system dll crossplatform.

----

Check the main.cpp for an example.

