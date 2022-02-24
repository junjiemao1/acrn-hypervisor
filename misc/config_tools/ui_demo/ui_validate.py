#!/usr/bin/env python3

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../scenario_config"))

import json

from pipeline import PipelineObject, PipelineEngine
from xml_loader import XMLLoadStage
from default_populator import DefaultValuePopulatingStage
from validator import ValidatorConstructionByFileStage, SemanticValidationStage

pipeline = PipelineEngine(["board_path", "scenario_path", "schema_path", "datachecks_path"])
pipeline.add_stages([
    ValidatorConstructionByFileStage(),
    XMLLoadStage("schema"),

    XMLLoadStage("board"),
    XMLLoadStage("scenario"),
    DefaultValuePopulatingStage(),
    SemanticValidationStage(),
])

obj = PipelineObject(board_path = "tgl-vecow-spc-7100-Corei7.xml", scenario_path = "shared.xml", schema_path = "sliced.xsd", datachecks_path = "allchecks.xsd")
pipeline.run(obj)
print(json.dumps(obj.get("semantic_errors")))
for e in obj.get("semantic_errors"):
    print(str(e))
