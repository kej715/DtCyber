# Tapes
The tape images in this directory are large binary files managed by Git LFS, so you
must have **git-lfs** installed on your system in order to clone and check them out
properly. For example, after cloning the *DtCyber* repository, if the *.bz2* files
appear as small (e.g., 133 bytes) text files, you will need to run **git lfs pull**
or **git lfs checkout** to pull the actual binary content from GitHub.

After obtaining the full *.bz2* files, you will need to use a tool such as
**bunzip2** to expand them into their original *.tap* forms before *DtCyber* will
recognize them as proper tape images. The automated installer in the parent
directory, `start.js`, will expand them automatically too.

Note that these tape images are made available by permission of BT Federal under
provisions of the
[BT Federal Permit](https://codex.retro1.org/cdc:b_t_permit).
BT Federal has given permission to hobbyists, researchers and museums to freely use
and share CDC material. The only restriction is that this material cannot be used
commercially. Previous time limits no longer apply.
