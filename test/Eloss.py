#!/usr/bin/env python3
import numpy as np
import matplotlib.pyplot as plt
import sys, h5py, os
from event import probe_run, rate_run

dt = 0.025
Nsteps = 200
Np = 200000
L = np.arange(Nsteps)*dt
E = np.array([5, 16, 100])
f = h5py.File("DiffBoxRate.h5", 'a')
name = 'diff-rate-rad-8GeV'
if name in f:
	del f[name]
gp = f.create_group(name)
gp.attrs.create('T', [0.2, 0.4])
gp.attrs.create('E', E)
gp.attrs.create('L', L)
for iT, T in enumerate([0.2, 0.4]):
	for iE, E0 in enumerate(E):
		mode = 'old' if os.path.exists('table.h5') else 'new'
		R = rate_run(E0, T, dt=dt, Nsteps=Nsteps, Nparticles=Np, mode=mode)
		R = np.array(R)
		R = np.sum(R, axis=1)
		gp.create_dataset("{}/{}".format(iT, iE), data=R)
f.close()
