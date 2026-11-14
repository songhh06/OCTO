"""Actual bidirectional native process protocol; no recorded animation."""
import subprocess,json,sys,math
from pathlib import Path
exe=Path(sys.argv[1]).resolve()
def trial(touch):
    process=subprocess.Popen([str(exe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding="utf-8")
    def receive():
        line=process.stdout.readline()
        if not line:raise RuntimeError(process.stderr.read())
        value=json.loads(line)
        if not value["ok"]:
            Path("work/live-protocol-failure.json").write_text(json.dumps(value))
            raise RuntimeError({key:value[key] for key in ["error","contacts","arm_motion","solver_ms"]}|{"tick":value["snapshot"]["tick"]})
        return value
    initial=receive();peak=0;motion=0;pen=0;times=[]
    try:
        for tick in range(360):
            target=[1.45-1.9*min(tick/300,1),0,.8] if touch else [1.45,0,.8+.6*min(tick/120,1)]
            process.stdin.write(json.dumps({"op":"step","target":target})+"\n");process.stdin.flush();result=receive()
            assert result["snapshot"]["tick"]==tick+1
            peak=max(peak,result["contacts"]);motion=max(motion,result["arm_motion"]);pen=max(pen,result["penetration"]);times.append(result["solver_ms"])
        assert len(result["snapshot"]["arms"])==8
        assert motion>.005 and peak>0 if touch else motion<1e-5 and peak==0,(touch,motion,peak)
        assert pen<.002
        process.stdin.write('{"op":"reset"}\n');process.stdin.flush();reset=receive();assert reset["snapshot"]["tick"]==0 and reset["arm_motion"]==0
        return {"contact_trial":touch,"ticks":360,"contacts_peak":peak,"max_arm_motion_m":motion,"max_penetration_m":pen,"p50_ms":sorted(times)[len(times)//2],"p95_ms":sorted(times)[int(len(times)*.95)]}
    finally:
        if process.poll() is None:
            process.stdin.write('{"op":"quit"}\n');process.stdin.flush();process.wait(timeout=10)
# Exercise Windows wide argv with both Korean text and spaces.
folder=Path("work/실시간 진단").resolve()
probe=subprocess.run([str(exe),"--diagnostics",str(folder)],input='{"op":"quit"}\n',text=True,encoding="utf-8",capture_output=True,timeout=10)
assert probe.returncode==0,(probe.returncode,probe.stderr)
assert json.loads(probe.stdout)["protocol"]==1 and folder.is_dir()
results=[trial(False),trial(True)]
Path("docs/progress/physics").mkdir(parents=True,exist_ok=True)
Path("docs/progress/physics/live-bridge-tests.json").write_text(json.dumps(results,indent=2))
print(json.dumps(results));print("LIVE_NATIVE_CONTACT_OK")
