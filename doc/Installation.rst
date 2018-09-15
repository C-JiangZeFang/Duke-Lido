Installation
=================

Download the source from `here`_.

.. _here: https://github.com/keweiyao/Duke-Lido

Requirements: c++11, libraries: gsl, hdf5, and boost

  
Build c++ main binary
---------------------

To compile and install exe:

.. code-block:: bash

  mkdir build && cd build
  cmake ..
  make
  make install

After this, you can also find an example under build/ or you/install/path/

.. code-block:: bash

  cp path/to/settings.xml ./
  ./example1 new


It should generate elastic scattering table and then calculate an energy loss for you.


Build python (Cpython) library
------------------------------

To setup python interface, have Cython installed and then:

To build stand-alone library

.. code-block:: bash

  python3 setup.py build_ext --inplace
  
or,
 
.. code-block:: bash

  python3 setup --build_ext -b <local folder>


To install into system path

.. code-block:: bash

  python3 setup.py install --prefix=<path>
  
