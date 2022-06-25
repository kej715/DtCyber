--------------------------------------------------------------------------

   Windows Build Notes :: 2022, Steven Zoppi

   Description:
   
       Build software engineering tools used in maintaining the
       Desktop Cyber source code repository.

--------------------------------------------------------------------------


2022-06-17 SJZ: Windows builds rely on a unix utility called "Uncrustify" which is available
                through loading of the MSYS2/CLANG toolchain.  That's a LOT of stuff to load
                for just this utility so pre-compiled, 64-bit versions (the most common 
                platform for Windows) of the following are included in this directory
                and the precompiled binaries are stored in subdirectory "wintools":
                
                    pp.exe
                    uncrustify.exe
                    libc++.dll
                    libunwind.dll
                
                The additional tools have been added to make reformatting convenient:
                
                _SendToAttic.vbs  (performs a date-stamped backup of source files before
                                   they are altered - better safe than sorry)
                
                style.cmd         (cmd file which applies the coding styles to the 
                                   .c or .h files.  All other files are skipped.)
                
                Do NOT change the name of the directory "wintools" unless you make the
                corresponding change to "style.cmd".
                