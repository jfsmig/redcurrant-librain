# redcurrant-librain

jerasure wrapper for Redcurrant's erasure code.

## Dependencies

* http://lab.jerasure.org/jerasure/gf-complete
* http://lab.jerasure.org/jerasure/jerasure

## Installation

Considering your installation with
* a working directory whose name is stored in TMPDIR
* the source directory whose name stored in SRCDIR
* Jerasure headers installed in a path stored in JINC
* Jerasure libraries insatlled in a path stored in JLIB

  ```shell
  cd $TMPDIR && \
  cmake -DJERASURE_INCDIR=$JINC -DJERASURE_LIBDIR=$JLIB -DPREFIX=... $SRCDIR && \
  make && \
  make install
  ```
