"""Plots native acceptance CSV output; does not synthesize simulation data."""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[1]/"docs/progress/physics"
fig,axes=plt.subplots(2,2,figsize=(10,7),constrained_layout=True)
for label,name in [("Glass (ASSUMED)","V08-glass.csv"),("Porous (ASSUMED)","V08-porous.csv")]:
    a=np.genfromtxt(root/name,delimiter=",",names=True)
    for ax,key,title,unit in [(axes[0,0],"pressure_Pa","Chamber pressure","Pa"),(axes[0,1],"x_m","Integrated box sliding","m"),(axes[1,0],"force_N","Suction capacity","N"),(axes[1,1],"friction_N","Cup tangential reaction","N")]:
        ax.plot(a["time_s"],a[key],label=label);ax.set(xlabel="Time after load (s)",ylabel=unit,title=title);ax.grid(alpha=.3)
axes[0,0].legend();fig.suptitle("V08: native coupled solver, equal 10 N horizontal load")
fig.savefig(root/"V08-material-comparison.png",dpi=150);plt.close(fig)
a=np.genfromtxt(root/"V07-peel.csv",delimiter=",",names=True)
fig,axes=plt.subplots(1,2,figsize=(10,3.7),constrained_layout=True)
axes[0].plot(a["time_s"],a["force_N"]);axes[0].set(title="Monotone suction loss",ylabel="Capacity (N)")
for key in ["damage_0","damage_6"]:axes[1].plot(a["time_s"],a[key],label=key)
axes[1].set(title="Ordered opposite-rim damage",ylabel="Damage fraction");axes[1].legend()
for ax in axes:ax.set_xlabel("Time after eccentric load (s)");ax.grid(alpha=.3)
fig.suptitle("V07: geometric 12-rim peeling (ASSUMED material)")
fig.savefig(root/"V07-eccentric-peeling.png",dpi=150)
