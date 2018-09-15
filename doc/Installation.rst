Installation
=================

Download the source code from https://github.com/keweiyao/Scattering.git

.. code-block:: bash
  
  git clone https://github.com/keweiyao/Scattering.git
  
Build c++ main binary
---------------------

.. code-block:: bash

  mkdir build && cd build
  cmake ..
  make

Build python (Cpython) library
------------------------------

To build stand-alone library

.. code-block:: bash

  python3 setup.py build_ext --inplace

To install into system path

.. code-block:: bash

  python3 setup.py install --prefix=<path>
