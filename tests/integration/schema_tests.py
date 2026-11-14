import copy
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
from validate_data import validate, load, ROOT

class Schemas(unittest.TestCase):
    def setUp(self):
        self.scene = load(ROOT/"tests/scenes/P0_empty.json")
        self.replay = load(ROOT/"replays/golden/P0_empty.json")
        self.body = {"id":1,"mass":1,"inertia_body":[[1,0,0],[0,1,0],[0,0,1]],
                     "pose":{"position":[0,0,0],"rotation_xyzw":[0,0,0,1]},
                     "linear_velocity":[0,0,0],"angular_velocity":[0,0,0],
                     "shape":{"type":"box","half_extents":[1,1,1]},"material_id":0}
    def test_valid(self):
        validate("scene",self.scene)
        validate("replay",self.replay)
        self.scene["dynamic_bodies"]=[self.body]
        validate("scene",self.scene)
    def test_bad_scene(self):
        for key,value in [("schema_version",2),("unit_system","cm"),("gravity",[0,0]),("water",{})]:
            with self.subTest(key=key):
                data=copy.deepcopy(self.scene); data[key]=value
                with self.assertRaises(Exception): validate("scene",data)
    def test_bad_bodies(self):
        variants=[("mass",-1),("mass",0),("mass",float("inf")),("material_id",99),
                  ("inertia_body",[[1,2,0],[2,1,0],[0,0,1]]),
                  ("inertia_body",[[1,1,0],[0,1,0],[0,0,1]]),
                  ("pose",{"position":[0,0,0],"rotation_xyzw":[0,0,0,2]})]
        for key,value in variants:
            with self.subTest(key=key,value=value):
                data=copy.deepcopy(self.scene); body=copy.deepcopy(self.body); body[key]=value
                data["dynamic_bodies"]=[body]
                with self.assertRaises(Exception): validate("scene",data)
        self.scene["dynamic_bodies"]=[self.body,self.body]
        with self.assertRaises(ValueError): validate("scene",self.scene)
    def test_bad_replay(self):
        for key,value in [("schema_version",2),("fixed_dt",1/60),("scene_sha256","bad"),
                          ("seed",-1),("snapshot_version",2)]:
            data=copy.deepcopy(self.replay); data[key]=value
            with self.assertRaises(Exception): validate("replay",data)
        intent={"tick":1,"selected":0,"target_world":[0,0,0],"reach":0,"twist":0,"suction":False,"jet":0}
        self.replay["intents"]=[intent,intent]
        with self.assertRaises(ValueError): validate("replay",self.replay)
    def test_json_rejects_ambiguous_values(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/"bad.json"
            for text in ['{"x":NaN}','{"x":1,"x":2}']:
                path.write_text(text)
                with self.assertRaises(ValueError): load(path)

unittest.main()
