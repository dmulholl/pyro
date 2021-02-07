##
# This module makes the package directly executable. To run a package located
# on Python's import search path use:
#
#   $ python -m pyro
#
# To run an arbitrary `pyro` package use:
#
#   $ python /path/to/pyro/package
#
# Note that we need to manually add the package's parent directory to the
# search path to make the import work.
##

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
import pyro
sys.path.pop(0)

pyro.main()

