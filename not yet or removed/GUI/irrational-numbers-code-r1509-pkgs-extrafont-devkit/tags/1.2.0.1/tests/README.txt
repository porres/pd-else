This folder contains tests for the "extrafont" package.

package TTXN (TclTest eXtended Notation) is required.
It can be downloaded from
 http://irrational-numbers.googlecode.com/files/TTXN-1.0.1.zip

You should install TTXN under a tcl-library directory (recommended)
OR
you should place it within this "test" directory.


NOTES on test-run
=================

You can run a single test from the OS shell, as follows:

    tclsh .../tests/helper   .../tests/xxx.test

or

You can run all tests
    tclsh ..../tests/all.tcl

In order to enable all tests , append 
  "-constraints userInteraction -constraints vfs::zip"  to
the previous commands.              