redcurrant-librain
==================

jerasure wrapper for Redcurrant's erasure code.

Considering your installation with
  * a working directory whose name is stored in TMPDIR
  * the source directory whose name stored in SRCDIR
  * Jerasure headers installed in a path stored in JINC
  * Jerasure libraries insatlled in a path stored in JLIB

    cd $TMPDIR
    cmake -DJERASURE_INCDIR=$JINC -DJERASURE_LIBDIR=$JLIB -DPREFIX=... $SRCDIR
    make
    make install

