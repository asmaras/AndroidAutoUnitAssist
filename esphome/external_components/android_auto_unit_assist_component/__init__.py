import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

android_auto_unit_assist_component_ns = cg.esphome_ns.namespace('android_auto_unit_assist_component')
AndroidAutoUnitAssistComponent = android_auto_unit_assist_component_ns.class_('AndroidAutoUnitAssistComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AndroidAutoUnitAssistComponent)
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)