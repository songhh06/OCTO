# Asset manifest

| Path | Origin | Author | Rights | Reproduction |
|---|---|---|---|---|
| game/scenes/bootstrap.tscn | Procedural plane, light, material | Project contribution | Project license pending | Godot built-in PlaneMesh; no external assets |

No third-party artwork or sound is included. Blender is not required for this empty scene; its toolchain lock remains pending.

| Path | Origin | Author | Rights | Reproduction |
|---|---|---|---|---|
| game/scripts/octopus_preview.gd | Procedural meshes, chromatophore marks, materials and visual motion | Project contribution | Project license pending; no third-party assets | run_preview.cmd |
| docs/preview/*.png | Actual Godot 4.6.2 viewport captures of the procedural scene | Project contribution | Same project assets; no external imagery | tools/preview.ps1 -Test |

Preview motion and shape are artistic approximations, not measured biological data.

| Path | Origin | Author | Rights | Reproduction |
|---|---|---|---|---|
| game/scripts/physics_lab.gd, game/scenes/physics_lab.tscn | Native trace viewer using procedural preview meshes | Project contribution | Project license pending | run_physics_lab.cmd |
| game/data/physics_trace.json | Actual successful C++ dynamics snapshots | Project contribution | Project license pending; ASSUMED model | tools/physics-lab.ps1 -BuildOnly |
| docs/preview/native_physics_lab.png | Actual Godot viewport of native trace playback | Project contribution | Same project assets | tools/physics-lab.ps1 -Capture |

| docs/progress/physics/V07-eccentric-peeling.png, V08-material-comparison.png | Native test CSV visualized by tools/plot_p2_results.py | Project contribution | Project license pending; no third-party imagery | Run Release P2.V07/P2.V08 then Python plot script |

| game/scripts/physics_demo.gd, game/scenes/physics_demo.tscn | Authored native-record viewer and procedural UI | Project contribution | Project license pending | run_p2_demo.cmd |
| game/data/demo/*.json | Native C++ traces/stats/manifest | Project contribution | Same project; no external assets | tools/build-p2-demo.ps1 |
| docs/preview/p2_demo_*.png | Actual Godot viewport captures | Project contribution | Same project | physics_demo.tscn -- --demo-test |

| game/scripts/live_demo.gd, game/scenes/live_demo.tscn | Authored live native-state renderer/UI | Project contribution | Project license pending | run_live_demo.cmd |
| docs/preview/live_box_contact.png | Actual Godot 4.6.2 live contact viewport | Project contribution | Same procedural assets | live_demo.tscn -- --live-test |
